#pragma once

#include <filesystem>

#include "../../core/net.hpp"
#include "../../core/logger.hpp"
#include "../../core/utils.hpp"
#include "../app.hpp"
#include "../modular_mgr.hpp"
#include "../config.hpp"

#include <asio3/core/program_location.hpp>

namespace nas
{
	namespace fs = std::filesystem;

	class worker
	{
	public:
		worker(int argc, char* argv[])
		{
			parse_args(argc, argv);
		}

		void parse_args(int argc, char* argv[])
		{
			if (argc > 1)
			{
				for (int i = 1; i < argc; ++i)
				{
					args.emplace_back(argv[i]);
				}

				for (std::string& s : args)
				{
					net::to_lower(s);
					net::trim_both(s);
				}

				std::erase_if(args, [](const std::string& sv) { return sv.empty(); });

				for (std::size_t i = 0; i < args.size(); ++i)
				{
					const std::string& s = args[i];

					if (s == "--service" || s == "/service")
					{
						has_service_flag = true;
					}
				}
			}
		}

		static bool init_work_directory()
		{
			// get program location
			try
			{
				fs::path root = net::program_location();

				root = fs::canonical(root);

				root = root.parent_path();

				app.exe_directory = std::move(root);
			}
			catch (const std::exception& e)
			{
				fmt::print("read program location failed: {}\n", e.what());
				return false;
			}

			return true;
		}

		static bool init_logger()
		{
			// create logger
			fs::path filepath = app.exe_directory / "naslite.log";

			try
			{
				auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
				console_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%-5l] %^%v%$");

				auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(filepath.string(), true);
				file_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%-5l] %v");

				std::vector<spdlog::sink_ptr> sinks{ console_sink, file_sink };

				std::shared_ptr<spdlog::logger> logger = std::make_shared<spdlog::logger>(
					"naslite_log", sinks.begin(), sinks.end());
				logger->set_level(spdlog::level::trace);
				logger->flush_on(spdlog::level::err);

				app.logger = std::move(logger);
			}
			catch (const std::exception& e)
			{
				fmt::print("create logger failed: {}\n", e.what());
				return false;
			}

			app.logger->info("create logger successed: {}", filepath.string());

			return true;
		}

		static bool init_config()
		{
			// load config
			app.config = std::make_shared<config_impl>();

			fs::path filepath = app.exe_directory / "naslite.json";

			if (auto result = app.config->load(filepath); !result.has_value())
			{
				app.logger->error("load config from {} failed: {}", filepath.string(), to_string(result.error()));
				return false;
			}

			app.logger->set_level(spdlog::level::from_str(app.config->get_log_level()));

			app.logger->info("load config successed: {}", filepath.string());

			return true;
		}

		int run();

	public:
		std::vector<std::string> args;

		bool has_service_flag = false;
	};
}
