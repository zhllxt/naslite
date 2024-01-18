#pragma once

#include "../../core/net.hpp"
#include "../../core/json.hpp"

#include "../../core/ievent.hpp"

namespace nas
{
	class service_start_event : public ievent
	{
	public:
		service_start_event(const auto& executor) : ievent(), ch(executor, 1)
		{
		}
		virtual ~service_start_event()
		{
		}

		virtual std::type_index get_type()
		{
			return typeid(*this);
		}

	public:
		net::experimental::channel<void(net::error_code)> ch;

		json data{ json::parse(R"({"error":0,"message":"success"})") };

		net::error_code ec{};

		std::string message{ "success" };

		std::string request_body{};
	};
}
