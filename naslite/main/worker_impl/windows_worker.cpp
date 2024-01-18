#include "worker.h"

#include <asio3/core/io_context_thread.hpp>
#include <asio3/core/netutil.hpp>
#include <asio3/core/predef.h>

#include "../restart_naslite_event.hpp"

#if ASIO3_OS_WINDOWS

#include <Windows.h>
#include <tlhelp32.h>
#include <tchar.h>
#include <stdio.h>

std::shared_ptr<net::io_context_thread> m_context_thread;
std::shared_ptr<net::signal_set>        m_signal_set;
std::atomic_flag                        m_aborted;

net::awaitable<void> wait_signal()
{
	co_await net::dispatch(m_context_thread->get_executor(), net::use_nothrow_awaitable);

	m_signal_set = std::make_shared<net::signal_set>(m_context_thread->get_executor(), SIGINT);

	for (; !m_aborted.test();)
	{
		auto [e1, n1] = co_await m_signal_set->async_wait(net::use_nothrow_awaitable);

		app.logger->debug("main signal callback triggered: {} {}", n1, e1.message());
	}

	app.logger->debug("main signal callback exited due to windows service stopped");
}

void create_signal()
{
	m_aborted.clear();
	m_context_thread = std::make_shared<net::io_context_thread>();
	net::co_spawn(m_context_thread->get_executor(), wait_signal(), net::detached);
}

void destroy_signal()
{
	net::error_code ec;

	m_aborted.test_and_set();

	if (m_signal_set)
		m_signal_set->cancel(ec);
	if (m_context_thread)
		m_context_thread->join();

	m_signal_set.reset();
	m_context_thread.reset();
}

net::awaitable<void> handle_event(std::shared_ptr<nas::restart_naslite_event> e)
{
	auto ex = co_await net::this_coro::executor;

	// send notify to the channel first, can't call modular_mgr stop functions
	// first, it will cause deadlock.
	// change thread to caller io_context
	co_await net::dispatch(e->ch.get_executor(), net::use_nothrow_awaitable);
	co_await e->ch.async_send(net::error_code{}, net::use_nothrow_awaitable);

	// change thread to current io_context
	co_await net::dispatch(ex, net::use_nothrow_awaitable);

	app.logger->info("prepare restart naslite ......");

	// must call modular_mgr functions in current thread, can't in caller io_context,
	// it will cause deadlock.
	app.modular->stop();
	app.modular->uninit();
	nas::worker::init_config();
	app.modular->init();
	app.modular->start();

	app.logger->info("restart naslite finished");
}

BOOL EnableDebugPrivilege()
{
	BOOL bRet = FALSE;
	HANDLE hToken;
	if (OpenProcessToken(::GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES, &hToken))
	{
		TOKEN_PRIVILEGES tkp;
		if (LookupPrivilegeValue(NULL, SE_DEBUG_NAME, &tkp.Privileges[0].Luid))
		{
			tkp.PrivilegeCount = 1;
			tkp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
			if (AdjustTokenPrivileges(hToken, FALSE, &tkp, sizeof tkp, NULL, NULL))
			{
				bRet = TRUE;
			}
		}
		CloseHandle(hToken);
	}
	return bRet;
}

LPCTSTR               ServiceName = _T("naslite");
SERVICE_STATUS        ServiceStatus;
SERVICE_STATUS_HANDLE ServiceStatusHandle;

VOID WINAPI ServiceHandler(DWORD dwControl)
{
	net::error_code ec;

	switch (dwControl)
	{
	case SERVICE_CONTROL_STOP:
	case SERVICE_CONTROL_SHUTDOWN:
		ServiceStatus.dwWin32ExitCode = 0;
		ServiceStatus.dwCurrentState = SERVICE_STOPPED;
		ServiceStatus.dwCheckPoint = 0;
		ServiceStatus.dwWaitHint = 0;

		//add you quit code here
		app.logger->info("windows service prepare exiting......");

		app.modular->stop();

		destroy_signal();

		app.modular->uninit();

		app.event_dispatcher.remove_listener(typeid(nas::worker).name());

		if (FreeConsole())
			app.logger->info("FreeConsole successed");
		else
			app.logger->error("FreeConsole failed: {}", ::GetLastError());

		app.logger->info("windows service exited successed");

		break;
	default:
		return;
	};

	if (!SetServiceStatus(ServiceStatusHandle, &ServiceStatus))
	{
		app.logger->error("ServiceHandler::SetServiceStatus failed: {}", ::GetLastError());
	}
}

VOID WINAPI ServiceMain(DWORD dwNumServicesArgs, LPTSTR* lpServiceArgVectors)
{
	net::ignore_unused(dwNumServicesArgs, lpServiceArgVectors);

	if (!nas::worker::init_work_directory())
		return;
	if (!nas::worker::init_logger())
		return;
	if (!nas::worker::init_config())
		return;

	// create modular management
	app.modular = std::make_shared<nas::modular_mgr_impl>();

	app.logger->info("windows service prepare starting......");
	app.logger->info("current work directory is: {}", app.exe_directory.string());

	ServiceStatus.dwServiceType = SERVICE_WIN32;
	ServiceStatus.dwCurrentState = SERVICE_START_PENDING;
	ServiceStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN | SERVICE_ACCEPT_PAUSE_CONTINUE;
	ServiceStatus.dwWin32ExitCode = 0;
	ServiceStatus.dwServiceSpecificExitCode = 0;
	ServiceStatus.dwCheckPoint = 0;
	ServiceStatus.dwWaitHint = 0;
	ServiceStatusHandle = RegisterServiceCtrlHandler(ServiceName, ServiceHandler);

	if (ServiceStatusHandle == 0)
	{
		app.logger->error("RegisterServiceCtrlHandler failed: {}", ::GetLastError());
		return;
	}

	//add your init code here
	// http://www.cppblog.com/cyt/archive/2008/01/08/40723.html
	if (AllocConsole())
		app.logger->info("AllocConsole successed");
	else
		app.logger->error("AllocConsole failed: {}", ::GetLastError());

	create_signal();

	app.event_dispatcher.append_listener(typeid(nas::worker).name(), typeid(nas::restart_naslite_event),
	[](std::shared_ptr<nas::ievent> e) mutable
	{
		// change thread to current io_context
		net::co_spawn(m_context_thread->get_executor(), handle_event(
			std::static_pointer_cast<nas::restart_naslite_event>(std::move(e))), net::detached);
	});

	if (!app.modular->init())
	{
		destroy_signal();
		return;
	}

	if (!app.modular->start())
	{
		destroy_signal();
		return;
	}

	app.logger->info("windows service started successed");

    //add your service thread here

	// Initialization complete - report running status 
	ServiceStatus.dwCurrentState = SERVICE_RUNNING;
	ServiceStatus.dwCheckPoint = 0;
	ServiceStatus.dwWaitHint = 9000;
	if (!SetServiceStatus(ServiceStatusHandle, &ServiceStatus))
	{
		app.logger->error("ServiceMain::SetServiceStatus failed: {}", ::GetLastError());
		return;
	}
}

namespace nas
{
	int worker::run()
	{
		EnableDebugPrivilege();

		if (has_service_flag)
		{
			SERVICE_TABLE_ENTRY ServiceTable[2];

			ServiceTable[0].lpServiceName = const_cast<LPTSTR>(ServiceName);
			ServiceTable[0].lpServiceProc = (LPSERVICE_MAIN_FUNCTION)ServiceMain;

			ServiceTable[1].lpServiceName = NULL;
			ServiceTable[1].lpServiceProc = NULL;

			return StartServiceCtrlDispatcher(ServiceTable);
		}
		else
		{
			if (!worker::init_work_directory())
				return -1;
			if (!worker::init_logger())
				return -1;
			if (!worker::init_config())
				return -1;

			// create modular management
			app.modular = std::make_shared<modular_mgr_impl>();

			app.logger->info("naslite prepare starting......");
			app.logger->info("current work directory is: {}", app.exe_directory.string());

			if (!app.modular->init())
				return -1;

			if (!app.modular->start())
				return -1;

			app.logger->info("naslite started successed");

			net::io_context ctx(1);
			net::signal_set sigset(ctx.get_executor(), SIGINT);
			sigset.async_wait([](net::error_code, int)
			{
				app.logger->info("naslite prepare exiting......");

				// beacuse the ctx run is called in the main thread, so this code
				// is running in the main thread too.
				app.modular->stop();
			});
			app.event_dispatcher.append_listener(typeid(nas::worker).name(), typeid(nas::restart_naslite_event),
			[this, &ctx](std::shared_ptr<nas::ievent> e) mutable
			{
				// change thread to current io_context
				net::co_spawn(ctx.get_executor(), handle_event(
					std::static_pointer_cast<restart_naslite_event>(std::move(e))), net::detached);
			});
			ctx.run();

			app.modular->uninit();

			app.logger->info("naslite exited successed");

			app.event_dispatcher.remove_listener(typeid(nas::worker).name());

			return 0;
		}
	}
}

#endif
