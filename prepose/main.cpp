#include <asio3/core/asio.hpp>

#ifdef ASIO_STANDALONE
namespace net = ::asio;
#else
namespace net = boost::asio;
#endif

int main()
{
	net::io_context ctx;

	net::signal_set sigset(ctx.get_executor(), SIGINT);
	sigset.async_wait([](net::error_code, int)
	{
	});

	ctx.run();
}
