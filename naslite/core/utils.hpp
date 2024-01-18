#pragma once

#include "net.hpp"
#include "json.hpp"
#include "logger.hpp"

#include <cstring>
#include <string>
#include <vector>
#include <exception>
#include <filesystem>

#include <boost/process/v2.hpp>

#include <asio3/core/strutil.hpp>
#include <asio3/core/pfr.hpp>

namespace nas
{
	namespace bp = boost::process::v2;

	namespace
	{
		std::string to_string(std::exception_ptr eptr)
		{
			try
			{
				if (eptr)
					std::rethrow_exception(eptr);
			}
			catch (const std::exception& e)
			{
				return e.what();
			}

			return "null";
		}

		std::filesystem::path to_canonical_path(const std::filesystem::path& root, const std::filesystem::path& sub)
		{
			std::error_code ec{};
			std::filesystem::path p{};

			if (sub.is_absolute())
				p = sub;
			else
				p = root / sub;

			p = std::filesystem::canonical(p, ec);

			return p;
		}
	}

	template<class T>
	std::string get_class_name(const T&)
	{
	#if __has_include(<boost/core/type_name.hpp>)
		std::string name = boost::core::type_name<T>();
	#else
		std::string name = bho::core::type_name<T>();
	#endif

		if (name.starts_with("nas::"))
			name.erase(0, std::strlen("nas::"));

		return name;
	}

	// 0 not found ;
	// other found;
	// process_name "process_name.exe"
	std::vector<bp::pid_type> find_pid_by_name(const std::string& process_name);
	bool send_signal_to_process(spdlog::logger& logger, const std::string& name, bp::process& process);

	json get_hardware_info();
	json get_disk_usage();
	json get_cpu_usage();
	json get_memory_usage();

	int shutdown();
	int restart();

}
