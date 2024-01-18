#pragma once

#include "../../core/iconfig.hpp"

namespace nas
{
	class config_impl final : public iconfig
	{
	public:
		config_impl() : iconfig()
		{
		}
		~config_impl()
		{
		}

		std::chrono::system_clock::time_point to_system_clock_time(const std::string& str);

		std::expected<bool, std::exception_ptr> load(std::filesystem::path filepath) override;

		std::expected<bool, std::exception_ptr> save() override;

		std::string get_log_level() override;

		const json& get_modular_json(std::string_view modular_name) override;

		bool set_modular_json(std::string_view modular_name, const std::string& value) override;

		std::vector<static_http_server_info> get_http_server_cfg() override;

		std::vector<http_reverse_proxy_info> get_http_reverse_proxy_cfg() override;

		std::vector<socks5_reverse_proxy_info> get_socks5_reverse_proxy_cfg() override;

		std::vector<service_process_mgr_info> get_service_process_mgr_cfg() override;

		std::vector<frontend_http_server_info> get_frontend_http_server_cfg() override;

	protected:
		void do_save();

	protected:
		std::shared_mutex     m_mutex;
		std::filesystem::path m_filepath;
		json                  m_jconfig = json::object();
	};
}
