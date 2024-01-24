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
	class frontend_http_server final
		: public imodular
		, public pfr::base_dynamic_creator<imodular, frontend_http_server>
	{
	public:
		struct router_data
		{
			net::tcp_socket& client;
			frontend_http_server_info& cfg;
		};

		using http_server_ex = net::basic_http_server<
			net::http_session, http::basic_router<http::web_request, http::web_response, router_data>>;
		using https_server_ex = net::basic_https_server<
			net::https_session, http::basic_router<http::web_request, http::web_response, router_data>>;

		struct node
		{
			frontend_http_server_info cfg{};
			net::io_context_thread ctx{ 1 };
			net::ip::tcp::socket sock_for_temperatures{ ctx.get_executor() };
			std::variant<std::shared_ptr<http_server_ex>, std::shared_ptr<https_server_ex>> server;
		};

	public:
		frontend_http_server();

		virtual bool init() override;

		virtual bool start() override;

		virtual void stop() override;

		virtual void uninit() override;

	public:
		std::vector<std::shared_ptr<node>> nodes;
	};
}
