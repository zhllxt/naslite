#include "../core/net.hpp"
#include "../core/json.hpp"
#include "../core/dump.hpp"

#include "worker.hpp"

int main(int argc, char* argv[])
{
	InstallDumpHandler();

	return nas::worker(argc, argv).run();
}
