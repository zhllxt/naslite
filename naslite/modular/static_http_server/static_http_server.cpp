#include "static_http_server.h"

#include "../../main/app.hpp"

namespace nas
{
	using node = static_http_server::node;

	template<typename T>
	concept is_https_server = requires(T & a)
	{
		a->ssl_context;
	};

	void init_server(std::shared_ptr<node>& p, auto& server)
	{
		std::error_code ec{};
		if (std::filesystem::absolute(p->cfg.webroot, ec) == std::filesystem::path(p->cfg.webroot))
			server->webroot = p->cfg.webroot;
		else
			server->webroot = app.exe_directory / p->cfg.webroot;

		if (ec)
			app.logger->error("    the webroot config '{}' is invalid: {}", p->cfg.webroot, ec.message());

		server->webroot = std::filesystem::canonical(server->webroot, ec);

		if (!std::filesystem::exists(server->webroot, ec) && !ec)
			app.logger->error("    the webroot directory '{}' of '{}' is not exists",
				p->cfg.webroot, p->cfg.name);
		else
			app.logger->info("    the webroot directory of '{}' is: {}",
				p->cfg.name, server->webroot.string());

		server->router.add("/", [p, server]
		(http::web_request& req, http::web_response& rep) mutable -> net::awaitable<bool>
		{
			std::filesystem::path filepath = server->webroot / p->cfg.index;
			auto [ec, file, content] = co_await net::async_read_file_content(filepath.string());
			if (ec)
			{
				rep = http::make_error_page_response(http::status::not_found);
				co_return true;
			}

			rep = http::make_html_response(std::move(content));
			co_return true;
		}, http::enable_cache);

		server->router.add("*", [p, server]
		(http::web_request& req, http::web_response& rep) mutable -> net::awaitable<bool>
		{
			std::filesystem::path filepath = net::make_filepath(server->webroot, req.target());

			auto [ec, file, content] = co_await net::async_read_file_content(filepath.string());
			if (ec)
			{
				rep = http::make_error_page_response(http::status::not_found);
				co_return true;
			}

			auto res = http::make_text_response(std::move(content));
			res.set(http::field::content_type, http::extension_to_mimetype(filepath.extension().string()));
			rep = std::move(res);
			co_return true;
		}, http::enable_cache);
	}

	net::awaitable<void> do_recv(std::shared_ptr<node> p, auto& server, auto& session)
	{
		// This buffer is required to persist across reads
		beast::flat_buffer buf;

		for (;;)
		{
			// Read a request
			http::web_request req;
			auto [e1, n1] = co_await http::async_read(session->get_stream(), buf, req);
			if (e1)
				break;

			session->update_alive_time();

			if (!p->lock.try_send())
			{
				co_await p->lock.async_send(net::deferred);
			}

			[[maybe_unused]] net::defer_unlock defered_unlock{ p->lock };

			http::web_response rep;
			bool result = co_await server->router.route(req, rep);

			// Send the response
			auto [e2, n2] = co_await beast::async_write(session->get_stream(), std::move(rep));
			if (e2)
				break;

			if (!result || !req.keep_alive())
			{
				// This means we should close the connection, usually because
				// the response indicated the "Connection: close" semantic.
				break;
			}
		}

		session->close();
	}

	net::awaitable<void> do_session(std::shared_ptr<node> p, auto& server, auto session)
	{
		co_await server->session_map.async_add(session);
		co_await(do_recv(p, server, session) || net::watchdog(session->alive_time, net::http_idle_timeout));
		co_await server->session_map.async_remove(session);
	}

	net::awaitable<void> client_join(std::shared_ptr<node> p, auto& server, auto client)
	{
		if constexpr (is_https_server<decltype(server)>)
		{
			auto session = std::make_shared<net::https_session>(std::move(client), server->ssl_context);
			auto [e2] = co_await net::async_handshake(
				session->ssl_stream, net::ssl::stream_base::handshake_type::server);
			if (e2)
			{
				app.logger->error("static_http_server handshake failure: {} {}:{} {}",
					p->cfg.name, p->cfg.listen_address, p->cfg.listen_port, e2.message());
				co_return;
			}
			co_await do_session(p, server, std::move(session));
		}
		else
		{
			auto session = std::make_shared<net::http_session>(std::move(client));
			co_await do_session(p, server, std::move(session));
		}
	}

	net::awaitable<void> start_server(std::shared_ptr<node> p, auto& server)
	{
		// delay some time to ensure the init log finished.
		co_await net::delay(std::chrono::milliseconds(500));

		auto [ec, ep] = co_await server->async_listen(p->cfg.listen_address, p->cfg.listen_port);
		if (ec)
		{
			app.logger->error("static_http_server listen failure: {} {}:{} {}",
				p->cfg.name, p->cfg.listen_address, p->cfg.listen_port, ec.message());
			co_return;
		}

		app.logger->info("static_http_server listen success: {} {}:{}",
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

	static_http_server::static_http_server() : imodular()
	{

	}

	bool static_http_server::init()
	{
		auto cfgs = app.config->get_http_server_cfg();

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

				// nginx: ssl->ctx = SSL_CTX_new(SSLv23_method());
				net::error_code ec{};
				net::ssl::context sslctx(net::ssl::context::sslv23);
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
				sslctx.use_private_key_file(key_file_path.string(), net::ssl::context::pem, ec);
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
				app.logger->error("    the protocol config '{}' of '{}' is invalid", p->cfg.protocol, p->cfg.name);
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

	bool static_http_server::start()
	{
		for (auto& p : nodes)
		{
			std::visit([&p](auto& server) mutable
				{
					net::co_spawn(server->get_executor(), start_server(p, server), net::detached);
				}, p->server);
		}

		app.event_dispatcher.append_listener(typeid(*this).name(), typeid(http_clear_cache_all_event),
			[this](std::shared_ptr<ievent> e) mutable
			{
				for (auto& p : nodes)
				{
					// change thread to current io_context
					net::co_spawn(p->ctx.get_executor(), handle_event(p,
						std::static_pointer_cast<http_clear_cache_all_event>(e)), net::detached);
				}
			});
		
		return true;
	}

	void static_http_server::stop()
	{
		app.event_dispatcher.remove_listener(typeid(*this).name());

		for (auto& p : nodes)
		{
			std::visit([&p](auto& server) mutable
				{
					server->async_stop([](net::error_code) {});
				}, p->server);
		}
		for (auto& p : nodes)
		{
			p->ctx.join();
		}
	}

	void static_http_server::uninit()
	{
		nodes.clear();
	}

	net::awaitable<void> static_http_server::handle_event(
		std::shared_ptr<node> p, std::shared_ptr<http_clear_cache_all_event> e)
	{
		// acquire lock
		if (!p->lock.try_send())
		{
			co_await p->lock.async_send(net::deferred);
		}

		// can't use defer_unlock, beacuse it will be destroyed in the thread which 
		// is not equal to current thread.
		//[[maybe_unused]] net::defer_unlock defered_unlock{ p->lock };

		std::visit([](auto& server) mutable
		{
			server->router.get_cache().clear();
		}, p->server);

		// release lock
		p->lock.try_receive([](auto...) {});

		// change thread to caller io_context
		co_await net::dispatch(net::bind_executor(e->ch.get_executor(), net::use_nothrow_awaitable));
		co_await e->ch.async_send(net::error_code{}, net::use_nothrow_awaitable);
	}
}
