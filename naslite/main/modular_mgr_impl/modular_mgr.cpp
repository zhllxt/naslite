#include "modular_mgr.h"

#include "../app.hpp"

namespace nas
{
	bool modular_mgr_impl::init()
	{
		pfr::class_factory<imodular>& factory = pfr::class_factory<imodular>::instance();

		factory.for_each([this](std::string name, const auto& func)
		{
			if (name.starts_with("nas::"))
				name.erase(0, std::strlen("nas::"));

			modular_map.emplace(name, std::shared_ptr<imodular>(func()));
		});

		for (auto& [class_name, modular_ptr] : modular_map)
		{
			// todo: module enable/disable config.
			modular_ptr->set_enabled(true);
		}

		app.logger->info("total {} modules has loaded:", modular_map.size());

		for (auto& [class_name, modular_ptr] : modular_map)
		{
			app.logger->info("  - {:5} {}", modular_ptr->is_enabled(), class_name);
		}

		app.logger->info("# begin init modules");

		for (auto& [class_name, modular_ptr] : modular_map)
		{
			app.logger->info("  - begin init the module: {}", class_name);
			modular_ptr->init();
			app.logger->info("    init the module '{}' finished", class_name);
		}

		return true;
	}

	bool modular_mgr_impl::start()
	{
		app.logger->info("# begin start modules");

		for (auto& [class_name, modular_ptr] : modular_map)
		{
			app.logger->info("  - begin start the module: {}", class_name);
			modular_ptr->start();
			app.logger->info("    start the module '{}' finished", class_name);
		}

		return true;
	}

	void modular_mgr_impl::stop()
	{
		app.logger->info("# begin stop modules");

		for (auto& [class_name, modular_ptr] : modular_map)
		{
			app.logger->info("  - begin stop the module: {}", class_name);
			modular_ptr->stop();
			app.logger->info("    stop the module '{}' finished", class_name);
		}
	}

	void modular_mgr_impl::uninit()
	{
		app.logger->info("# begin uninit modules");

		for (auto& [class_name, modular_ptr] : modular_map)
		{
			app.logger->info("  - begin uninit the module: {}", class_name);
			modular_ptr->uninit();
			app.logger->info("    uninit the module '{}' finished", class_name);
		}

		modular_map.clear();
	}

	void modular_mgr_impl::for_each(std::function<void(
		std::string class_name, std::shared_ptr<imodular> modular_ptr)> fun)
	{
		for (auto& [class_name, modular_ptr] : modular_map)
		{
			fun(class_name, modular_ptr);
		}
	}

	std::shared_ptr<imodular> modular_mgr_impl::find(const std::string& modular_name)
	{
		for (auto& [class_name, modular_ptr] : modular_map)
		{
			if (modular_ptr->get_name() == modular_name)
				return modular_ptr;
		}

		return nullptr;
	}
}
