#pragma once

#include <optional>

#include "../../core/net.hpp"
#include "../../core/json.hpp"
#include "../../core/iconfig.hpp"
#include "../../core/utils.hpp"
#include "../../core/noncopyable.hpp"

#include <asio3/core/pfr.hpp>

namespace nas
{
	struct request_info
	{
		asio::ssl::stream<net::tcp_socket&>* stream;
		net::tcp_socket* sock;
		http::request_header<>& header;
	};

	struct ibuiltin_variable
	{
		virtual ~ibuiltin_variable() {}
		virtual std::optional<std::string> get_value(request_info& info) = 0;
		virtual std::string_view get_variable_name() = 0;
	};

	struct http_host final
		: public ibuiltin_variable
		, public pfr::base_dynamic_creator<ibuiltin_variable, http_host>
	{
		virtual std::optional<std::string> get_value(request_info& info) override
		{
			if (auto it = info.header.find(http::field::host); it != info.header.end())
				return it->value();
			return std::nullopt;
		}
		virtual std::string_view get_variable_name() override
		{
			return "http_host";
		}
	};

	struct http_upgrade final
		: public ibuiltin_variable
		, public pfr::base_dynamic_creator<ibuiltin_variable, http_upgrade>
	{
		virtual std::optional<std::string> get_value(request_info& info) override
		{
			if (auto it = info.header.find(http::field::upgrade); it != info.header.end())
				return it->value();
			return std::nullopt;
		}
		virtual std::string_view get_variable_name() override
		{
			return "http_upgrade";
		}
	};

	struct http_connection final
		: public ibuiltin_variable
		, public pfr::base_dynamic_creator<ibuiltin_variable, http_connection>
	{
		virtual std::optional<std::string> get_value(request_info& info) override
		{
			if (auto it = info.header.find(http::field::connection); it != info.header.end())
				return it->value();
			return std::nullopt;
		}
		virtual std::string_view get_variable_name() override
		{
			return "http_connection";
		}
	};

	struct remote_addr final
		: public ibuiltin_variable
		, public pfr::base_dynamic_creator<ibuiltin_variable, remote_addr>
	{
		virtual std::optional<std::string> get_value(request_info& info) override
		{
			net::tcp_socket* sock = info.sock ? info.sock : std::addressof(info.stream->next_layer());
			if (sock)
			{
				net::error_code ec{};
				auto endp = sock->lowest_layer().remote_endpoint(ec);
				if (ec)
					return std::nullopt;

				std::string result = endp.address().to_string(ec);
				if (result.empty())
					return std::nullopt;

				return result;
			}
			else
			{
				return std::nullopt;
			}
		}
		virtual std::string_view get_variable_name() override
		{
			return "remote_addr";
		}
	};

	struct remote_port final
		: public ibuiltin_variable
		, public pfr::base_dynamic_creator<ibuiltin_variable, remote_port>
	{
		virtual std::optional<std::string> get_value(request_info& info) override
		{
			net::tcp_socket* sock = info.sock ? info.sock : std::addressof(info.stream->next_layer());
			if (sock)
			{
				net::error_code ec{};
				auto endp = sock->lowest_layer().remote_endpoint(ec);
				if (ec)
					return std::nullopt;

				return std::to_string(endp.port());
			}
			else
			{
				return std::nullopt;
			}
		}
		virtual std::string_view get_variable_name() override
		{
			return "remote_port";
		}
	};

	struct proxy_add_x_forwarded_for final
		: public ibuiltin_variable
		, public pfr::base_dynamic_creator<ibuiltin_variable, proxy_add_x_forwarded_for>
	{
		virtual std::optional<std::string> get_value(request_info& info) override
		{
			net::tcp_socket* sock = info.sock ? info.sock : std::addressof(info.stream->next_layer());
			if (sock)
			{
				std::string result;
				auto it = info.header.find("X-Forwarded-For");
				if (it != info.header.end())
					result = it->value();
				net::error_code ec{};
				std::string ip = sock->lowest_layer().remote_endpoint(ec).address().to_string(ec);
				if (!ip.empty())
				{
					net::trim_right(result);
					if (!result.empty() && result.back() == ',')
						result.pop_back();
					if (!result.empty())
						result += ", ";
					result += ip;
					return result;
				}
				else
				{
					if (it != info.header.end())
						return result;
					return std::nullopt;
				}
			}
			else
			{
				if (auto it = info.header.find("X-Forwarded-For"); it != info.header.end())
					return it->value();
				return std::nullopt;
			}
		}
		virtual std::string_view get_variable_name() override
		{
			return "proxy_add_x_forwarded_for";
		}
	};

	struct http_x_forwarded_proto final
		: public ibuiltin_variable
		, public pfr::base_dynamic_creator<ibuiltin_variable, http_x_forwarded_proto>
	{
		virtual std::optional<std::string> get_value(request_info& info) override
		{
			if (info.stream)
				return "https";
			else if (info.sock)
				return "http";
			else
				return std::nullopt;
		}
		virtual std::string_view get_variable_name() override
		{
			return "http_x_forwarded_proto";
		}
	};

	struct ssl_client_cert final
		: public ibuiltin_variable
		, public pfr::base_dynamic_creator<ibuiltin_variable, ssl_client_cert>
	{
		virtual std::optional<std::string> get_value(request_info& info) override
		{
			std::optional<std::string> result{};

			if (info.stream)
			{
				X509* client_cert = SSL_get_peer_certificate(info.stream->native_handle());
				if (client_cert != nullptr)
				{
					BIO* mem_bio = BIO_new(BIO_s_mem());
					if (mem_bio != nullptr)
					{
						PEM_write_bio_X509(mem_bio, client_cert);

						BUF_MEM* mem_buf;
						BIO_get_mem_ptr(mem_bio, &mem_buf);

						result = std::string{ mem_buf->data, mem_buf->length };

						BIO_free(mem_bio);
					}

					X509_free(client_cert);
				}
			}

			return result;
		}
		virtual std::string_view get_variable_name() override
		{
			return "ssl_client_cert";
		}
	};

	class builtin_variables : public noncopyable
	{
	private:
		builtin_variables()
		{
			pfr::class_factory<ibuiltin_variable>& factory = pfr::class_factory<ibuiltin_variable>::instance();

			factory.for_each([this](std::string name, const auto& func) mutable
			{
				std::shared_ptr<ibuiltin_variable> var = std::shared_ptr<ibuiltin_variable>(func());
				m_variable_map[var->get_variable_name()] = var;
			});
		}
		~builtin_variables()
		{
		}

	public:
		static builtin_variables& instance() { static builtin_variables g; return g; }

		std::optional<std::string> get_value(request_info& info, std::string_view variable)
		{
			if (!variable.empty())
			{
				for (auto& c : variable)
				{
					const_cast<std::string_view::reference>(c) =
						static_cast<std::string_view::value_type>(std::tolower(c));
				}

				if (auto it = m_variable_map.find(variable); it != m_variable_map.end())
				{
					return it->second->get_value(info);
				}
			}

			return std::nullopt;
		}

	protected:
		std::map<std::string_view, std::shared_ptr<ibuiltin_variable>> m_variable_map;
	};
}
