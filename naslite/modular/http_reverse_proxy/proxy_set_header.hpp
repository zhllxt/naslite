#pragma once

#include "builtin_variables.hpp"

namespace nas
{
	void set_proxy_headers(proxy_site_info& site, request_info info)
	{
		builtin_variables& builtin = builtin_variables::instance();

		for (auto& [field_name, field_value] : site.proxy_set_header)
		{
			if (field_value.empty())
				continue;

			std::string_view variables = field_value;

			std::string value;

			int total_vars = 0, failed_vars = 0;

			std::vector<std::string_view> vars;
			std::size_t pos_start = 0, pos_begin = 0, pos_end;
			while ((pos_end = variables.find('$', pos_start)) != std::string::npos)
			{
				std::string_view token = variables.substr(pos_begin, pos_end - pos_begin);
				pos_begin = pos_end;
				pos_start = pos_end + 1;
				vars.emplace_back(std::move(token));
			}
			vars.emplace_back(variables.substr(pos_begin));

			value = vars.front();

			for (std::size_t i = 1; i < vars.size(); ++i)
			{
				std::string_view var = vars[i];

				if (var.starts_with("${"))
				{
					if (auto pos = var.find('}', 1); pos != std::string_view::npos)
					{
						++total_vars;
						std::optional<std::string> os = builtin.get_value(info, var.substr(2, pos - 2));
						if (os)
							value += std::move(os.value());
						else
							++failed_vars;
						value += var.substr(pos + 1);
					}
					else
					{
						value += var;
					}
					continue;
				}

				if (var.front() == '$')
				{
					++total_vars;
					std::optional<std::string> os = builtin.get_value(info, var.substr(1));
					if (os)
						value += std::move(os.value());
					else
						++failed_vars;
					continue;
				}

				assert(false);
				value += var;
			}

			if (failed_vars == 0)
				info.header.set(field_name, std::move(value));
		}
	}
}
