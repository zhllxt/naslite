#pragma once

#include "../../core/net.hpp"
#include "../../core/json.hpp"

#include "../../core/ievent.hpp"

namespace nas
{
	class service_status_event : public ievent
	{
	public:
		service_status_event(const auto& executor) : ievent(), ch(executor, 1)
		{
		}
		virtual ~service_status_event()
		{
		}

		virtual std::type_index get_type()
		{
			return typeid(*this);
		}

	public:
		net::experimental::channel<void(net::error_code)> ch;

		json data{ json::array() };

		net::error_code ec{};

		std::string message{ "success" };
	};
}
