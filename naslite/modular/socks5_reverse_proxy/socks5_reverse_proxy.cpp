#include "socks5_reverse_proxy.h"

#include "../../main/app.hpp"

namespace nas
{
	using safety = socks5_reverse_proxy::safety;
	using node = socks5_reverse_proxy::node;
	using time_point = std::chrono::steady_clock::time_point;

	void init_server(std::shared_ptr<node>& p)
	{
		net::ignore_unused(p);
	}

	net::awaitable<void> do_auth_persist(
		std::shared_ptr<node> p, std::shared_ptr<safety> safety_ptr, net::ip::address addr)
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
		p->safety_map.erase(addr);
	}

	net::awaitable<bool> socks5_auth(std::shared_ptr<node> p, socks5::handshake_info& info)
	{
		net::error_code ec{};
		auto addr = info.client_endpoint.address();
		auto port = info.client_endpoint.port();

		if (auto it = p->cfg.tokens.find(info.username); it != p->cfg.tokens.end())
		{
			auto& token = it->second;

			if (token.password == info.password && std::chrono::system_clock::now() < token.expires_at)
			{
				app.logger->info("socks5_reverse_proxy: authed success: {}:{} {} {}",
					addr.to_string(ec), port, info.username, info.password);

				co_return true;
			}
		}

		app.logger->error("socks5_reverse_proxy: authed failure: {}:{} {} {}",
			addr.to_string(ec), port, info.username, info.password);

		std::shared_ptr<safety> safety_ptr;
		if (auto it = p->safety_map.find(addr); it == p->safety_map.end())
		{
			safety_ptr = std::make_shared<safety>();
			safety_ptr->has_authed = false;
			safety_ptr->auth_failed_times = 1;
			safety_ptr->deadline = std::chrono::steady_clock::now() + std::chrono::minutes(10);

			it = p->safety_map.emplace(addr, safety_ptr).first;

			net::co_spawn(p->ctx.get_executor(), do_auth_persist(p, safety_ptr, addr), net::detached);
		}
		else
		{
			safety_ptr = it->second;
			safety_ptr->auth_failed_times++;
			if (safety_ptr->auth_failed_times > 3)
			{
				safety_ptr->deadline = std::max(safety_ptr->deadline,
					std::chrono::steady_clock::now() + std::chrono::minutes(p->cfg.ip_blacklist_minutes));

				app.logger->critical("socks5_reverse_proxy: authed failed too much: {}:{} {} {}",
					addr.to_string(ec), port, info.username, info.password);
			}
			else
			{
				safety_ptr->deadline = std::max(safety_ptr->deadline, std::chrono::steady_clock::now() +
					std::chrono::minutes(10 * safety_ptr->auth_failed_times));
			}
		}

		co_return false;
	}

	net::awaitable<void> tcp_transfer(
		std::shared_ptr<net::socks5_session>& conn, net::tcp_socket& from, net::tcp_socket& to)
	{
		std::array<char, 1024> data;

		for (;;)
		{
			conn->update_alive_time();

			auto [e1, n1] = co_await net::async_read_some(from, net::buffer(data));
			if (e1)
				break;

			auto [e2, n2] = co_await net::async_write(to, net::buffer(data, n1));
			if (e2)
				break;
		}
	}

	net::awaitable<void> udp_transfer(
		std::shared_ptr<net::socks5_session>& conn, net::tcp_socket& front, net::udp_socket& bound)
	{
		std::array<char, 1024> data;
		net::ip::udp::endpoint sender_endpoint{};

		for (;;)
		{
			conn->update_alive_time();

			auto [e1, n1] = co_await net::async_receive_from(bound, net::buffer(data), sender_endpoint);
			if (e1)
				break;

			if (socks5::is_data_come_from_frontend(front, sender_endpoint, conn->handshake_info))
			{
				conn->last_read_channel = net::protocol::udp;

				auto [e2, n2] = co_await socks5::async_forward_data_to_backend(bound, net::buffer(data, n1));
				if (e2)
					break;
			}
			else
			{
				if (conn->last_read_channel == net::protocol::udp)
				{
					auto [e2, n2] = co_await socks5::async_forward_data_to_frontend(
						bound, net::buffer(data, n1), sender_endpoint, conn->get_frontend_udp_endpoint());
					if (e2)
						break;
				}
				else
				{
					auto [e2, n2] = co_await socks5::async_forward_data_to_frontend(
						front, net::buffer(data, n1), sender_endpoint);
					if (e2)
						break;
				}
			}
		}
	}

	net::awaitable<void> ext_transfer(
		std::shared_ptr<net::socks5_session>& conn, net::tcp_socket& front, net::udp_socket& bound)
	{
		std::string buf;

		for (;;)
		{
			conn->update_alive_time();

			// recvd data from the front client by tcp, forward the data to back client.
			auto [e1, n1] = co_await net::async_read_until(
				front, net::dynamic_buffer(buf), socks5::udp_match_condition{});
			if (e1)
				break;

			conn->last_read_channel = net::protocol::tcp;

			// this packet is a extension protocol base of below:
			// +----+------+------+----------+----------+----------+
			// |RSV | FRAG | ATYP | DST.ADDR | DST.PORT |   DATA   |
			// +----+------+------+----------+----------+----------+
			// | 2  |  1   |  1   | Variable |    2     | Variable |
			// +----+------+------+----------+----------+----------+
			// the RSV field is the real data length of the field DATA.
			// so we need unpacket this data, and send the real data to the back client.

			auto [err, ep, domain, real_data] = socks5::parse_udp_packet(net::buffer(buf.data(), n1), true);
			if (err == 0)
			{
				if (domain.empty())
				{
					auto [e2, n2] = co_await net::async_send_to(bound, net::buffer(real_data), ep);
					if (e2)
						break;
				}
				else
				{
					auto [e2, n2] = co_await net::async_send_to(
						bound, net::buffer(real_data), std::move(domain), ep.port());
					if (e2)
						break;
				}
			}
			else
			{
				break;
			}

			buf.erase(0, n1);
		}

		net::error_code ec{};
		front.shutdown(net::socket_base::shutdown_both, ec);
		front.close(ec);
	}

	net::awaitable<void> do_proxy(std::shared_ptr<net::socks5_session>& conn)
	{
		auto result = co_await(
			socks5::accept(conn->socket, conn->auth_config, conn->handshake_info) ||
			net::timeout(std::chrono::seconds(5)));
		if (net::is_timeout(result))
			co_return;
		auto e1 = std::get<0>(result);
		if (e1)
			co_return;

		net::error_code ec{};

		if (conn->handshake_info.cmd == socks5::command::connect)
		{
			net::tcp_socket& front_client = conn->socket;
			net::tcp_socket& back_client = *conn->get_backend_tcp_socket();
			co_await(
				tcp_transfer(conn, front_client, back_client) ||
				tcp_transfer(conn, back_client, front_client) ||
				net::watchdog(conn->alive_time, net::proxy_idle_timeout));
			front_client.close(ec);
			back_client.close(ec);
		}
		else if (conn->handshake_info.cmd == socks5::command::udp_associate)
		{
			net::tcp_socket& front_client = conn->socket;
			net::udp_socket& back_client = *conn->get_backend_udp_socket();
			co_await(
				udp_transfer(conn, front_client, back_client) ||
				ext_transfer(conn, front_client, back_client) ||
				net::watchdog(conn->alive_time, net::proxy_idle_timeout));
			front_client.close(ec);
			back_client.close(ec);
		}
	}

	std::tuple<bool, std::shared_ptr<safety>> safety_check(std::shared_ptr<node>& p, auto& client)
	{
		net::error_code ec{};
		auto endp = client.lowest_layer().remote_endpoint(ec);
		if (ec)
			return { false, nullptr };

		auto addr = endp.address();
		auto port = endp.port();

		auto& safety_map = p->safety_map;

		std::shared_ptr<safety> safety_ptr;

		if (auto it = safety_map.find(addr); it != safety_map.end())
		{
			if (it->second->auth_failed_times > 3)
			{
				safety_ptr = it->second;
				app.logger->error("socks5_reverse_proxy: reject a client from forbiddened ip: {}:{}",
					addr.to_string(ec), port);
				return { false, std::move(safety_ptr) };
			}
		}

		return { true, std::move(safety_ptr) };
	}

	net::awaitable<void> client_join(std::shared_ptr<node>& p, std::shared_ptr<net::socks5_session> session)
	{
		auto [result, safety_ptr] = safety_check(p, session->socket);
		if (!result)
			co_return;

		co_await p->server.session_map.async_add(session);

		session->socket.set_option(net::ip::tcp::no_delay(true));
		session->socket.set_option(net::socket_base::keep_alive(true));

		co_await do_proxy(session);
		co_await session->async_disconnect();

		co_await p->server.session_map.async_remove(session);
	}

	net::awaitable<void> start_server(std::shared_ptr<node> p, socks5::auth_config auth_cfg)
	{
		// delay some time to ensure the init log finished.
		co_await net::delay(std::chrono::milliseconds(500));

		auto& server = p->server;

		auto [ec, ep] = co_await server.async_listen(p->cfg.listen_address, p->cfg.listen_port);
		if (ec)
		{
			app.logger->error("socks5_reverse_proxy listen failure: {} {}:{} {}",
				p->cfg.name, p->cfg.listen_address, p->cfg.listen_port, ec.message());
			co_return;
		}

		app.logger->info("socks5_reverse_proxy listen success: {} {}:{}",
			p->cfg.name, server.get_listen_address(), server.get_listen_port());

		while (!server.is_aborted())
		{
			auto [e1, client] = co_await server.acceptor.async_accept();
			if (e1)
			{
				co_await net::delay(std::chrono::milliseconds(100));
			}
			else
			{
				auto session = std::make_shared<net::socks5_session>(std::move(client));
				session->auth_config = auth_cfg;
				net::co_spawn(server.get_executor(), client_join(p, std::move(session)), net::detached);
			}
		}
	}

	socks5_reverse_proxy::socks5_reverse_proxy() : imodular()
	{

	}

	bool socks5_reverse_proxy::init()
	{
		auto cfgs = app.config->get_socks5_reverse_proxy_cfg();

		for (auto& cfg : cfgs)
		{
			if (!cfg.enable)
				continue;

			std::shared_ptr<node> p = std::make_shared<node>();

			p->cfg = std::move(cfg);

			init_server(p);

			nodes.emplace_back(std::move(p));
		}

		return true;
	}

	bool socks5_reverse_proxy::start()
	{
		for (auto& p : nodes)
		{
			socks5::auth_config auth_cfg;

			for (auto m : p->cfg.supported_method)
			{
				auth_cfg.supported_method.emplace_back(static_cast<socks5::auth_method>(m));
			}

			auth_cfg.on_auth = std::bind_front(socks5_auth, p);

			net::co_spawn(p->server.get_executor(), start_server(p, std::move(auth_cfg)), net::detached);
		}

		return true;
	}

	void socks5_reverse_proxy::stop()
	{
		for (auto& p : nodes)
		{
			p->server.async_stop([&p](net::error_code)
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
		}
		for (auto& p : nodes)
		{
			p->ctx.join();
		}
	}

	void socks5_reverse_proxy::uninit()
	{
		nodes.clear();
	}
}
