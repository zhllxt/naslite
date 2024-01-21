#include "config.h"

#include "../app.hpp"

#include <asio3/core/codecvt.hpp>
#include <asio3/core/strutil.hpp>

namespace nas
{
	std::chrono::system_clock::time_point config_impl::to_system_clock_time(const std::string& str)
	{
		std::tm t = {};
		std::istringstream ss(str);
		ss >> std::get_time(&t, "%Y-%m-%d %H:%M:%S");

		if (ss.fail())
		{
			app.logger->error("the date time format of '{}' is invalid, the correctly format is like '2024-01-04 12:30:45'", str);
			return std::chrono::system_clock::now();
		}

		return std::chrono::system_clock::from_time_t(std::mktime(&t));
	}

	std::expected<bool, std::exception_ptr> config_impl::load(std::filesystem::path filepath)
	{
		try
		{
			std::unique_lock g(m_mutex);

			m_filepath = std::move(filepath);

			m_jconfig.clear();

			// read a JSON file
			std::ifstream i(m_filepath);
			if (!i)
				throw std::runtime_error("open the file for read failed");
			i >> m_jconfig;

			return true;
		}
		catch (...)
		{
			return std::unexpected(std::current_exception());
		}
	}

	void config_impl::do_save()
	{
		// write prettified JSON to another file
		std::ofstream o(m_filepath);
		o << std::setw(2) << m_jconfig << std::endl;
	}

	std::expected<bool, std::exception_ptr> config_impl::save()
	{
		try
		{
			std::unique_lock g(m_mutex);

			do_save();

			return true;
		}
		catch (...)
		{
			return std::unexpected(std::current_exception());
		}
	}

	std::string config_impl::get_log_level()
	{
		std::shared_lock g(m_mutex);

		if (json& j = m_jconfig["log_level"]; j.is_string())
			return j.get<std::string>();

		return "trace";
	}

	const json& config_impl::get_modular_json(std::string_view modular_name)
	{
		std::shared_lock g(m_mutex);

		return m_jconfig[modular_name];
	}

	bool config_impl::set_modular_json(std::string_view modular_name, const std::string& value)
	{
		std::unique_lock g(m_mutex);

		try
		{
			json v = json::parse(value);

			if (json& j = m_jconfig[modular_name]; !j.empty())
			{
				if /**/ (j.is_object())
				{
					if (v.is_object())
					{
						j = std::move(v);
						do_save();
						return true;
					}
				}
				else if (j.is_array())
				{
					if /**/ (v.is_object())
					{
						j.front() = std::move(v);
						do_save();
						return true;
					}
					else if (v.is_array())
					{
						j = std::move(v);
						do_save();
						return true;
					}
				}
			}
		}
		catch (const std::exception& e)
		{
			app.logger->error("config::set_modular_json failed: {}", e.what());
		}

		return false;
	}

	std::vector<static_http_server_info> config_impl::get_http_server_cfg()
	{
		std::shared_lock g(m_mutex);

		std::vector<static_http_server_info> cfgs;

		json& jm = m_jconfig["static_http_server"];

		if (jm.empty() || !jm.is_array())
		{
			app.logger->error("can't find the '{}' modular config", "static_http_server");
			return cfgs;
		}

		try
		{
			for (json& j : jm)
			{
				std::unordered_map<std::string, token_info> tokens;
				for (json& jtoken : j["tokens"])
				{
					tokens.emplace(jtoken["username"], token_info{
							.username = jtoken["username"],
							.password = jtoken["password"],
							.expires_at = to_system_clock_time(jtoken["expires_at"]),
						});
				}
				cfgs.emplace_back(static_http_server_info{
						.enable = j["enable"],
						.protocol = j["protocol"],
						.name = net::utf8_to_locale(j["name"].get<std::string>()),
						.listen_address = j["listen_address"],
						.listen_port = std::uint16_t(std::stoi(j["listen_port"].get<std::string>())),
						.webroot = net::utf8_to_locale(j["webroot"].get<std::string>()),
						.index = j["index"],
						.cert_file = net::utf8_to_locale(j["cert_file"].get<std::string>()),
						.key_file = net::utf8_to_locale(j["key_file"].get<std::string>()),
						.max_request_header_size = std::stoul(j["max_request_header_size"].get<std::string>()),
						.enable_cors = j["enable_cors"],
						.requires_auth = j["requires_auth"],
						.tokens = std::move(tokens),
					});
			}
		}
		catch (const std::exception& e)
		{
			app.logger->error("read config from '{}' modular failed: {}", "static_http_server", e.what());
		}

		return cfgs;
	}

	std::vector<http_reverse_proxy_info> config_impl::get_http_reverse_proxy_cfg()
	{
		std::shared_lock g(m_mutex);

		std::vector<http_reverse_proxy_info> cfgs;

		json& jm = m_jconfig["http_reverse_proxy"];

		if (jm.empty() || !jm.is_array())
		{
			app.logger->error("can't find the '{}' modular config", "http_reverse_proxy");
			return cfgs;
		}

		try
		{
			for (json& j : jm)
			{
				std::unordered_map<std::string, proxy_site_info> proxy_sites;
				for (json& jsite : j["proxy_sites"])
				{
					std::vector<proxy_auth_role> auth_roles;
					for (json& jrole : jsite["auth_roles"])
					{
						auth_roles.emplace_back(proxy_auth_role{
							.method = http::string_to_verb(jrole["method"].get<std::string>()),
							.target = jrole["target"],
							.result = std::stoul(jrole["result"].get<std::string>()),
							});
					}
					std::map<std::string, std::string> proxy_set_header;
					std::map<std::string, std::string> proxy_options;
					if (std::string options = jsite["proxy_options"]; !options.empty())
					{
						std::vector<std::string> rows = net::split(options, '\n');
						for (std::string& row : rows)
						{
							net::trim_both(row);
						}
						std::erase_if(rows, [](std::string& row) { return row.empty(); });
						for (std::string& row : rows)
						{
							std::vector<std::string> kvs = net::split(row, ' ');
							for (std::string& kv : kvs)
							{
								net::trim_both(kv);
								while (!kv.empty() && kv.back() == ';')
									kv.pop_back();
							}
							std::erase_if(kvs, [](std::string& kv) { return kv.empty(); });
							if (kvs.size() == 3 && net::iequals(kvs.front(), "proxy_set_header"))
							{
								proxy_set_header.emplace(std::move(kvs[1]), std::move(kvs[2]));
							}
							else if (kvs.size() == 2)
							{
								proxy_options.emplace(std::move(kvs[0]), std::move(kvs[1]));
							}
						}
					}
					proxy_sites.emplace(jsite["domain"], proxy_site_info{
							.name = net::utf8_to_locale(jsite["name"].get<std::string>()),
							.domain = jsite["domain"],
							.host = jsite["host"],
							.port = std::uint16_t(std::stoi(jsite["port"].get<std::string>())),
							.skip_body_for_head_request = jsite["skip_body_for_head_request"],
							.skip_body_for_head_response = jsite["skip_body_for_head_response"],
							.requires_auth = jsite["requires_auth"],
							.auth_roles = std::move(auth_roles),
							.proxy_set_header = std::move(proxy_set_header),
							.proxy_options = std::move(proxy_options),
						});
				}
				cfgs.emplace_back(http_reverse_proxy_info{
						.enable = j["enable"],
						.protocol = j["protocol"],
						.name = net::utf8_to_locale(j["name"].get<std::string>()),
						.ip_blacklist_minutes = std::stoul(j["ip_blacklist_minutes"].get<std::string>()),
						.listen_address = j["listen_address"],
						.listen_port = std::uint16_t(std::stoi(j["listen_port"].get<std::string>())),
						.cert_file = net::utf8_to_locale(j["cert_file"].get<std::string>()),
						.key_file = net::utf8_to_locale(j["key_file"].get<std::string>()),
						.proxy_sites = std::move(proxy_sites),
					});
			}
		}
		catch (const std::exception& e)
		{
			app.logger->error("read config from '{}' modular failed: {}", "http_reverse_proxy", e.what());
		}

		return cfgs;
	}

	std::vector<socks5_reverse_proxy_info> config_impl::get_socks5_reverse_proxy_cfg()
	{
		std::shared_lock g(m_mutex);

		std::vector<socks5_reverse_proxy_info> cfgs;

		json& jm = m_jconfig["socks5_reverse_proxy"];

		if (jm.empty() || !jm.is_array())
		{
			app.logger->error("can't find the '{}' modular config", "socks5_reverse_proxy");
			return cfgs;
		}

		try
		{
			for (json& j : jm)
			{
				std::unordered_map<std::string, token_info> tokens;
				for (json& jtoken : j["tokens"])
				{
					tokens.emplace(jtoken["username"], token_info{
							.username = jtoken["username"],
							.password = jtoken["password"],
							.expires_at = to_system_clock_time(jtoken["expires_at"]),
						});
				}
				std::vector<std::uint8_t> supported_method;
				for (json& jmethod : j["supported_method"])
				{
					supported_method.emplace_back(jmethod);
				}
				cfgs.emplace_back(socks5_reverse_proxy_info{
						.enable = j["enable"],
						.protocol = j["protocol"],
						.name = net::utf8_to_locale(j["name"].get<std::string>()),
						.ip_blacklist_minutes = std::stoul(j["ip_blacklist_minutes"].get<std::string>()),
						.listen_address = j["listen_address"],
						.listen_port = std::uint16_t(std::stoi(j["listen_port"].get<std::string>())),
						.supported_method = std::move(supported_method),
						.tokens = std::move(tokens),
					});
			}
		}
		catch (const std::exception& e)
		{
			app.logger->error("read config from '{}' modular failed: {}", "socks5_reverse_proxy", e.what());
		}

		return cfgs;
	}

	std::vector<service_process_mgr_info> config_impl::get_service_process_mgr_cfg()
	{
		std::shared_lock g(m_mutex);

		std::vector<service_process_mgr_info> cfgs;

		json& jm = m_jconfig["service_process_mgr"];

		if (jm.empty() || !jm.is_array())
		{
			app.logger->error("can't find the '{}' modular config", "service_process_mgr");
			return cfgs;
		}

		try
		{
			for (json& j : jm)
			{
				std::vector<process_info> process_list;
				for (json& jprocess : j["process_list"])
				{
					process_list.emplace_back(process_info{
							.name = net::utf8_to_locale(jprocess["name"].get<std::string>()),
							.path = net::utf8_to_locale(jprocess["path"].get<std::string>()),
							.args = jprocess["args"],
							.childs = jprocess["childs"],
						});
				}
				cfgs.emplace_back(service_process_mgr_info{
						.enable = j["enable"],
						.name = net::utf8_to_locale(j["name"].get<std::string>()),
						.auto_start_process = j["auto_start_process"],
						.auto_attach_process = j["auto_attach_process"],
						.stop_process_when_exit = j["stop_process_when_exit"],
						.stop_process_timeout = std::stoul(j["stop_process_timeout"].get<std::string>()),
						.process_list = std::move(process_list),
					});
			}
		}
		catch (const std::exception& e)
		{
			app.logger->error("read config from '{}' modular failed: {}", "service_process_mgr", e.what());
		}

		return cfgs;
	}

	std::vector<frontend_http_server_info> config_impl::get_frontend_http_server_cfg()
	{
		std::shared_lock g(m_mutex);

		std::vector<frontend_http_server_info> cfgs;

		json& jm = m_jconfig["frontend_http_server"];

		if (jm.empty() || !jm.is_array())
		{
			app.logger->error("can't find the '{}' modular config", "frontend_http_server");
			return cfgs;
		}

		try
		{
			for (json& j : jm)
			{
				std::unordered_map<std::string, token_info> tokens;
				for (json& jtoken : j["tokens"])
				{
					tokens.emplace(jtoken["username"], token_info{
							.username = jtoken["username"],
							.password = jtoken["password"],
							.expires_at = to_system_clock_time(jtoken["expires_at"]),
						});
				}
				auto& cfg = cfgs.emplace_back(frontend_http_server_info{
						.enable = j["enable"],
						.protocol = j["protocol"],
						.name = net::utf8_to_locale(j["name"].get<std::string>()),
						.listen_address = j["listen_address"],
						.listen_port = std::uint16_t(std::stoi(j["listen_port"].get<std::string>())),
						.webroot = net::utf8_to_locale(j["webroot"].get<std::string>()),
						.index = j["index"],
						.cert_file = net::utf8_to_locale(j["cert_file"].get<std::string>()),
						.key_file = net::utf8_to_locale(j["key_file"].get<std::string>()),
						.max_request_header_size = std::stoul(j["max_request_header_size"].get<std::string>()),
						.enable_cors = j["enable_cors"],
						.cors_allow_headers = j["cors_allow_headers"],
						.cors_allow_methods = j["cors_allow_methods"],
						.cors_allow_origin = j["cors_allow_origin"],
						.requires_auth = j["requires_auth"],
						.tokens = std::move(tokens),
					});
				net::trim_both(cfg.cors_allow_headers);
				net::trim_both(cfg.cors_allow_methods);
				net::trim_both(cfg.cors_allow_origin);
				if (cfg.cors_allow_headers.empty())
					cfg.cors_allow_headers = "*";
				if (cfg.cors_allow_methods.empty())
					cfg.cors_allow_methods = "*";
				if (cfg.cors_allow_origin.empty())
					cfg.cors_allow_origin = "*";
			}
		}
		catch (const std::exception& e)
		{
			app.logger->error("read config from '{}' modular failed: {}", "frontend_http_server", e.what());
		}

		return cfgs;
	}

}
