#pragma once

#define SPDLOG_LEVEL_NAMES { "trace", "debug", "info", "warn", "error", "fatal", "off" }

#include "../core/net.hpp"
#include "../core/logger.hpp"
#include "../core/noncopyable.hpp"
#include "../core/json.hpp"
#include "../core/ievent.hpp"
#include "../core/imodular_mgr.hpp"
#include "../core/iconfig.hpp"

#include <asio3/core/event_dispatcher.hpp>

namespace nas
{
	class application : private noncopyable
	{
	private:
		application()
		{
		}
		~application()
		{
		}

		struct event_policy
		{
			static std::type_index get_event(const std::shared_ptr<ievent>& e)
			{
				return e->get_type();
			}

			using user_data_t = std::int32_t;
		};

	public:
		using event_dispatcher_type = asio::event_dispatcher<
			std::type_index, void(std::shared_ptr<ievent>), event_policy>;

		static application& instance() { static application g; return g; }

	public:
		std::string                       version = "1.0";

		std::filesystem::path             exe_directory{};

		event_dispatcher_type             event_dispatcher{};

		std::shared_ptr<iconfig>          config{};

		std::shared_ptr<spdlog::logger>   logger{};

		std::shared_ptr<imodular_mgr>     modular{};
	};
}

inline nas::application& app = nas::application::instance();
