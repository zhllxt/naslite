#pragma once

#ifndef ASIO3_ENABLE_SSL
#define ASIO3_ENABLE_SSL
#endif

#include <asio3/config.hpp>
#undef ASIO3_HEADER_ONLY

#include <asio3/core/asio.hpp>
#include <asio3/core/beast.hpp>
#include <asio3/core/timer.hpp>

#ifdef ASIO_STANDALONE
namespace net = ::asio;
#else
namespace net = boost::asio;
#endif
