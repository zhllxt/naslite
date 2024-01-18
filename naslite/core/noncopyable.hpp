#pragma once

namespace nas
{
	namespace detail
	{
		class noncopyable
		{
		protected:
			constexpr noncopyable() = default;
			~noncopyable() = default;

			noncopyable(const noncopyable&) = delete;
			noncopyable& operator=(const noncopyable&) = delete;
		};
	}

	using detail::noncopyable;
}
