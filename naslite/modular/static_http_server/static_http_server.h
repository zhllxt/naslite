#pragma once

#include <variant>

#include "../../core/net.hpp"
#include "../../core/json.hpp"
#include "../../core/iconfig.hpp"
#include "../../core/utils.hpp"
#include "../../core/imodular.hpp"

#include "../frontend_http_server/http_clear_cache_all_event.hpp"

#include <asio3/http/https_server.hpp>

namespace nas
{
	class static_http_server final
		: public imodular
		, public pfr::base_dynamic_creator<imodular, static_http_server>
	{
	public:
		using lock_type = net::as_tuple_t<net::use_awaitable_t<>>::as_default_on_t<
			net::experimental::channel<void()>>;

		struct node
		{
			static_http_server_info cfg{};
			net::io_context_thread ctx{ 1 };
			lock_type lock{ ctx.get_executor(), 1 };
			std::variant<std::shared_ptr<net::http_server>, std::shared_ptr<net::https_server>> server;
		};

	public:
		static_http_server();

		virtual bool init() override;

		virtual bool start() override;

		virtual void stop() override;

		virtual void uninit() override;

		net::awaitable<void> handle_event(
			std::shared_ptr<node> p, std::shared_ptr<http_clear_cache_all_event> e);

	public:
		std::vector<std::shared_ptr<node>> nodes;
	};
}
