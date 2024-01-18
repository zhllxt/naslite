#pragma once

#include "../../core/net.hpp"
#include "../../core/json.hpp"
#include "../../core/iconfig.hpp"
#include "../../core/utils.hpp"
#include "../../core/imodular.hpp"

#include <asio3/proxy/socks5_server.hpp>

namespace nas
{
	class socks5_reverse_proxy final
		: public imodular
		, public pfr::base_dynamic_creator<imodular, socks5_reverse_proxy>
	{
	public:
		struct safety
		{
			bool has_authed;
			std::int32_t auth_failed_times;
			std::chrono::steady_clock::time_point deadline;
			net::steady_timer* timer = nullptr;
			std::unordered_map<void*,
				std::variant<net::tcp_socket*, net::ssl::stream<net::tcp_socket>*>> conns;
		};

		struct node
		{
			socks5_reverse_proxy_info cfg{};
			net::io_context_thread ctx{ 1 };
			net::socks5_server server{ ctx.get_executor() };
			std::unordered_map<net::ip::address, std::shared_ptr<safety>> safety_map;
		};

	public:
		socks5_reverse_proxy();

		virtual bool init() override;

		virtual bool start() override;

		virtual void stop() override;

		virtual void uninit() override;

	public:
		std::vector<std::shared_ptr<node>> nodes;
	};
}
