#include "frontend_http_server.h"

#include "../../core/utils.hpp"
#include "../../main/app.hpp"

#include "../service_process_mgr/service_status_event.hpp"
#include "../service_process_mgr/service_start_event.hpp"
#include "../service_process_mgr/service_stop_event.hpp"
#include "../service_process_mgr/service_start_all_event.hpp"
#include "../service_process_mgr/service_stop_all_event.hpp"
#include "../../main/restart_naslite_event.hpp"
#include "http_clear_cache_all_event.hpp"

#include <jwt-cpp/jwt.h>
#include <asio3/http/request.hpp>
#include <asio3/core/codecvt.hpp>

namespace nas
{
	using router_data = frontend_http_server::router_data;
	using node = frontend_http_server::node;

	template<typename T>
	concept is_https_server = requires(T & a)
	{
		a->ssl_context;
	};

	bool jwt_verify(http::web_request& req, http::web_response& rep, frontend_http_server_info& cfg)
	{
		if (auto it = req.find(http::field::authorization); it != req.end())
		{
			std::string token{ it->value() };

			try
			{
				const auto decoded = jwt::decode(token);

				std::ostringstream oss;
				auto jclaim = decoded.get_payload_claim("object");
				oss << jclaim;

				json obj = json::parse(oss.str());
				const std::string& username = obj["username"];

				if (auto it = cfg.tokens.find(username); it != cfg.tokens.end())
				{
					jwt::verify()
						.allow_algorithm(jwt::algorithm::hs256{ it->second.password })
						.with_issuer(cfg.name)
						.with_claim("object", jclaim)
						.verify(decoded);

					app.logger->trace("jwt_verify success: {}", req.target());

					return true;
				}
			}
			catch (const std::exception&)
			{
			}
		}

		app.logger->error("jwt_verify failure: {}", req.target());

		return false;
	}

	struct aop_auth
	{
		net::awaitable<bool> before(http::web_request& req, http::web_response& rep, router_data data)
		{
			if (!data.cfg.requires_auth)
				co_return true;

			if (jwt_verify(req, rep, data.cfg))
				co_return true;

			rep = http::make_text_response("", http::status::unauthorized);
			co_return false;
		}
	};

	void set_cors(http::web_request& req, auto& res, frontend_http_server_info& cfg)
	{
		if (cfg.enable_cors)
		{
			res.set("Access-Control-Allow-Headers", cfg.cors_allow_headers);
			res.set("Access-Control-Allow-Methods", cfg.cors_allow_methods);
			res.set("Access-Control-Allow-Origin", cfg.cors_allow_origin);
		}
	}

	net::awaitable<bool> index_page(
		std::shared_ptr<node>& p, auto& server, http::web_request& req, http::web_response& rep, router_data data)
	{
		std::filesystem::path filepath = server->webroot / p->cfg.index;
		auto [ec, file, content] = co_await net::async_read_file_content(filepath.string());
		if (ec)
		{
			rep = http::make_error_page_response(http::status::not_found);
			co_return true;
		}

		rep = http::make_html_response(std::move(content));
		co_return true;
	}

	net::awaitable<bool> static_assets(
		std::shared_ptr<node>& p, auto& server, http::web_request& req, http::web_response& rep, router_data data)
	{
		std::filesystem::path filepath = net::make_filepath(server->webroot, req.target());

		auto [ec, file, content] = co_await net::async_read_file_content(filepath.string());
		if (ec)
		{
			rep = http::make_error_page_response(http::status::not_found);
			co_return true;
		}

		auto res = http::make_text_response(std::move(content));
		res.set(http::field::content_type, http::extension_to_mimetype(filepath.extension().string()));
		rep = std::move(res);
		co_return true;
	}

	net::awaitable<bool> get_config(
		auto& p, auto& server,
		http::web_request& req, http::web_response& rep, router_data& data,
		std::string_view modular_name)
	{
		const json& j = app.config->get_modular_json(modular_name);
		auto res = http::make_json_response(
			j.dump(), j.empty() ? http::status::no_content : http::status::ok);
		set_cors(req, res, p->cfg);
		rep = std::move(res);
		co_return true;
	}

	net::awaitable<bool> set_config(
		auto& p, auto& server,
		http::web_request& req, http::web_response& rep, router_data& data,
		std::string_view modular_name)
	{
		bool result = app.config->set_modular_json(modular_name, req.body());
		auto res = http::make_json_response("{}", result ? http::status::ok : http::status::bad_request);
		set_cors(req, res, p->cfg);
		rep = std::move(res);
		co_return true;
	}

	void init_server(std::shared_ptr<node>& p, auto& server)
	{
		std::error_code ec{};
		if (std::filesystem::absolute(p->cfg.webroot, ec) == std::filesystem::path(p->cfg.webroot))
			server->webroot = p->cfg.webroot;
		else
			server->webroot = app.exe_directory / p->cfg.webroot;

		if (ec)
			app.logger->error("    the webroot config '{}' is invalid: {}", p->cfg.webroot, ec.message());

		server->webroot = std::filesystem::canonical(server->webroot, ec);

		if (!std::filesystem::exists(server->webroot, ec) && !ec)
			app.logger->error("    the webroot directory '{}' of '{}' is not exists",
				p->cfg.webroot, p->cfg.name);
		else
			app.logger->info("    the webroot directory of '{}' is: {}",
				p->cfg.name, server->webroot.string());

		server->router.add("/", [p, server]
		(http::web_request& req, http::web_response& rep, router_data data) mutable -> net::awaitable<bool>
		{
			co_return co_await index_page(p, server, req, rep, data);
		}, http::enable_cache);

		server->router.add("/view/signin", [p, server]
		(http::web_request& req, http::web_response& rep, router_data data) mutable -> net::awaitable<bool>
		{
			co_return co_await index_page(p, server, req, rep, data);
		}, http::enable_cache);

		server->router.add("/view/http_reverse_proxy", [p, server]
		(http::web_request& req, http::web_response& rep, router_data data) mutable -> net::awaitable<bool>
		{
			co_return co_await index_page(p, server, req, rep, data);
		}, http::enable_cache);

		server->router.add("/view/socks5_reverse_proxy", [p, server]
		(http::web_request& req, http::web_response& rep, router_data data) mutable -> net::awaitable<bool>
		{
			co_return co_await index_page(p, server, req, rep, data);
		}, http::enable_cache);

		server->router.add("/view/static_http_server", [p, server]
		(http::web_request& req, http::web_response& rep, router_data data) mutable -> net::awaitable<bool>
		{
			co_return co_await index_page(p, server, req, rep, data);
		}, http::enable_cache);

		server->router.add("/view/service_process_mgr", [p, server]
		(http::web_request& req, http::web_response& rep, router_data data) mutable -> net::awaitable<bool>
		{
			co_return co_await index_page(p, server, req, rep, data);
		}, http::enable_cache);

		server->router.add("/view/frontend_http_server", [p, server]
		(http::web_request& req, http::web_response& rep, router_data data) mutable -> net::awaitable<bool>
		{
			co_return co_await index_page(p, server, req, rep, data);
		}, http::enable_cache);

		server->router.add("/favicon.ico", [p, server]
		(http::web_request& req, http::web_response& rep, router_data data) mutable -> net::awaitable<bool>
		{
			co_return co_await static_assets(p, server, req, rep, data);
		}, http::enable_cache);

		server->router.add("/assets/*", [p, server]
		(http::web_request& req, http::web_response& rep, router_data data) mutable -> net::awaitable<bool>
		{
			co_return co_await static_assets(p, server, req, rep, data);
		}, http::enable_cache);

		// for cors
		server->router.add<http::verb::options>("*", [p, server]
		(http::web_request& req, http::web_response& rep, router_data data) mutable -> net::awaitable<bool>
		{
			auto res = http::make_text_response(
				"", p->cfg.enable_cors ? http::status::ok : http::status::unauthorized);
			set_cors(req, res, p->cfg);
			rep = std::move(res);
			co_return true;
		});

		server->router.add<http::verb::post>("/api/user/signin", [p, server]
		(http::web_request& req, http::web_response& rep, router_data data) mutable -> net::awaitable<bool>
		{
			try
			{
				json j = json::parse(req.body());

				const std::string& username = j["username"];
				const std::string& password = j["password"];

				if (auto it = p->cfg.tokens.find(username); it != p->cfg.tokens.end())
				{
					token_info& token = it->second;

					if (password == token.password && std::chrono::system_clock::now() < token.expires_at)
					{
						jwt::claim jclaim;
						std::string payload = fmt::vformat(
							R"({{"version":100,"username":"{}"}})", fmt::make_format_args(username));
						std::istringstream iss{ std::move(payload) };
						iss >> jclaim;

						auto time = std::chrono::system_clock::now();

						auto jwt_token = jwt::create()
							.set_type("JWT")
							.set_issuer(p->cfg.name)
							.set_issued_at(time)
							.set_expires_at(time + std::chrono::seconds{ 36000 })
							.set_payload_claim("object", jclaim)
							.sign(jwt::algorithm::hs256{ password });

						json j = json::object();
						j["token_type"] = "JWT";
						j["access_token"] = std::move(jwt_token);
						j["refresh_token"] = nullptr;
						j["expires_at"] = std::chrono::duration_cast<std::chrono::seconds>(
							(time + std::chrono::seconds(36000)).time_since_epoch()).count();
						j["username"] = username;

						auto res = http::make_json_response(j.dump(), http::status::ok);
						set_cors(req, res, p->cfg);
						res.set(http::field::content_type, "application/plain;charset=UTF-8");

						app.logger->info("user signin successed: {}:{}", username, password);

						rep = std::move(res);
						co_return true;
					}
				}

				app.logger->error("user signin failed: {}:{}", username, password);
			}
			catch (const std::exception&)
			{
			}
		
			auto res = http::make_json_response(
				R"({"error":1,"message":"auth failed","data":""})", http::status::unauthorized);
			set_cors(req, res, p->cfg);
			rep = std::move(res);
			co_return true;
		});

		server->router.add<http::verb::get>("/api/user/signined", [p, server]
		(http::web_request& req, http::web_response& rep, router_data data) mutable -> net::awaitable<bool>
		{
			auto res = http::make_text_response("", http::status::unauthorized);
			set_cors(req, res, p->cfg);
			if (jwt_verify(req, rep, p->cfg))
				res.result(http::status::ok);
			rep = std::move(res);
			co_return true;
		});

		server->router.add<http::verb::get>("/api/status/service_process_mgr/all_process", [p, server]
		(http::web_request& req, http::web_response& rep, router_data data) mutable -> net::awaitable<bool>
		{
			std::shared_ptr<service_status_event> e = std::make_shared<service_status_event>(p->ctx.get_executor());
			if (app.event_dispatcher.dispatch(e))
			{
				co_await e->ch.async_receive(net::use_nothrow_awaitable);
			}
			else
			{
				e->ec = net::error::operation_aborted;
				e->message.clear();
				e->data.clear();
			}

			auto res = http::make_json_response(
				e->data.dump(), e->ec ? http::status::no_content : http::status::ok);
			set_cors(req, res, p->cfg);
			rep = std::move(res);
			co_return true;
		}, aop_auth{});

		server->router.add<http::verb::get>("/api/config/service_process_mgr", [p, server]
		(http::web_request& req, http::web_response& rep, router_data data) mutable -> net::awaitable<bool>
		{
			co_return co_await get_config(p, server, req, rep, data, "service_process_mgr");
		}, aop_auth{});

		server->router.add<http::verb::put>("/api/config/service_process_mgr", [p, server]
		(http::web_request& req, http::web_response& rep, router_data data) mutable -> net::awaitable<bool>
		{
			co_return co_await set_config(p, server, req, rep, data, "service_process_mgr");
		}, aop_auth{});

		server->router.add<http::verb::get>("/api/config/socks5_reverse_proxy", [p, server]
		(http::web_request& req, http::web_response& rep, router_data data) mutable -> net::awaitable<bool>
		{
			co_return co_await get_config(p, server, req, rep, data, "socks5_reverse_proxy");
		}, aop_auth{});

		server->router.add<http::verb::put>("/api/config/socks5_reverse_proxy", [p, server]
		(http::web_request& req, http::web_response& rep, router_data data) mutable -> net::awaitable<bool>
		{
			co_return co_await set_config(p, server, req, rep, data, "socks5_reverse_proxy");
		}, aop_auth{});

		server->router.add<http::verb::get>("/api/config/frontend_http_server", [p, server]
		(http::web_request& req, http::web_response& rep, router_data data) mutable -> net::awaitable<bool>
		{
			co_return co_await get_config(p, server, req, rep, data, "frontend_http_server");
		}, aop_auth{});

		server->router.add<http::verb::put>("/api/config/frontend_http_server", [p, server]
		(http::web_request& req, http::web_response& rep, router_data data) mutable -> net::awaitable<bool>
		{
			co_return co_await set_config(p, server, req, rep, data, "frontend_http_server");
		}, aop_auth{});

		server->router.add<http::verb::get>("/api/config/static_http_server", [p, server]
		(http::web_request& req, http::web_response& rep, router_data data) mutable -> net::awaitable<bool>
		{
			co_return co_await get_config(p, server, req, rep, data, "static_http_server");
		}, aop_auth{});

		server->router.add<http::verb::put>("/api/config/static_http_server", [p, server]
		(http::web_request& req, http::web_response& rep, router_data data) mutable -> net::awaitable<bool>
		{
			co_return co_await set_config(p, server, req, rep, data, "static_http_server");
		}, aop_auth{});

		server->router.add<http::verb::get>("/api/config/http_reverse_proxy", [p, server]
		(http::web_request& req, http::web_response& rep, router_data data) mutable -> net::awaitable<bool>
		{
			co_return co_await get_config(p, server, req, rep, data, "http_reverse_proxy");
		}, aop_auth{});

		server->router.add<http::verb::put>("/api/config/http_reverse_proxy", [p, server]
		(http::web_request& req, http::web_response& rep, router_data data) mutable -> net::awaitable<bool>
		{
			co_return co_await set_config(p, server, req, rep, data, "http_reverse_proxy");
		}, aop_auth{});

		server->router.add<http::verb::get>("/api/status/device/hardware_info", [p, server]
		(http::web_request& req, http::web_response& rep, router_data data) mutable -> net::awaitable<bool>
		{
			json j = get_hardware_info();
			auto res = http::make_json_response(j.dump(), http::status::ok);
			set_cors(req, res, p->cfg);
			rep = std::move(res);
			co_return true;
		}, aop_auth{});

		server->router.add<http::verb::get>("/api/status/device/disk_usage", [p, server]
		(http::web_request& req, http::web_response& rep, router_data data) mutable -> net::awaitable<bool>
		{
			json j = get_disk_usage();
			auto res = http::make_json_response(j.dump(), http::status::ok);
			set_cors(req, res, p->cfg);
			rep = std::move(res);
			co_return true;
		}, aop_auth{});

		server->router.add<http::verb::get>("/api/status/device/cpu_usage", [p, server]
		(http::web_request& req, http::web_response& rep, router_data data) mutable -> net::awaitable<bool>
		{
			json j = get_cpu_usage();
			auto res = http::make_json_response(j.dump(), http::status::ok);
			set_cors(req, res, p->cfg);
			rep = std::move(res);
			co_return true;
		}, aop_auth{});

		server->router.add<http::verb::get>("/api/status/device/memory_usage", [p, server]
		(http::web_request& req, http::web_response& rep, router_data data) mutable -> net::awaitable<bool>
		{
			json j = get_memory_usage();
			auto res = http::make_json_response(j.dump(), http::status::ok);
			set_cors(req, res, p->cfg);
			rep = std::move(res);
			co_return true;
		}, aop_auth{});

		server->router.add<http::verb::get>("/api/status/hardware/temperatures", [p, server]
		(http::web_request& req, http::web_response& rep, router_data data) mutable -> net::awaitable<bool>
		{
			http::request_option opt{
				.url = "http://127.0.0.1:28881/api/status/hardware/temperatures",
				.method = http::verb::get,
				.socket = p->sock_for_temperatures,
			};
			auto [e1, resp1] = co_await http::co_request(opt);
			if (e1 == asio::error::connection_reset)
			{
				auto [e2, resp2] = co_await http::co_request(opt);
				e1 = e2;
				resp1 = std::move(resp2);
			}
			if (e1)
			{
				auto res = http::make_text_response(net::locale_to_utf8(e1.message()),
					http::status::bad_request, "text/plain;charset=UTF-8");
				set_cors(req, res, p->cfg);
				rep = std::move(res);
				co_return true;
			}
			else
			{
				auto res = http::make_json_response(resp1.body(), http::status::ok);
				set_cors(req, res, p->cfg);
				rep = std::move(res);
				co_return true;
			}
		}, aop_auth{});

		server->router.add<http::verb::post>("/api/command/device/shutdown", [p, server]
		(http::web_request& req, http::web_response& rep, router_data data) mutable -> net::awaitable<bool>
		{
			nas::shutdown();
			auto res = http::make_json_response("{}", http::status::ok);
			set_cors(req, res, p->cfg);
			rep = std::move(res);
			co_return true;
		}, aop_auth{});

		server->router.add<http::verb::post>("/api/command/device/restart", [p, server]
		(http::web_request& req, http::web_response& rep, router_data data) mutable -> net::awaitable<bool>
		{
			nas::restart();
			auto res = http::make_json_response("{}", http::status::ok);
			set_cors(req, res, p->cfg);
			rep = std::move(res);
			co_return true;
		}, aop_auth{});

		server->router.add<http::verb::post>("/api/command/naslite/restart", [p, server]
		(http::web_request& req, http::web_response& rep, router_data data) mutable -> net::awaitable<bool>
		{
			std::shared_ptr<restart_naslite_event> e = std::make_shared<restart_naslite_event>(p->ctx.get_executor());
			if (app.event_dispatcher.dispatch(e))
			{
				co_await e->ch.async_receive(net::use_nothrow_awaitable);
			}
			else
			{
				e->ec = net::error::operation_aborted;
				e->message = e->ec.message();
				e->data = json::parse(R"({"error":3,"message":"failed"})");
			}

			auto res = http::make_json_response(
				e->data.dump(), e->ec ? http::status::bad_request : http::status::ok);
			set_cors(req, res, p->cfg);
			rep = std::move(res);
			co_return true;
		}, aop_auth{});

		server->router.add<http::verb::post>("/api/command/service_process_mgr/start", [p, server]
		(http::web_request& req, http::web_response& rep, router_data data) mutable -> net::awaitable<bool>
		{
			std::shared_ptr<service_start_event> e = std::make_shared<service_start_event>(p->ctx.get_executor());
			e->request_body = req.body();
			if (app.event_dispatcher.dispatch(e))
			{
				co_await e->ch.async_receive(net::use_nothrow_awaitable);
			}
			else
			{
				e->ec = net::error::operation_aborted;
				e->message = e->ec.message();
				e->data = json::parse(R"({"error":3,"message":"failed"})");
			}

			auto res = http::make_json_response(
				e->data.dump(), e->ec ? http::status::bad_request : http::status::ok);
			set_cors(req, res, p->cfg);
			rep = std::move(res);
			co_return true;
		}, aop_auth{});

		server->router.add<http::verb::post>("/api/command/service_process_mgr/stop", [p, server]
		(http::web_request& req, http::web_response& rep, router_data data) mutable -> net::awaitable<bool>
		{
			std::shared_ptr<service_stop_event> e = std::make_shared<service_stop_event>(p->ctx.get_executor());
			e->request_body = req.body();
			if (app.event_dispatcher.dispatch(e))
			{
				co_await e->ch.async_receive(net::use_nothrow_awaitable);
			}
			else
			{
				e->ec = net::error::operation_aborted;
				e->message = e->ec.message();
				e->data = json::parse(R"({"error":3,"message":"failed"})");
			}

			auto res = http::make_json_response(
				e->data.dump(), e->ec ? http::status::bad_request : http::status::ok);
			set_cors(req, res, p->cfg);
			rep = std::move(res);
			co_return true;
		}, aop_auth{});

		server->router.add<http::verb::post>("/api/command/service_process_mgr/start_all", [p, server]
		(http::web_request& req, http::web_response& rep, router_data data) mutable -> net::awaitable<bool>
		{
			std::shared_ptr<service_start_all_event> e = std::make_shared<service_start_all_event>(p->ctx.get_executor());
			if (app.event_dispatcher.dispatch(e))
			{
				co_await e->ch.async_receive(net::use_nothrow_awaitable);
			}
			else
			{
				e->ec = net::error::operation_aborted;
				e->message = e->ec.message();
				e->data = json::parse(R"({"error":3,"message":"failed"})");
			}

			auto res = http::make_json_response(
				e->data.dump(), e->ec ? http::status::bad_request : http::status::ok);
			set_cors(req, res, p->cfg);
			rep = std::move(res);
			co_return true;
		}, aop_auth{});

		server->router.add<http::verb::post>("/api/command/service_process_mgr/stop_all", [p, server]
		(http::web_request& req, http::web_response& rep, router_data data) mutable -> net::awaitable<bool>
		{
			std::shared_ptr<service_stop_all_event> e = std::make_shared<service_stop_all_event>(p->ctx.get_executor());
			if (app.event_dispatcher.dispatch(e))
			{
				co_await e->ch.async_receive(net::use_nothrow_awaitable);
			}
			else
			{
				e->ec = net::error::operation_aborted;
				e->message = e->ec.message();
				e->data = json::parse(R"({"error":3,"message":"failed"})");
			}

			auto res = http::make_json_response(
				e->data.dump(), e->ec ? http::status::bad_request : http::status::ok);
			set_cors(req, res, p->cfg);
			rep = std::move(res);
			co_return true;
		}, aop_auth{});

		server->router.add<http::verb::post>("/api/command/http/clear_cache/all", [p, server]
		(http::web_request& req, http::web_response& rep, router_data data) mutable -> net::awaitable<bool>
		{
			server->router.get_cache().clear();

			std::shared_ptr<http_clear_cache_all_event> e =
				std::make_shared<http_clear_cache_all_event>(p->ctx.get_executor());
			if (app.event_dispatcher.dispatch(e))
			{
				co_await e->ch.async_receive(net::use_nothrow_awaitable);
			}
			else
			{
				e->ec = net::error::operation_aborted;
				e->message = e->ec.message();
				e->data = json::parse(R"({"error":3,"message":"failed"})");
			}

			auto res = http::make_json_response(
				e->data.dump(), e->ec ? http::status::bad_request : http::status::ok);
			set_cors(req, res, p->cfg);
			rep = std::move(res);
			co_return true;
		}, aop_auth{});
	}

	net::awaitable<void> do_recv(std::shared_ptr<node>& p, auto& server, auto& session)
	{
		// This buffer is required to persist across reads
		beast::flat_buffer buf;

		for (;;)
		{
			// Read a request
			http::web_request req;
			auto [e1, n1] = co_await http::async_read(session->get_stream(), buf, req);
			if (e1)
				break;

			app.logger->trace("frontend_http_server::request: {} {}", req.method_string(), req.target());

			for (auto it = req.begin(); it != req.end(); ++it)
			{
				app.logger->trace("    {}: {}", it->name_string(), it->value());
			}

			session->update_alive_time();

			http::web_response rep;
			bool result = co_await server->router.route(req, rep, router_data{ session->socket, p->cfg });

			// Send the response
			auto [e2, n2] = co_await beast::async_write(session->get_stream(), std::move(rep));
			if (e2)
				break;

			if (!result || !req.keep_alive())
			{
				// This means we should close the connection, usually because
				// the response indicated the "Connection: close" semantic.
				break;
			}
		}

		session->close();
	}

	net::awaitable<void> do_session(std::shared_ptr<node>& p, auto& server, auto session)
	{
		co_await server->session_map.async_add(session);
		co_await(do_recv(p, server, session) || net::watchdog(session->alive_time, net::http_idle_timeout));
		co_await server->session_map.async_remove(session);
	}

	net::awaitable<void> client_join(std::shared_ptr<node> p, auto& server, auto client)
	{
		if constexpr (is_https_server<decltype(server)>)
		{
			auto session = std::make_shared<net::https_session>(std::move(client), server->ssl_context);
			auto [e2] = co_await net::async_handshake(
				session->ssl_stream, net::ssl::stream_base::handshake_type::server);
			if (e2)
			{
				app.logger->error("frontend_http_server handshake failure: {} {}:{} {}",
					p->cfg.name, p->cfg.listen_address, p->cfg.listen_port, e2.message());
				co_return;
			}
			co_await do_session(p, server, std::move(session));
		}
		else
		{
			auto session = std::make_shared<net::http_session>(std::move(client));
			co_await do_session(p, server, std::move(session));
		}
	}

	net::awaitable<void> start_server(std::shared_ptr<node> p, auto& server)
	{
		// delay some time to ensure the init log finished.
		co_await net::delay(std::chrono::milliseconds(500));

		auto [ec, ep] = co_await server->async_listen(p->cfg.listen_address, p->cfg.listen_port);
		if (ec)
		{
			app.logger->error("frontend_http_server listen failure: {} {}:{} {}",
				p->cfg.name, p->cfg.listen_address, p->cfg.listen_port, ec.message());
			co_return;
		}

		app.logger->info("frontend_http_server listen success: {} {}:{}",
			p->cfg.name, server->get_listen_address(), server->get_listen_port());

		while (!server->is_aborted())
		{
			auto [e1, client] = co_await server->acceptor.async_accept();
			if (e1)
			{
				co_await net::delay(std::chrono::milliseconds(100));
			}
			else
			{
				net::co_spawn(server->get_executor(), client_join(p, server, std::move(client)), net::detached);
			}
		}
	}

	frontend_http_server::frontend_http_server() : imodular()
	{

	}

	bool frontend_http_server::init()
	{
		auto cfgs = app.config->get_frontend_http_server_cfg();

		for (auto& cfg : cfgs)
		{
			if (!cfg.enable)
				continue;

			std::shared_ptr<node> p = std::make_shared<node>();

			p->cfg = std::move(cfg);

			if /**/ (net::iequals(p->cfg.protocol, "http"))
			{
				p->server = std::make_shared<http_server_ex>(p->ctx.get_executor());
			}
			else if (net::iequals(p->cfg.protocol, "https"))
			{
				auto cert_file_path = to_canonical_path(app.exe_directory, p->cfg.cert_file);
				auto key_file_path = to_canonical_path(app.exe_directory, p->cfg.key_file);

				// nginx: ssl->ctx = SSL_CTX_new(SSLv23_method());
				net::error_code ec{};
				net::ssl::context sslctx(net::ssl::context::sslv23);
				sslctx.set_options(
					net::ssl::context::default_workarounds |
					net::ssl::context::no_sslv2 |
					net::ssl::context::single_dh_use, ec);
				if (ec)
				{
					app.logger->error("    set ssl options failed: {} {}", p->cfg.name, ec.message());
					continue;
				}
				sslctx.use_certificate_chain_file(cert_file_path.string(), ec);
				if (ec)
				{
					app.logger->error("    set ssl certificate chain for '{}' failed: {} {}",
						p->cfg.name, p->cfg.cert_file, ec.message());
					continue;
				}
				sslctx.use_private_key_file(key_file_path.string(), asio::ssl::context::pem, ec);
				if (ec)
				{
					app.logger->error("    set ssl private key for '{}' failed: {} {}",
						p->cfg.name, p->cfg.key_file, ec.message());
					continue;
				}

				p->server = std::make_shared<https_server_ex>(p->ctx.get_executor(), std::move(sslctx));
			}
			else
			{
				app.logger->error("    the protocol config '{}' of '{}' is invalid", p->cfg.protocol, p->cfg.name);
				continue;
			}

			std::visit([&p](auto& server) mutable
			{
				init_server(p, server);
			}, p->server);

			nodes.emplace_back(std::move(p));
		}

		return true;
	}

	bool frontend_http_server::start()
	{
		for (auto& p : nodes)
		{
			std::visit([&p](auto& server) mutable
			{
				net::co_spawn(server->get_executor(), start_server(p, server), net::detached);
			}, p->server);
		}
		
		return true;
	}

	void frontend_http_server::stop()
	{
		for (auto& p : nodes)
		{
			std::visit([&p](auto& server) mutable
			{
				server->async_stop([p](net::error_code ec)
				{
					p->sock_for_temperatures.close(ec);
				});
			}, p->server);
		}
		for (auto& p : nodes)
		{
			p->ctx.join();
		}
	}

	void frontend_http_server::uninit()
	{
		nodes.clear();
	}
}
