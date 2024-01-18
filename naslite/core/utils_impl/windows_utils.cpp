#include "../utils.hpp"

#include <cstdlib>

#include <asio3/core/predef.h>

#if ASIO3_OS_WINDOWS

#include <Windows.h>
#include <tlhelp32.h>
#include <tchar.h>
#include <stdio.h>

#include <windows-kill-master/windows-kill-library/windows-kill-library.h>

namespace nas
{
	namespace fs = std::filesystem;

	// https://learn.microsoft.com/zh-cn/windows/win32/toolhelp/taking-a-snapshot-and-viewing-processes
	std::vector<bp::pid_type> find_pid_by_name(const std::string& process_name)
	{
		std::vector<bp::pid_type> pids;

		HANDLE hProcessSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
		if (hProcessSnap == INVALID_HANDLE_VALUE)
		{
			return pids;
		}

		PROCESSENTRY32 pe32;
		pe32.dwSize = sizeof(PROCESSENTRY32);
		if (!Process32First(hProcessSnap, &pe32))
		{
			// clean the snapshot object
			CloseHandle(hProcessSnap);
			return pids;
		}

		do
		{
			if (process_name == pe32.szExeFile)
			{
				bp::pid_type processId = pe32.th32ProcessID;
				pids.emplace_back(processId);
			}
		} while (Process32Next(hProcessSnap, &pe32));

		CloseHandle(hProcessSnap);

		return pids;
	}

	bool send_signal_to_process(spdlog::logger& logger, const std::string& name, bp::process& process)
	{
		try
		{
			WindowsKillLibrary::sendSignal(process.id(), WindowsKillLibrary::SIGNAL_TYPE_CTRL_C);

			logger.debug("sent signal successfuly: {} {}", name, process.id());

			return true;
		}
		catch (const std::invalid_argument& e)
		{
			if (std::strcmp(e.what(), "ESRCH") == 0)
			{
				logger.error("sent signal failed, Pid dosen't exist: {} {}", name, process.id());
			}
			else if (strcmp(e.what(), "EINVAL") == 0)
			{
				logger.error("sent signal failed, Invalid signal type: {} {}", name, process.id());
			}
			else
			{
				logger.error("sent signal failed: {} {} InvalidArgument: {}", name, process.id(), e.what());
			}
		}
		catch (const std::system_error& e)
		{
			logger.error("sent signal failed: {} {} SystemError: {}", name, process.id(), e.what());
		}
		catch (const std::runtime_error& e)
		{
			if (std::strcmp(e.what(), "EPERM") == 0)
			{
				logger.error("sent signal failed, Not enough permission: {} {}", name, process.id());
			}
			else
			{
				logger.error("sent signal failed: {} {} RuntimeError: {}", name, process.id(), e.what());
			}
		}
		catch (const std::exception& e)
		{
			logger.error("sent signal failed: {} {} Exception: {}", name, process.id(), e.what());
		}

		return false;
	}

	std::int64_t DiffFileTime(FILETIME time1, FILETIME time2)
	{
		std::int64_t a = (std::int64_t(time1.dwHighDateTime) << 32) | time1.dwLowDateTime;
		std::int64_t b = (std::int64_t(time2.dwHighDateTime) << 32) | time2.dwLowDateTime;
		return b - a;
	}

	// https://blog.csdn.net/ilyhlf5201314/article/details/8078267
	std::string GetCpuInfo()
	{
		std::string strCPU = "Unknown";

		HKEY hKey;
		constexpr int BUFSIZE = 0xff;
		char szCPUInfo[BUFSIZE]{ 0 };
		DWORD dwBufLen = BUFSIZE;

		LONG lRet = RegOpenKeyExA(HKEY_LOCAL_MACHINE,
			"HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0",
			0, KEY_QUERY_VALUE, &hKey);
		if (lRet == ERROR_SUCCESS)
		{
			lRet = RegQueryValueExA(hKey, "ProcessorNameString", NULL, NULL, (LPBYTE)szCPUInfo, &dwBufLen);
			if (lRet == ERROR_SUCCESS)
			{
				strCPU = szCPUInfo;
			}
		}
		RegCloseKey(hKey);

		return strCPU;
	}

	json get_hardware_info()
	{
		json j = json::object();

		std::uintmax_t diskTotal = 0;
		std::string sDiskTotal;
		try
		{
			CHAR sDrivesName[0x100];
			DWORD dwDriveStrLen = sizeof(sDrivesName);
			GetLogicalDriveStringsA(dwDriveStrLen, sDrivesName);
			for (CHAR* p = (CHAR*)sDrivesName; *p;)
			{
				int DType = GetDriveTypeA(p);
				if (DType == DRIVE_FIXED)
				{
					fs::space_info devd = fs::space(fs::path(p).root_path());
					diskTotal += devd.capacity;
				}
				p += (std::strlen(p) + 1);
			}
			if (diskTotal > 1099511627776ull)
				sDiskTotal = fmt::format("{:.1f}T", (long double)(diskTotal) / (long double)(1099511627776ull));
			else
				sDiskTotal = fmt::format("{:.1f}G", (long double)(diskTotal) / (long double)(1073741824ull));
		}
		catch (...) {}

		DWORDLONG ullTotalPhys = 0;
		std::string sTotalPhys;
		try
		{
			MEMORYSTATUSEX statex;
			statex.dwLength = sizeof(statex);
			GlobalMemoryStatusEx(&statex);
			ullTotalPhys = statex.ullTotalPhys;

			sTotalPhys = fmt::format("{}G", std::uint32_t(std::round((long double)(ullTotalPhys) / (long double)(1073741824ull))));
		}
		catch (...) {}

		j["cpu"] = GetCpuInfo();
		j["memory"] = sTotalPhys;
		j["disk"] = sDiskTotal;

		return j;
	}

	// https://blog.csdn.net/HQY_0306/article/details/105414389
	json get_disk_usage()
	{
		json j = json::array();

		try
		{
			CHAR sDrivesName[0x100];
			DWORD dwDriveStrLen = sizeof(sDrivesName);
			GetLogicalDriveStringsA(dwDriveStrLen, sDrivesName);
			for (CHAR* p = (CHAR*)sDrivesName; *p;)
			{
				int DType = GetDriveTypeA(p);
				if (DType == DRIVE_FIXED)
				{
					fs::space_info devd = fs::space(fs::path(p).root_path());
					float diskuse = float(devd.capacity - devd.free) / devd.capacity;
					json o = json::object();
					o["name"] = p;
					o["total"] = std::to_string(devd.capacity);
					o["used"] = std::to_string(devd.capacity - devd.free);
					o["percentage"] = std::to_string(int(diskuse * 100.0f));
					j.emplace_back(std::move(o));
				}
				p += (std::strlen(p) + 1);
			}
		}
		catch (...) {}

		return j;
	}

	json get_cpu_usage()
	{
		json j = json::object();
		j["percentage"] = "0";

		try
		{
			static FILETIME idleTime1{};
			static FILETIME kernelTime1{};
			static FILETIME userTime1{};

			if (idleTime1.dwHighDateTime == 0 && idleTime1.dwLowDateTime == 0)
			{
				GetSystemTimes(&idleTime1, &kernelTime1, &userTime1);
				return j;
			}

			FILETIME  idleTime2;
			FILETIME  kernelTime2;
			FILETIME  userTime2;

			GetSystemTimes(&idleTime2, &kernelTime2, &userTime2);

			std::int64_t idle   = DiffFileTime(idleTime1, idleTime2);
			std::int64_t kernel = DiffFileTime(kernelTime1, kernelTime2);
			std::int64_t user   = DiffFileTime(userTime1, userTime2);

			idleTime1 = idleTime2;
			kernelTime1 = kernelTime2;
			userTime1 = userTime2;

			float cpuuse;
			if (kernel + user == 0)
				cpuuse = 0.f;
			else
				cpuuse = std::fabs(float(kernel + user - idle) / float(kernel + user));

			j["percentage"] = std::to_string(int(cpuuse * 100.0f));
		}
		catch (...) {}

		return j;
	}

	json get_memory_usage()
	{
		json j = json::object();
		j["percentage"] = "0";

		try
		{
			MEMORYSTATUSEX statex;
			statex.dwLength = sizeof(statex);
			GlobalMemoryStatusEx(&statex);
			float memuse = statex.dwMemoryLoad / 100.f;

			j["percentage"] = std::to_string(int(memuse * 100.0f));
		}
		catch (...) {}

		return j;
	}

	int shutdown()
	{
		return std::system("shutdown -s -t 3");
	}
	int restart()
	{
		return std::system("shutdown -r -t 3");
	}
}

#endif
