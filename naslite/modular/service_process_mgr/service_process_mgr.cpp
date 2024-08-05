#include "service_process_mgr.h"

#include "../../core/utils.hpp"
#include "../../main/app.hpp"

#include <asio3/core/codecvt.hpp>
#include <asio3/core/defer.hpp>
#include <ranges>

namespace nas
{
	using node = service_process_mgr::node;

	bool is_process_running(bp::process* p, net::error_code& ec)
	{
		if (!p)
			return false;

		if (!p->is_open())
			return false;

		if (!p->id())
			return false;

		if (p->running(ec))
			return true;

	#if ASIO3_OS_WINDOWS
		if (ec.value() == 5)
			return true;
	#else
	#endif
		return false;
	}

	net::awaitable<void> wait_signal(std::shared_ptr<node> p)
	{
		for (; !p->aborted.test();)
		{
			auto [e1, n1] = co_await p->sig.async_wait(net::use_nothrow_awaitable);

			app.logger->debug("main signal callback triggered: {} {}", n1, e1.message());
		}

		app.logger->debug("main signal callback exited");
	}

	net::awaitable<void> start_process(std::shared_ptr<node> p, process_info& info)
	{
		net::error_code ec;

		co_await net::dispatch(net::bind_executor(p->ctx.get_executor(), net::use_nothrow_awaitable));

		std::shared_ptr<bp::process> proc = std::static_pointer_cast<bp::process>(info.process);

		if (is_process_running(proc.get(), ec))
			co_return;

		app.logger->trace("prepare start process: {} {}", info.name, info.path);

		std::vector<std::string> args = net::split(info.args, ' ');
		std::erase_if(args, [](const std::string& s) { return s.empty(); });

		proc = std::make_shared<bp::process>(
			p->ctx.get_executor(),
			bp::filesystem::path(info.path),
			args,
			bp::process_start_dir{ bp::filesystem::path(info.path).parent_path() }/*,
			bp::windows::show_window_normal*/
		);
		info.process = proc;

		proc->async_wait([info, pid = proc->id()](net::error_code ec, int sig)
		{
			app.logger->info("process exited: {} {} {} {}", info.name, pid, sig, ec.message());
		});

		if (is_process_running(proc.get(), ec))
		{
			app.logger->debug("start process successed: {} {}", info.name, proc->id());
		}
		else
		{
			app.logger->error("start process failed: {} {}", info.name, ec.message());
		}
	}

	net::awaitable<void> do_stop_process(
		std::shared_ptr<node> p, const std::string& name, std::shared_ptr<bp::process>& process)
	{
		net::error_code ec;
		
		for (int i = 0; i < 5; i++)
		{
			if (!is_process_running(process.get(), ec))
				break;
			if (send_signal_to_process(*app.logger, name, *process))
				break;

			co_await net::async_sleep(p->ctx.get_executor(), std::chrono::milliseconds(100),
				net::bind_executor(p->ctx.get_executor(), net::use_nothrow_awaitable));
		}

		net::steady_timer timer(p->ctx.get_executor());
		auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(p->cfg.stop_process_timeout);
		while (std::chrono::steady_clock::now() < deadline)
		{
			if (!is_process_running(process.get(), ec))
			{
				app.logger->trace("process is not running, break the stop process timeout checker: {}", name);
				break;
			}
			timer.expires_after(std::chrono::milliseconds(100));
			co_await timer.async_wait(net::bind_executor(timer.get_executor(), net::use_nothrow_awaitable));
		}

		if (is_process_running(process.get(), ec))
		{
			app.logger->info("interrupt process timeout, terminate it: {} {}", name, ec.message());

			process->terminate(ec);
		}
	}

	net::awaitable<void> stop_process(std::shared_ptr<node> p, process_info& info)
	{
		net::error_code ec;

		co_await net::dispatch(net::bind_executor(p->ctx.get_executor(), net::use_nothrow_awaitable));

		if (!info.process)
			co_return;

		if (!is_process_running(static_cast<bp::process*>(info.process.get()), ec))
			co_return;

		bp::pid_type main_pid = static_cast<bp::process*>(info.process.get())->id();

		std::vector<std::string> childs = net::split(info.childs, ';');
		std::erase_if(childs, [](const std::string& s) { return s.empty(); });

		app.logger->trace("prepare stop process: {} {}", info.name, info.path);

		// stop childs
		for (const std::string& child_name : childs)
		{
			for (auto pid : find_pid_by_name(child_name))
			{
				if (pid == main_pid)
					continue;

				std::shared_ptr<bp::process> child_process =
					std::make_shared<bp::process>(p->ctx.get_executor(), pid);

				child_process->async_wait([child_process, child_name, pid](net::error_code ec, int sig)
				{
					app.logger->info("process exited: {} {} {} {}", child_name, pid, sig, ec.message());
				});

				co_await do_stop_process(p, child_name, child_process);
			}
		}

		if (info.process)
		{
			std::shared_ptr<bp::process> process = std::static_pointer_cast<bp::process>(info.process);

			//process->interrupt(ec);

			co_await do_stop_process(p, info.name, process);
		}

		app.logger->debug("stop process successed: {} {}", info.name, ec.message());
	}

	net::awaitable<void> stop_all_process(std::shared_ptr<node> p)
	{
		for (auto& info : p->cfg.process_list | std::views::reverse)
		{
			co_await stop_process(p, info);
		}
	}

	net::awaitable<void> start_all_process(std::shared_ptr<node> p)
	{
		for (auto& info : p->cfg.process_list)
		{
			co_await start_process(p, info);
		}
	}

	net::awaitable<void> attach_all_process(std::shared_ptr<node> p)
	{
		net::error_code ec;

		for (auto& info : p->cfg.process_list)
		{
			if (info.process && is_process_running(static_cast<bp::process*>(info.process.get()), ec))
				continue;

			std::string process_name = std::filesystem::path(info.path).filename().string();

			if (auto pids = find_pid_by_name(process_name); !pids.empty())
			{
				auto pid = pids.front();

				// note: need modify the code of /boost/process/v2/detail/impl/process_handle_windows.ipp
				// set the PROCESS_QUERY_INFORMATION flag.
				// like this: auto proc = OpenProcess(PROCESS_TERMINATE | SYNCHRONIZE | PROCESS_QUERY_INFORMATION, FALSE, pid);
				std::shared_ptr<bp::process> proc =
					std::make_shared<bp::process>(p->ctx.get_executor(), pid);
				info.process = proc;

				proc->async_wait([proc, name = info.name, pid](net::error_code ec, int sig)
				{
					app.logger->info("process exited: {} {} {} {}", name, pid, sig, ec.message());
				});

				if (is_process_running(proc.get(), ec))
				{
					app.logger->debug("attach process successed: {} {}", info.name, proc->id());
				}
				else
				{
					app.logger->error("attach process failed: {} {}", info.name, ec.message());
				}
			}
		}

		co_return;
	}

	net::awaitable<void> runonce_prepose_process(std::shared_ptr<node> p)
	{
		co_await net::dispatch(net::bind_executor(p->ctx.get_executor(), net::use_nothrow_awaitable));

		// I have observed the following phenomenon: when sending a SIGINT signal 
		// to any one process for the first time, it causes all processes to receive 
		// the SIGINT signal, leading to the termination of all processes. However, 
		// subsequently sending a signal to any one process does not affect other 
		// processes. Therefore, to resolve this issue, I have initiated an auxiliary 
		// process and sent a SIGINT signal to it in advance.
		std::string process_name = "prepose.exe";

		net::error_code ec;
		std::filesystem::path filepath = app.exe_directory / process_name;

		bp::process process(
			p->ctx.get_executor(),
			bp::filesystem::path(filepath),
			std::initializer_list<bp::string_view>{},
			bp::process_start_dir{ bp::filesystem::path(filepath).parent_path() }/*,
			bp::windows::show_window_normal*/
		);

		process.async_wait([process_name](net::error_code ec, int sig)
		{
			app.logger->debug("{} recvd signal: {} {}", process_name, sig, ec.message());
		});

		co_await net::async_sleep(p->ctx.get_executor(), std::chrono::milliseconds(500),
			net::bind_executor(p->ctx.get_executor(), net::use_nothrow_awaitable));

		for (int i = 0; i < 5; i++)
		{
			if (!is_process_running(std::addressof(process), ec))
				break;
			if (send_signal_to_process(*app.logger, process_name, process))
				break;

			co_await net::async_sleep(p->ctx.get_executor(), std::chrono::milliseconds(100),
				net::bind_executor(p->ctx.get_executor(), net::use_nothrow_awaitable));
		}

		if (is_process_running(std::addressof(process), ec))
		{
			app.logger->info("interrupt {} timeout, terminate it: {}", process_name, ec.message());

			process.terminate(ec);
		}
	}

	net::awaitable<void> start_service(std::shared_ptr<node> p)
	{
		// delay some time to ensure the init log finished.
		co_await net::delay(std::chrono::milliseconds(500));

		co_await runonce_prepose_process(p);

		if (p->cfg.auto_attach_process)
		{
			co_await attach_all_process(p);
		}

		if (p->cfg.auto_start_process)
		{
			co_await start_all_process(p);
		}
	}

	net::awaitable<void> stop_service(std::shared_ptr<node> p)
	{
		p->aborted.test_and_set();

		if (p->cfg.stop_process_when_exit)
		{
			co_await stop_all_process(p);
		}
		else
		{
			for (auto& info : p->cfg.process_list)
			{
				if (!info.process)
					continue;

				static_cast<bp::process*>(info.process.get())->detach();
			}
		}

		net::error_code ec{};
		p->sig.cancel(ec);
	}

	service_process_mgr::service_process_mgr() : imodular()
	{
	}

	bool service_process_mgr::init()
	{
		auto cfgs = app.config->get_service_process_mgr_cfg();

		for (auto& cfg : cfgs)
		{
			if (!cfg.enable)
				continue;

			std::shared_ptr<node> p = std::make_shared<node>();

			p->cfg = std::move(cfg);
			p->aborted.clear();

			for (auto& proc : p->cfg.process_list)
			{
				proc.path = to_canonical_path(app.exe_directory, proc.path).string();
			}

			nodes.emplace_back(std::move(p));
		}

		return true;
	}

	bool service_process_mgr::start()
	{
		for (auto& p : nodes)
		{
			net::co_spawn(p->ctx.get_executor(), wait_signal(p), net::detached);
			net::co_spawn(p->ctx.get_executor(), start_service(p), net::detached);
		}

		append_listener<service_status_event>();
		append_listener<service_start_event>();
		append_listener<service_stop_event>();
		append_listener<service_start_all_event>();
		append_listener<service_stop_all_event>();

		return true;
	}

	void service_process_mgr::stop()
	{
		app.event_dispatcher.remove_listener(typeid(*this).name());

		for (auto& p : nodes)
		{
			net::co_spawn(p->ctx.get_executor(), stop_service(p), net::detached);
		}
		for (auto& p : nodes)
		{
			p->ctx.join();
		}
	}

	void service_process_mgr::uninit()
	{
		nodes.clear();
	}

	template<typename T>
	void service_process_mgr::append_listener()
	{
		app.event_dispatcher.append_listener(typeid(*this).name(), typeid(T),
			[this](std::shared_ptr<ievent> e) mutable
			{
				for (auto& p : nodes)
				{
					// change thread to current io_context
					net::co_spawn(p->ctx.get_executor(),
						handle_event(p, std::static_pointer_cast<T>(e)), net::detached);
				}
			});
	}

	net::awaitable<void> service_process_mgr::handle_event(
		std::shared_ptr<node> p, std::shared_ptr<service_status_event> e)
	{
		try
		{
			int index = 1;
			for (auto& info : p->cfg.process_list)
			{
				net::error_code ec;
				bp::process* proc = static_cast<bp::process*>(info.process.get());

				json item = json::object();
				item["index"] = index++;
				item["name"] = net::locale_to_utf8(info.name);
				item["status"] = is_process_running(proc, ec) ? "running" : "stopped";

				e->data.emplace_back(std::move(item));
			}
		}
		catch (const std::exception& ex)
		{
			e->ec = net::error::invalid_argument;
			e->message = ex.what();
			e->data.clear();

			app.logger->error("handle service_status_event cause an exception: {}", ex.what());
		}

		// change thread to caller io_context
		co_await net::dispatch(net::bind_executor(e->ch.get_executor(), net::use_nothrow_awaitable));
		co_await e->ch.async_send(net::error_code{}, net::use_nothrow_awaitable);
	}
	net::awaitable<void> service_process_mgr::handle_event(
		std::shared_ptr<node> p, std::shared_ptr<service_start_event> e)
	{
		try
		{
			json j = json::parse(e->request_body);

			std::string name = net::utf8_to_locale(j["name"].get<std::string>());

			bool finded = false;

			for (auto& info : p->cfg.process_list)
			{
				if (info.name == name)
				{
					finded = true;
					co_await start_process(p, info);
					break;
				}
			}

			if (!finded)
			{
				e->ec = net::error::invalid_argument;
				e->message = e->ec.message();
				e->data = json::parse(R"({"error":1,"message":"failed"})");

				app.logger->error("request service_start_event can't find the process: {}", name);
			}
		}
		catch (const std::exception& ex)
		{
			e->ec = net::error::invalid_argument;
			e->message = ex.what();
			e->data = json::parse(R"({"error":2,"message":"failed"})");

			app.logger->error("handle service_start_event cause an exception: {}", ex.what());
		}

		// change thread to caller io_context
		co_await net::dispatch(net::bind_executor(e->ch.get_executor(), net::use_nothrow_awaitable));
		co_await e->ch.async_send(net::error_code{}, net::use_nothrow_awaitable);
	}
	net::awaitable<void> service_process_mgr::handle_event(
		std::shared_ptr<node> p, std::shared_ptr<service_stop_event> e)
	{
		try
		{
			json j = json::parse(e->request_body);

			std::string name = net::utf8_to_locale(j["name"].get<std::string>());

			bool finded = false;

			for (auto& info : p->cfg.process_list)
			{
				if (info.name == name)
				{
					finded = true;
					co_await stop_process(p, info);
					break;
				}
			}

			if (!finded)
			{
				e->ec = net::error::invalid_argument;
				e->message = e->ec.message();
				e->data = json::parse(R"({"error":1,"message":"failed"})");

				app.logger->error("request service_stop_event can't find the process: {}", name);
			}
		}
		catch (const std::exception& ex)
		{
			e->ec = net::error::invalid_argument;
			e->message = ex.what();
			e->data = json::parse(R"({"error":2,"message":"failed"})");

			app.logger->error("handle service_stop_event cause an exception: {}", ex.what());
		}

		// change thread to caller io_context
		co_await net::dispatch(net::bind_executor(e->ch.get_executor(), net::use_nothrow_awaitable));
		co_await e->ch.async_send(net::error_code{}, net::use_nothrow_awaitable);
	}
	net::awaitable<void> service_process_mgr::handle_event(
		std::shared_ptr<node> p, std::shared_ptr<service_start_all_event> e)
	{
		try
		{
			co_await start_all_process(p);
		}
		catch (const std::exception& ex)
		{
			e->ec = net::error::invalid_argument;
			e->message = ex.what();
			e->data = json::parse(R"({"error":2,"message":"failed"})");

			app.logger->error("handle service_start_all_event cause an exception: {}", ex.what());
		}

		// change thread to caller io_context
		co_await net::dispatch(net::bind_executor(e->ch.get_executor(), net::use_nothrow_awaitable));
		co_await e->ch.async_send(net::error_code{}, net::use_nothrow_awaitable);
	}
	net::awaitable<void> service_process_mgr::handle_event(
		std::shared_ptr<node> p, std::shared_ptr<service_stop_all_event> e)
	{
		try
		{
			co_await stop_all_process(p);
		}
		catch (const std::exception& ex)
		{
			e->ec = net::error::invalid_argument;
			e->message = ex.what();
			e->data = json::parse(R"({"error":2,"message":"failed"})");

			app.logger->error("handle service_stop_all_event cause an exception: {}", ex.what());
		}

		// change thread to caller io_context
		co_await net::dispatch(net::bind_executor(e->ch.get_executor(), net::use_nothrow_awaitable));
		co_await e->ch.async_send(net::error_code{}, net::use_nothrow_awaitable);
	}
}
