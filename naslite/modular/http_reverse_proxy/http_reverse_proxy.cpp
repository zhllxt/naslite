#include "http_reverse_proxy.h"

#include "../../main/app.hpp"

#include <asio3/tcp/connect.hpp>
#include <asio3/http/relay.hpp>
#include <asio3/core/defer.hpp>

namespace nas
{
	using safety = http_reverse_proxy::safety;
	using node = http_reverse_proxy::node;

	template<typename T>
	concept is_https_server = requires(T & a)
	{
		a->ssl_context;
	};

	void init_server(std::shared_ptr<node>& p, auto& server)
	{
		net::ignore_unused(p, server);
	}

	net::awaitable<void> watchdog(std::chrono::steady_clock::time_point& deadline)
	{
		asio::steady_timer timer(co_await net::this_coro::executor);

		auto now = std::chrono::steady_clock::now();
		while (deadline > now)
		{
			timer.expires_at(deadline);
			co_await timer.async_wait(net::use_nothrow_awaitable);
			now = std::chrono::steady_clock::now();
		}
	}

	net::awaitable<void> tcp_transfer(
		auto& server, auto& from, auto& to, proxy_site_info& site, net::tcp_socket& backend,
		std::chrono::steady_clock::time_point& deadline, std::shared_ptr<safety>& safety_ptr)
	{
		net::error_code ec{};
		std::array<char, net::tcp_frame_size> data;

		for (;;)
		{
			deadline = (std::max)(deadline, std::chrono::steady_clock::now() + std::chrono::minutes(10));

			safety_ptr->deadline = std::max(
				safety_ptr->deadline, std::chrono::steady_clock::now() + std::chrono::minutes(10));

			auto [e1, n1] = co_await from.async_read_some(net::buffer(data), net::use_nothrow_awaitable);
			if (e1)
			{
				//app.logger->debug("tcp_transfer::read  failed: {} {}", site.domain, e1.message());
				break;
			}

			auto [e2, n2] = co_await net::async_write(to, net::buffer(data, n1), net::use_nothrow_awaitable);
			if (e2 || server->is_aborted())
			{
				//app.logger->debug("tcp_transfer::write failed: {} {}", site.domain, e2.message());
				break;
			}
		}

		from.lowest_layer().shutdown(net::socket_base::shutdown_both, ec);
		to.lowest_layer().shutdown(net::socket_base::shutdown_both, ec);
		from.lowest_layer().close(ec);
		to.lowest_layer().close(ec);
	}

	net::awaitable<void> do_transfer(
		std::shared_ptr<node>& p, auto& server, auto& client, auto& backend,
		std::shared_ptr<safety>& safety_ptr, proxy_site_info& site,
		auto& client_endp, auto& client_ip, auto client_port)
	{
		std::chrono::steady_clock::time_point client_to_server_deadline{};
		std::chrono::steady_clock::time_point server_to_client_deadline{};

		co_await
		(
			(
				tcp_transfer(server, client, backend, site, backend, client_to_server_deadline, safety_ptr) ||
				watchdog(client_to_server_deadline)
			)
			&&
			(
				tcp_transfer(server, backend, client, site, backend, server_to_client_deadline, safety_ptr) ||
				watchdog(server_to_client_deadline)
			)
		);

		app.logger->debug("coroutine returned: {}:{} {} {}:{}",
			client_ip, client_port, site.host, site.port, site.domain);
	}

	void set_proxy_headers(auto& req, proxy_site_info& site)
	{
		for (auto& [key, value] : site.proxy_set_header)
		{
			req.set(key, value);
		}
	}

	bool check_auth(
		std::shared_ptr<node>& p, std::shared_ptr<safety>& safety_ptr,
		proxy_site_info& site, auto& req, auto& rep, auto& client_ip, auto client_port)
	{
		if (!site.requires_auth || site.auth_roles.empty())
			return true;

		for (auto& role : site.auth_roles)
		{
			if (role.target.empty())
				continue;

			if (role.method != req.method())
				continue;

			if (role.target == "/")
			{
				if (req.target() != "/")
					continue;
			}
			else
			{
				if (!req.target().starts_with(role.target))
					continue;
			}

			if (rep.result_int() == role.result)
			{
				safety_ptr->has_authed = true;
				safety_ptr->auth_failed_times = 0;

				app.logger->debug("http_reverse_proxy: authed success: {}:{} {} {} {}",
					client_ip, client_port, site.domain, req.method_string(), req.target());

				return true;
			}
			else
			{
				app.logger->error("http_reverse_proxy: authed failure: {}:{} {} {} {}",
					client_ip, client_port, site.domain, req.method_string(), req.target());

				safety_ptr->auth_failed_times++;

				if (safety_ptr->auth_failed_times > 3)
				{
					safety_ptr->deadline = std::max(safety_ptr->deadline,
						std::chrono::steady_clock::now() + std::chrono::minutes(p->cfg.ip_blacklist_minutes));

					app.logger->critical("http_reverse_proxy: authed failed too much: {}:{} {} {}",
						client_ip, client_port, site.domain, req.target());

					for (auto& [k, v] : safety_ptr->conns)
					{
						std::visit([](auto& p)
							{
								net::error_code ec{};
								p->lowest_layer().shutdown(net::socket_base::shutdown_both, ec);
								p->lowest_layer().close(ec);
							}, v);
					}
				}
				else
				{
					safety_ptr->deadline = std::max(safety_ptr->deadline, std::chrono::steady_clock::now() +
						std::chrono::minutes(10 * safety_ptr->auth_failed_times));
				}

				return false;
			}
		}

		return true;
	}

	net::awaitable<void> do_site_transfer(
		std::shared_ptr<node>& p, auto& server, auto& session, std::shared_ptr<safety>& safety_ptr,
		beast::flat_buffer& buffer,
		http::request_parser<http::buffer_body>& parser, proxy_site_info& site,
		auto& client_endp, auto& client_ip, auto client_port)
	{
		net::tcp_socket backend(session->get_executor());

		auto e8 = co_await net::connect(backend, site.host, site.port);
		if (e8 || server->is_aborted())
		{
			app.logger->error("connect to backend service failed: {}:{} {} {}",
				client_ip, client_port, site.domain, e8.message());
			http::response<http::string_body> rep = http::make_error_page_response(http::status::service_unavailable);
			co_await http::async_write(session->get_stream(), rep);
			co_return;
		}

		safety_ptr->conns.emplace(std::addressof(backend), std::addressof(backend));
		std::defer auto_remove_conn = [&safety_ptr, &backend]() mutable
		{
			safety_ptr->conns.erase(std::addressof(backend));
		};

		set_proxy_headers(parser.get(), site);

		auto [e0, p0, r0, w0] = co_await http::relay(session->get_stream(), backend, buffer, parser);
		if (e0)
		{
			app.logger->error("relay first http request failed: {}:{} {} {}",
				client_ip, client_port, site.domain, e0.message());
			co_return;
		}

		auto req = parser.release();
		auto log_level = app.logger->level();

		if ((!site.requires_auth || site.auth_roles.empty()) && site.proxy_set_header.empty())
		{
			app.logger->debug("don't requries auth, switch to tcp transfer: {}:{} {}",
				client_ip, client_port, site.domain);
			if (auto b = buffer.data(); b.size())
			{
				app.logger->debug("send remaining data to backend: {}:{} {} {}",
					client_ip, client_port, site.domain, b.size());
				co_await net::async_write(backend, b, net::use_nothrow_awaitable);
			}
			co_return co_await do_transfer(
				p, server, session->get_stream(), backend, safety_ptr, site, client_endp, client_ip, client_port);
		}

		beast::flat_buffer buffer_backend;

		for (; !server->is_aborted();)
		{
			safety_ptr->deadline = std::max(
				safety_ptr->deadline, std::chrono::steady_clock::now() + std::chrono::minutes(10));

			app.logger->trace("recvd request: {}:{} {} {} {}",
				client_ip, client_port, site.domain, req.method_string(), req.target());

			if (websocket::is_upgrade(req))
			{
				app.logger->debug("websocket upgrade, switch to tcp transfer: {}:{} {}",
					client_ip, client_port, site.domain);
				if (auto b = buffer.data(); b.size())
				{
					app.logger->debug("send remaining data to backend: {}:{} {} {}",
						client_ip, client_port, site.domain, b.size());
					co_await net::async_write(backend, b, net::use_nothrow_awaitable);
				}
				co_return co_await do_transfer(
					p, server, session->get_stream(), backend, safety_ptr, site, client_endp, client_ip, client_port);
			}

			http::response_parser<http::buffer_body> rep_parser;
			rep_parser.body_limit((std::numeric_limits<std::size_t>::max)());
			if (req.method() == http::verb::head && site.skip_body_for_head_response)
			{
				rep_parser.skip(true);
			}
			if (req.method() == http::verb::head && log_level > spdlog::level::trace)
			{
				app.logger->trace("recvd head response begin: {}:{} {} [{}]",
					client_ip, client_port, site.domain, req.target());
				//std::array<char, 1024> buf;
				//auto [e1, n1] = co_await net::async_read_some(backend, net::buffer(buf), net::use_nothrow_awaitable);
				//std::string_view sv{ buf.data(), n1 };
				//app.logger->error("read http response: {}:{} {} [{}]", client_ip, client_port, site.domain, sv);
				//co_await net::delay(std::chrono::seconds(1));
			}
			auto [e1, p1, r1, w1] = co_await http::relay(
				backend, session->get_stream(), buffer_backend, rep_parser, [](auto&...) {});
			if (e1)
			{
				app.logger->error("relay response failed: {}:{} {} {} {}",
					client_ip, client_port, site.domain, req.method_string(), e1.message());
				break;
			}
			else
			{
				if (req.method() == http::verb::head && log_level > spdlog::level::trace)
				{
					std::stringstream ss;
					ss << rep_parser.get().base();
					app.logger->trace("recvd head response end: {}:{} {} [{}]",
						client_ip, client_port, site.domain, ss.str());
				}
			}

			if (!check_auth(p, safety_ptr, site, req, rep_parser.get(), client_ip, client_port) &&
				safety_ptr->auth_failed_times > 3)
				co_return;

			if (!req.keep_alive())
			{
				app.logger->trace("keep alive of request is false, go exit: {}:{} {} {}",
					client_ip, client_port, site.domain, req.target());
				break;
			}

			http::request_parser<http::buffer_body> req_parser;
			req_parser.body_limit((std::numeric_limits<std::size_t>::max)());
			auto req_header_cb = [&site, &req_parser, log_level, &client_ip, client_port]
			(auto& req, net::error_code&) mutable
			{
				if (req.method() == http::verb::head && site.skip_body_for_head_request)
				{
					req_parser.skip(true);
				}
				if (req.method() == http::verb::head && log_level > spdlog::level::trace)
				{
					std::stringstream ss;
					ss << req.base();
					app.logger->trace("recvd head request begin: {}:{} {} [{}]",
						client_ip, client_port, site.domain, ss.str());
				}
				set_proxy_headers(req, site);
			};
			auto [e3, p3, r3, w3] = co_await http::relay(
				session->get_stream(), backend, buffer, req_parser, req_header_cb);
			if (e3)
			{
				app.logger->debug("relay request failed: {}:{} {} {} {}",
					client_ip, client_port, site.domain, req.method_string(), e3.message());
				break;
			}
			else
			{
				if (req_parser.get().method() == http::verb::head && log_level > spdlog::level::trace)
				{
					app.logger->trace("recvd head request end: {}:{} {} [{}]",
						client_ip, client_port, site.domain, req_parser.get().target());
				}
			}

			req = req_parser.release();
		}
	}

	net::awaitable<void> do_recv(
		std::shared_ptr<node>& p, auto& server, auto& session, std::shared_ptr<safety>& safety_ptr,
		auto& client_endp, auto& client_ip, auto client_port)
	{
		beast::flat_buffer buffer;
		http::request_parser<http::buffer_body> parser;
		parser.body_limit((std::numeric_limits<std::size_t>::max)());

		auto [e1, n1] = co_await http::async_read_header(session->get_stream(), buffer, parser);
		if (e1)
		{
			std::string_view sv{ reinterpret_cast<std::string_view::pointer>(
				buffer.data().data()), (std::min<std::size_t>)(n1, 16) };
			app.logger->error("read first http packet failed: {}:{} {} {} {}:{}",
				client_ip, client_port, p->cfg.name, e1.message(), n1, sv);
			co_return;
		}

		std::string_view host;
		if (auto it = parser.get().find(http::field::host); it != parser.get().end())
		{
			host = it->value();
			host = host.substr(0, host.rfind(':'));
		}
		else
		{
			app.logger->error("recvd http request without host header: {}:{} {} {}",
				client_ip, client_port, p->cfg.name, parser.get().target());
			co_return;
		}

		auto it_site = p->cfg.proxy_sites.find(std::string(host));
		if (it_site == p->cfg.proxy_sites.end())
		{
			app.logger->error("can't find matched website: {}:{} {} host:{} {}",
				client_ip, client_port, p->cfg.name, host, parser.get().target());
			co_return;
		}
		else
		{
			co_await do_site_transfer(p, server, session, safety_ptr, buffer, parser,
				it_site->second, client_endp, client_ip, client_port);
		}
	}

	net::awaitable<void> do_session(
		std::shared_ptr<node>& p, auto& server, auto& session, std::shared_ptr<safety>& safety_ptr,
		auto& client_endp, auto& client_ip, auto client_port)
	{
		co_await server->session_map.async_add(session);
		co_await
		(
			do_recv(p, server, session, safety_ptr, client_endp, client_ip, client_port) ||
			net::watchdog(session->alive_time, net::http_idle_timeout)
		);
		co_await server->session_map.async_remove(session);
	}

	net::awaitable<void> do_auth_persist(
		std::shared_ptr<node> p, std::shared_ptr<safety> safety_ptr,
		auto client_endp, auto client_ip, auto client_port)
	{
		net::steady_timer t(co_await net::this_coro::executor);
		safety_ptr->timer = std::addressof(t);
		while (safety_ptr->deadline > std::chrono::steady_clock::now())
		{
			t.expires_at(safety_ptr->deadline);
			auto [e1] = co_await t.async_wait(net::use_nothrow_awaitable);
			if (e1)
				break;
		}
		p->safety_map.erase(client_endp.address());
	}

	std::tuple<bool, std::shared_ptr<safety>> safety_check(
		std::shared_ptr<node>& p, auto& client, auto& client_endp, auto& client_ip, auto client_port)
	{
		std::shared_ptr<safety> safety_ptr;
		if (auto it = p->safety_map.find(client_endp.address()); it == p->safety_map.end())
		{
			safety_ptr = std::make_shared<safety>();
			safety_ptr->has_authed = false;
			safety_ptr->auth_failed_times = 0;
			safety_ptr->deadline = std::chrono::steady_clock::now() + std::chrono::minutes(10);

			it = p->safety_map.emplace(client_endp.address(), safety_ptr).first;

			net::co_spawn(client.get_executor(), do_auth_persist(
				p, safety_ptr, client_endp, client_ip, client_port), net::detached);
		}
		else
		{
			safety_ptr = it->second;
			if (safety_ptr->auth_failed_times > 3)
			{
				app.logger->error("http_reverse_proxy: reject a client from blacklist: {}:{}",
					client_ip, client_port);
				return { false, std::move(safety_ptr)};
			}
		}

		return { true, std::move(safety_ptr) };
	}

	net::awaitable<void> client_join(std::shared_ptr<node> p, auto& server, auto client)
	{
		net::error_code ec{};
		auto client_endp = client.lowest_layer().remote_endpoint(ec);
		if (ec)
		{
			app.logger->error("get client remote endpoint failed: {}", ec.message());
			co_return;
		}
		auto client_ip = client_endp.address().to_string(ec);
		auto client_port = client_endp.port();

		p->client_count++;
		app.logger->trace("client join: {}:{} current client count: {}", client_ip, client_port, p->client_count);

		std::defer auto_log_when_destroyed = [&p, &client_ip, client_port]() mutable
		{
			p->client_count--;
			app.logger->trace("client exit: {}:{} current client count: {}", client_ip, client_port, p->client_count);
		};

		auto [result, safety_ptr] = safety_check(p, client, client_endp, client_ip, client_port);
		if (!result)
			co_return;

		if constexpr (is_https_server<decltype(server)>)
		{
			auto session = std::make_shared<net::https_session>(std::move(client), server->ssl_context);
			auto [e2] = co_await net::async_handshake(
				session->ssl_stream, net::ssl::stream_base::handshake_type::server);
			if (e2)
			{
				app.logger->error("http_reverse_proxy handshake failure: {}:{} {} {}",
					client_ip, client_port, p->cfg.name, e2.message());
				co_return;
			}
			co_await do_session(p, server, session, safety_ptr, client_endp, client_ip, client_port);
		}
		else
		{
			auto session = std::make_shared<net::http_session>(std::move(client));
			co_await do_session(p, server, session, safety_ptr, client_endp, client_ip, client_port);
		}
	}

	net::awaitable<void> start_server(std::shared_ptr<node> p, auto& server)
	{
		// delay some time to ensure the init log finished.
		co_await net::delay(std::chrono::milliseconds(500));

		auto [ec, ep] = co_await server->async_listen(p->cfg.listen_address, p->cfg.listen_port);
		if (ec)
		{
			app.logger->error("http_reverse_proxy listen failure: {} {}:{} {}",
				p->cfg.name, p->cfg.listen_address, p->cfg.listen_port, ec.message());
			co_return;
		}

		app.logger->info("http_reverse_proxy listen success: {} {}:{}",
			p->cfg.name, server->get_listen_address(), server->get_listen_port());

		while (!server->is_aborted())
		{
			auto [e1, client] = co_await server->acceptor.async_accept();
			if (e1)
			{
				co_await net::delay(std::chrono::milliseconds(100));
			}
			else
			{
				net::co_spawn(server->get_executor(), client_join(p, server, std::move(client)), net::detached);
			}
		}
	}

	http_reverse_proxy::http_reverse_proxy() : imodular()
	{
	}

	bool http_reverse_proxy::init()
	{
		auto cfgs = app.config->get_http_reverse_proxy_cfg();

		for (auto& cfg : cfgs)
		{
			if (!cfg.enable)
				continue;

			std::shared_ptr<node> p = std::make_shared<node>();

			p->cfg = std::move(cfg);

			if /**/ (net::iequals(p->cfg.protocol, "http"))
			{
				p->server = std::make_shared<net::http_server>(p->ctx.get_executor());
			}
			else if (net::iequals(p->cfg.protocol, "https"))
			{
				auto cert_file_path = to_canonical_path(app.exe_directory, p->cfg.cert_file);
				auto key_file_path = to_canonical_path(app.exe_directory, p->cfg.key_file);

				net::error_code ec{};
				net::ssl::context sslctx(net::ssl::context::tlsv12);
				sslctx.set_options(
					net::ssl::context::default_workarounds |
					net::ssl::context::no_sslv2 |
					net::ssl::context::single_dh_use, ec);
				if (ec)
				{
					app.logger->error("    set ssl options failed: {} {}", p->cfg.name, ec.message());
					continue;
				}
				sslctx.use_certificate_chain_file(cert_file_path.string(), ec);
				if (ec)
				{
					app.logger->error("    set ssl certificate chain for '{}' failed: {} {}",
						p->cfg.name, p->cfg.cert_file, ec.message());
					continue;
				}
				sslctx.use_private_key_file(key_file_path.string(), asio::ssl::context::pem, ec);
				if (ec)
				{
					app.logger->error("    set ssl private key for '{}' failed: {} {}",
						p->cfg.name, p->cfg.key_file, ec.message());
					continue;
				}

				p->server = std::make_shared<net::https_server>(p->ctx.get_executor(), std::move(sslctx));
			}
			else
			{
				app.logger->error("    the protocol config '{}' of '{}' is invalid",
					p->cfg.protocol, p->cfg.name);
				continue;
			}

			std::visit([&p](auto& server) mutable
				{
					init_server(p, server);
				}, p->server);

			nodes.emplace_back(std::move(p));
		}

		return true;
	}

	bool http_reverse_proxy::start()
	{
		for (auto& p : nodes)
		{
			std::visit([&p](auto& server) mutable
				{
					net::co_spawn(server->get_executor(), start_server(p, server), net::detached);
				}, p->server);
		}

		return true;
	}

	void http_reverse_proxy::stop()
	{
		for (auto& p : nodes)
		{
			std::visit([&p](auto& server) mutable
			{
				server->async_stop([&p](net::error_code)
				{
					for (auto& [addr, ptr] : p->safety_map)
					{
						if (ptr->timer)
							net::cancel_timer(*(ptr->timer));

						for (auto& [k, v] : ptr->conns)
						{
							std::visit([](auto& p)
							{
								net::error_code ec{};
								p->lowest_layer().shutdown(net::socket_base::shutdown_both, ec);
								p->lowest_layer().close(ec);
							}, v);
						}
					}
				});
			}, p->server);
		}
		for (auto& p : nodes)
		{
			p->ctx.join();
		}
	}

	void http_reverse_proxy::uninit()
	{
		nodes.clear();
	}
}
