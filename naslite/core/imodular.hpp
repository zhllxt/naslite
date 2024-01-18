#pragma once

#include <string>
#include <memory>
#include <functional>

#include "net.hpp"
#include "json.hpp"

#include <asio3/core/pfr.hpp>

namespace nas
{
	class imodular
	{
	public:
		imodular()
		{
			jconfig["name"] = "";
			jconfig["enable"] = false;
		}
		virtual ~imodular()
		{
		}

		virtual bool init() = 0;

		virtual bool start() = 0;

		virtual void stop() = 0;

		virtual void uninit() = 0;

		inline std::string get_name()
		{
			return jconfig["name"];
		}

		inline bool is_enabled() const noexcept
		{
			return jconfig["enable"];
		}

		inline void set_enabled(bool v) noexcept
		{
			jconfig["enable"] = v;
		}

	public:
		json jconfig = json::object();
	};
}
