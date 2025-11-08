#ifndef CONSTANTS_H
#define CONSTANTS_H

#include <chrono>
#include <string>

namespace constants {
constexpr unsigned long max_k = 10000;
constexpr unsigned long max_n = 8;
constexpr unsigned long max_m = 12341234;

constexpr double min_coeff = -100.0;
constexpr double max_coeff = 100.0;
constexpr double eps = 3e-8;
constexpr double min_put_value = -5.0;
constexpr double max_put_value = 5.0;

constexpr int max_fractional_digits = 7;

constexpr int early_put_penalty = 20;
constexpr int bad_put_penalty = 10;
constexpr int bad_put_delay = 1;   // seconds
constexpr int hello_wait_time = 3; // seconds

const std::string crlf = "\r\n";

constexpr int listening_socket_backlog = 64;
constexpr int reset_delay = 1000; // milliseconds
const auto client_timeout = std::chrono::milliseconds(200);
} // namespace constants

#endif // CONSTANTS_H