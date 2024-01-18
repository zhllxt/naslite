#pragma once

#include <chrono>
#include <typeindex>

namespace nas
{
	class ievent
	{
	public:
		ievent()
		{
		}
		virtual ~ievent()
		{
		}

		virtual std::type_index get_type()
		{
			return typeid(*this);
		}

		inline auto get_elapsed_time()
		{
			return std::chrono::duration_cast<std::chrono::milliseconds>(
				std::chrono::system_clock::now() - utc).count();
		}

		inline auto get_utc_milliseconds()
		{
			return std::chrono::duration_cast<std::chrono::milliseconds>(
				utc.time_since_epoch()).count();
		}

		std::chrono::system_clock::time_point utc = std::chrono::system_clock::now();
	};
}
