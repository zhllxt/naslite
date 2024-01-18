#pragma once

#include "../../core/net.hpp"
#include "../../core/json.hpp"
#include "../../core/iconfig.hpp"
#include "../../core/utils.hpp"
#include "../../core/imodular.hpp"

#include "service_status_event.hpp"
#include "service_start_event.hpp"
#include "service_stop_event.hpp"
#include "service_start_all_event.hpp"
#include "service_stop_all_event.hpp"

#include <asio3/core/io_context_thread.hpp>
#include <boost/process/v2.hpp>
#include <boost/process/filesystem.hpp>
//#include <boost/process/v2/windows/show_window.hpp>

namespace nas
{
	namespace bp = boost::process::v2;

	class service_process_mgr final
		: public imodular
		, public pfr::base_dynamic_creator<imodular, service_process_mgr>
	{
	public:
		struct node
		{
			net::io_context_thread ctx{ 1 };
			net::signal_set sig{ ctx.get_executor(), SIGINT };
			std::atomic_flag aborted;
			// process_info in the 'cfg' has hold the io_context of 'ctx',
			// so 'cfg' must be destroyed before 'ctx', otherwise crash.
			service_process_mgr_info cfg{};
		};

	public:
		service_process_mgr();

		virtual bool init() override;

		virtual bool start() override;

		virtual void stop() override;

		virtual void uninit() override;

		template<typename T>
		void append_listener();

		net::awaitable<void> handle_event(std::shared_ptr<node> p, std::shared_ptr<service_status_event> e);
		net::awaitable<void> handle_event(std::shared_ptr<node> p, std::shared_ptr<service_start_event> e);
		net::awaitable<void> handle_event(std::shared_ptr<node> p, std::shared_ptr<service_stop_event> e);
		net::awaitable<void> handle_event(std::shared_ptr<node> p, std::shared_ptr<service_start_all_event> e);
		net::awaitable<void> handle_event(std::shared_ptr<node> p, std::shared_ptr<service_stop_all_event> e);

	public:
		std::vector<std::shared_ptr<node>> nodes;
	};
}
