#pragma once

#include "imodular.hpp"

namespace nas
{
	class imodular_mgr
	{
	public:
		imodular_mgr()
		{
		}
		virtual ~imodular_mgr()
		{
		}

		virtual bool init() = 0;

		virtual bool start() = 0;

		virtual void stop() = 0;

		virtual void uninit() = 0;

		virtual void for_each(std::function<void(std::string, std::shared_ptr<imodular>)> fun) = 0;

		virtual std::shared_ptr<imodular> find(const std::string& modular_name) = 0;
	};
}
