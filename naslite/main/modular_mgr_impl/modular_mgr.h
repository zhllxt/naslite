#pragma once

#include "../../core/net.hpp"
#include "../../core/json.hpp"
#include "../../core/imodular_mgr.hpp"

namespace nas
{
	class modular_mgr_impl final : public imodular_mgr
	{
	public:
		modular_mgr_impl() : imodular_mgr()
		{
		}
		virtual ~modular_mgr_impl()
		{
		}

		virtual bool init() override;

		virtual bool start() override;

		virtual void stop() override;

		virtual void uninit() override;

		virtual void for_each(std::function<void(
			std::string class_name, std::shared_ptr<imodular> modular_ptr)> fun) override;

		virtual std::shared_ptr<imodular> find(const std::string& modular_name) override;

	public:
		std::map<std::string, std::shared_ptr<imodular>> modular_map;
	};
}
