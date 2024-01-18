#pragma once

#include <variant>

#include "../../core/net.hpp"
#include "../../core/json.hpp"
#include "../../core/iconfig.hpp"
#include "../../core/utils.hpp"
#include "../../core/imodular.hpp"

#include <asio3/http/https_server.hpp>

namespace nas
{
	class http_reverse_proxy final
		: public imodular
		, public pfr::base_dynamic_creator<imodular, http_reverse_proxy>
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
			http_reverse_proxy_info cfg{};
			net::io_context_thread ctx{ 1 };
			std::variant<std::shared_ptr<net::http_server>, std::shared_ptr<net::https_server>> server;
			std::unordered_map<net::ip::address, std::shared_ptr<safety>> safety_map;
			int client_count = 0;
		};

	public:
		http_reverse_proxy();

		virtual bool init() override;

		virtual bool start() override;

		virtual void stop() override;

		virtual void uninit() override;

	public:
		std::vector<std::shared_ptr<node>> nodes;
	};
}
