#pragma once

// Официальный Boost.Asio: заголовок boost/asio.hpp и пространство boost::asio.
// Репозиторий библиотеки — github.com/boostorg/asio. Это не standalone Asio
// автора (github.com/chriskohlhoff/asio): у того другие заголовки, <asio.hpp>,
// и макрос ASIO_STANDALONE.
#include <boost/asio.hpp>

namespace net = boost::asio;
using error_code = boost::system::error_code;
