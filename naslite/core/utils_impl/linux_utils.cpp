#include "../utils.hpp"

#include <cstdlib>

#include <asio3/core/predef.h>

#if ASIO3_OS_LINUX || ASIO3_OS_UNIX

namespace nas
{
	namespace fs = std::filesystem;

	std::vector<bp::pid_type> find_pid_by_name(const std::string& process_name)
	{
		std::vector<bp::pid_type> pids;

		//HANDLE hProcessSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
		//if (hProcessSnap == INVALID_HANDLE_VALUE)
		//{
		//	return pids;
		//}

		//PROCESSENTRY32 pe32;
		//pe32.dwSize = sizeof(PROCESSENTRY32);
		//if (!Process32First(hProcessSnap, &pe32))
		//{
		//	// clean the snapshot object
		//	CloseHandle(hProcessSnap);
		//	return pids;
		//}

		//do
		//{
		//	if (process_name == pe32.szExeFile)
		//	{
		//		bp::pid_type processId = pe32.th32ProcessID;
		//		pids.emplace_back(processId);
		//	}
		//} while (Process32Next(hProcessSnap, &pe32));

		//CloseHandle(hProcessSnap);

		return pids;
	}

	bool send_signal_to_process(spdlog::logger& logger, const std::string& name, bp::process& process)
	{
		//try
		//{
		//	WindowsKillLibrary::sendSignal(process.id(), WindowsKillLibrary::SIGNAL_TYPE_CTRL_C);

		//	logger.debug("sent signal successfuly: {} {}", name, process.id());

		//	return true;
		//}
		//catch (const std::invalid_argument& e)
		//{
		//	if (std::strcmp(e.what(), "ESRCH") == 0)
		//	{
		//		logger.error("sent signal failed, Pid dosen't exist: {} {}", name, process.id());
		//	}
		//	else if (strcmp(e.what(), "EINVAL") == 0)
		//	{
		//		logger.error("sent signal failed, Invalid signal type: {} {}", name, process.id());
		//	}
		//	else
		//	{
		//		logger.error("sent signal failed: {} {} InvalidArgument: {}", name, process.id(), e.what());
		//	}
		//}
		//catch (const std::system_error& e)
		//{
		//	logger.error("sent signal failed: {} {} SystemError: {}", name, process.id(), e.what());
		//}
		//catch (const std::runtime_error& e)
		//{
		//	if (std::strcmp(e.what(), "EPERM") == 0)
		//	{
		//		logger.error("sent signal failed, Not enough permission: {} {}", name, process.id());
		//	}
		//	else
		//	{
		//		logger.error("sent signal failed: {} {} RuntimeError: {}", name, process.id(), e.what());
		//	}
		//}
		//catch (const std::exception& e)
		//{
		//	logger.error("sent signal failed: {} {} Exception: {}", name, process.id(), e.what());
		//}

		return false;
	}

	json get_hardware_info()
	{
		json j = json::object();
		j["cpu"] = "";
		j["memory"] = "";
		j["disk"] = "";
		return j;
	}

	json get_disk_usage()
	{
		json j = json::array();
		json o = json::object();
		o["name"] = "/dev/sda1";
		o["total"] = "0";
		o["used"] = "0";
		o["percentage"] = "0";
		j.emplace_back(std::move(o));
		return j;
	}

	json get_cpu_usage()
	{
		json j = json::object();
		j["percentage"] = "0";
		return j;
	}

	json get_memory_usage()
	{
		json j = json::object();
		j["percentage"] = "0";
		return j;
	}

	int shutdown()
	{
		return std::system("shutdown -h -t 3");
	}
	int restart()
	{
		return std::system("shutdown -r -t 3");
	}
}

#endif
