#include "worker.h"

#include <asio3/core/io_context_thread.hpp>
#include <asio3/core/netutil.hpp>
#include <asio3/core/predef.h>

#if ASIO3_OS_LINUX || ASIO3_OS_UNIX

namespace nas
{
	int worker::run()
	{
		if (!worker::init_app())
			return -1;

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

			app.modular->stop();
		});
		ctx.run();

		app.modular->uninit();

		app.logger->info("naslite exited successed");

		return 0;
	}
}

#endif
