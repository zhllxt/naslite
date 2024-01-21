#pragma once

#include <cstdint>

#include <fstream>
#include <exception>
#include <expected>
#include <shared_mutex>
#include <map>
#include <unordered_map>
#include <chrono>
#include <vector>

#include "net.hpp"
#include "json.hpp"

namespace nas
{
	struct token_info
	{
		std::string username;
		std::string password;
		std::chrono::system_clock::time_point expires_at;
	};

	struct static_http_server_info
	{
		bool          enable = false;
		std::string   protocol;
		std::string   name;
		std::string   listen_address = "0.0.0.0";
		std::uint16_t listen_port = 0;
		std::string   webroot;
		std::string   index = "index.html";
		std::string   cert_file;
		std::string   key_file;
		std::uint32_t max_request_header_size = 1048576;
		bool          enable_cors = false;
		bool          requires_auth = false;
		std::unordered_map<std::string, token_info> tokens;
	};

	struct frontend_http_server_info
	{
		bool          enable = false;
		std::string   protocol;
		std::string   name;
		std::string   listen_address = "0.0.0.0";
		std::uint16_t listen_port = 0;
		std::string   webroot;
		std::string   index = "index.html";
		std::string   cert_file;
		std::string   key_file;
		std::uint32_t max_request_header_size = 1048576;
		bool          enable_cors = false;
		std::string   cors_allow_headers;
		std::string   cors_allow_methods;
		std::string   cors_allow_origin;
		bool          requires_auth = false;
		std::unordered_map<std::string, token_info> tokens;
	};

	struct proxy_auth_role
	{
		http::verb  method;
		std::string target;
		unsigned    result;
	};

	struct proxy_site_info
	{
		std::string   name;
		std::string   domain;
		std::string   host;
		std::uint16_t port;
		bool          skip_body_for_head_request;
		bool          skip_body_for_head_response;
		bool          requires_auth;
		std::vector<proxy_auth_role> auth_roles;
		std::map<std::string, std::string> proxy_set_header;
		std::map<std::string, std::string> proxy_options;
	};

	struct http_reverse_proxy_info
	{
		bool          enable = true;
		std::string   protocol;
		std::string   name;
		std::uint32_t ip_blacklist_minutes = 1440;
		std::string   listen_address = "0.0.0.0";
		std::uint16_t listen_port = 0;
		std::string   cert_file;
		std::string   key_file;
		std::unordered_map<std::string, proxy_site_info> proxy_sites;
	};

	struct socks5_reverse_proxy_info
	{
		bool          enable = true;
		std::string   protocol;
		std::string   name;
		std::uint32_t ip_blacklist_minutes = 1440;
		std::string   listen_address = "0.0.0.0";
		std::uint16_t listen_port = 0;
		std::vector<std::uint8_t> supported_method;
		std::unordered_map<std::string, token_info> tokens;
	};

	struct process_info
	{
		std::string name;
		std::string path;
		std::string args;
		std::string childs;
		std::shared_ptr<void> process;
	};

	struct service_process_mgr_info
	{
		bool          enable = true;
		std::string   name;
		bool          auto_start_process = false;
		bool          auto_attach_process = true;
		bool          stop_process_when_exit = false;
		std::uint32_t stop_process_timeout = 5000;
		std::vector<process_info> process_list;
	};

	class iconfig
	{
	public:
		iconfig()
		{
		}
		~iconfig()
		{
		}

		virtual std::expected<bool, std::exception_ptr> load(std::filesystem::path filepath) = 0;

		virtual std::expected<bool, std::exception_ptr> save() = 0;

		virtual std::string get_log_level() = 0;

		virtual std::vector<static_http_server_info> get_http_server_cfg() = 0;
		virtual std::vector<http_reverse_proxy_info> get_http_reverse_proxy_cfg() = 0;
		virtual std::vector<socks5_reverse_proxy_info> get_socks5_reverse_proxy_cfg() = 0;
		virtual std::vector<service_process_mgr_info> get_service_process_mgr_cfg() = 0;
		virtual std::vector<frontend_http_server_info> get_frontend_http_server_cfg() = 0;

		virtual const json& get_modular_json(std::string_view modular_name) = 0;
		virtual bool set_modular_json(std::string_view modular_name, const std::string& value) = 0;
	};
}
