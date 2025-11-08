#include "client_logic.h"

#include <poll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <iomanip>
#include <ios>
#include <iostream>
#include <iterator>
#include <numeric>
#include <string>
#include <utility>
#include <vector>

#include "constants.h"
#include "err.h"
#include "msg_parser.h"
#include "ts_queue.h"

ClientLogic::ClientLogic(const std::string& player_id, bool is_auto_strategy)
    : player_id(player_id),
      is_auto_strategy(is_auto_strategy),
      game_over(false),
      incoming_messages(),
      outgoing_messages(),
      logs(),
      K(0),
      K_set(false),
      puts_without_answer(1), // initialized to 1 to wait for coefficients before putting
      waiting_for_put_response(),
      current_approximation(),
      real_values(),
      poly_value_mutex() {}

void ClientLogic::log_stdout(const std::string& msg) {
    logs.push(std::make_pair(msg, false));
}

void ClientLogic::log_stderr(const std::string& msg) {
    logs.push(std::make_pair(msg, true));
}

void ClientLogic::register_connection(const std::string& server_ip, int server_port,
                                      int sockfd) {
    this->server_ip = server_ip;
    this->server_port = server_port;
    this->sockfd = sockfd;
    this->server_info = "[" + server_ip + "]:" + std::to_string(server_port);
    this->full_info = this->server_info + ", " + this->player_id;

    log_stdout("Connected to " + server_info);
}

void ClientLogic::start_threads_and_send_hello() {
    log_thread = std::thread(&ClientLogic::log_printer, this);
    strategy_thread = std::thread(
        is_auto_strategy ? &ClientLogic::auto_strategy : &ClientLogic::manual_strategy, this);
    network_receiver_thread = std::thread(&ClientLogic::network_receiver, this);
    network_sender_thread = std::thread(&ClientLogic::network_sender, this);
    message_processor_thread = std::thread(&ClientLogic::message_processor, this);

    send_hello_message();
}

void ClientLogic::join_threads() {
    join_thread(message_processor_thread);
    join_thread(network_sender_thread);
    join_thread(network_receiver_thread);
    join_thread(strategy_thread);
    join_thread(log_thread);
}

void ClientLogic::join_thread(std::thread& thread) {
    if (thread.joinable()) {
        thread.join();
    }
}

void ClientLogic::print_log_to_console(std::pair<std::string, bool>& log) {
    if (log.second) {
        std::cerr << "ERROR: " << log.first << std::endl;
    } else {
        std::cout << log.first << "." << std::endl;
    }
}

void ClientLogic::log_printer() {
    std::cout << std::fixed << std::setprecision(constants::max_fractional_digits);
    std::cerr << std::fixed << std::setprecision(constants::max_fractional_digits);
    std::pair<std::string, bool> log;

    while (!game_over.load()) {
        if (logs.try_pop_for(log, constants::client_timeout)) {
            print_log_to_console(log);
        }
    }

    if (logs.try_pop_for(log, constants::client_timeout)) {
        print_log_to_console(log);
    }

    while (logs.try_pop(log)) {
        print_log_to_console(log);
    }
}

void ClientLogic::manual_strategy() {
    constexpr size_t max_line_length = 128;
    char temp_buf[max_line_length];
    std::string buffer = "";

    struct pollfd stdin_pollfd = {STDIN_FILENO, POLLIN, 0};

    while (!game_over.load()) {
        int ready = poll(&stdin_pollfd, 1, constants::client_timeout.count());
        if (ready < 0) {
            syserr("poll");
        } else if (ready == 0) { // Timeout
            continue;
        }

        if (stdin_pollfd.revents & POLLIN) {
            ssize_t bytes_read = read(STDIN_FILENO, temp_buf, max_line_length);
            if (bytes_read < 0) {
                log_stderr("Error reading from stdin.");
                continue;
            }

            buffer.append(temp_buf, bytes_read);
        }

        size_t newline_pos;
        while ((newline_pos = buffer.find('\n')) != std::string::npos) {
            std::string line = buffer.substr(0, newline_pos);
            buffer.erase(0, newline_pos + 1);

            std::vector<std::string> params;
            if (!Message::splitParams(line, params)) {
                log_stderr("invalid input line " + line);
                continue;
            }

            int point;
            double value;

            if (params.size() != 2 || !Message::parseInteger(params[0], point) ||
                !Message::parseDouble(params[1], value)) {
                log_stderr("invalid input line " + line);
                continue;
            }

            send_put_message(point, value);
        }
    }
}

void ClientLogic::auto_strategy() {
    while (!game_over.load()) {
        if (wait_for_puts(constants::client_timeout)) { // wait for put response from server
            increment_puts_without_answer();
            std::pair<int, double> best_put = get_best_put();
            send_put_message(best_put.first, best_put.second);
        }
    }
}

void ClientLogic::network_receiver() {
    std::string recv_buffer;
    char temp_buf[std::numeric_limits<uint16_t>::max()];
    bool is_first_message = true;

    while (!game_over.load()) {
        ssize_t bytes_received =
            recv(sockfd, temp_buf, std::numeric_limits<uint16_t>::max(), 0);

        if (bytes_received > 0) {
            recv_buffer.append(temp_buf, bytes_received);

            size_t crlf_pos;
            while ((crlf_pos = recv_buffer.find(constants::crlf)) != std::string::npos) {
                std::string line = recv_buffer.substr(0, crlf_pos + constants::crlf.length());
                recv_buffer.erase(0, crlf_pos + constants::crlf.length());

                std::unique_ptr<Message> msg = Message::createMessage(line);

                if (msg) {
                    incoming_messages.push(std::move(msg));
                } else {
                    std::string error_msg =
                        "bad message from " + full_info + ": " +
                        line.substr(0, line.size() - constants::crlf.length());
                    if (is_first_message) {
                        fatal(error_msg.c_str());
                    } else {
                        log_stderr(error_msg);
                    }
                }

                is_first_message = false;
            }
        } else if (bytes_received == 0) { // Server closed connection
            game_over.store(true);
        } else {
            if (errno == EAGAIN || errno == EWOULDBLOCK) { // Timeout
                continue;
            }
            syserr("recv");
        }
    }

    if (!recv_buffer.empty()) {
        log_stderr("partial message remaining in buffer at disconnection: " + recv_buffer);
    }
}

void ClientLogic::network_sender() {
    std::unique_ptr<Message> msg;

    while (!game_over.load()) {
        if (!outgoing_messages.try_pop_for(msg, constants::client_timeout)) {
            continue;
        }

        const char* msg_ptr = msg->getRawMessage().c_str();
        ssize_t nleft = msg->getRawMessage().length();
        ssize_t nwritten = 0;

        while (nleft > 0) {
            if ((nwritten = write(sockfd, msg_ptr, nleft)) <= 0) {
                if ((errno == EAGAIN || errno == EWOULDBLOCK) &&
                    game_over.load()) { // Timeout and game over
                    break;
                }
                syserr("write");
            }

            nleft -= nwritten;
            msg_ptr += nwritten;
        }
    }
}

void ClientLogic::message_processor() {
    std::unique_ptr<Message> msg;
    bool is_first_message = true;
    bool incorrect_message = false;
    bool scoring_received = false;

    while (!game_over.load()) {
        if (!incoming_messages.try_pop_for(msg, constants::client_timeout)) {
            continue;
        }

        if (is_first_message) {
            is_first_message = false;
            incorrect_message = true;
            if (msg->getType() == MessageType::COEFF) {
                incorrect_message =
                    !processCoeffMessage(dynamic_cast<CoeffMessage*>(msg.get()));
            }

            if (incorrect_message) {
                fatal("bad message from %s: %s", full_info.c_str(),
                      msg->toRawString().c_str());
            }
            continue;
        }

        // Not a first message
        switch (msg->getType()) {
            case MessageType::BAD_PUT:
                incorrect_message =
                    !processBadPutMessage(dynamic_cast<BadPutMessage*>(msg.get()));
                break;
            case MessageType::STATE:
                incorrect_message =
                    !processStateMessage(dynamic_cast<StateMessage*>(msg.get()));
                break;
            case MessageType::PENALTY:
                incorrect_message =
                    !processPenaltyMessage(dynamic_cast<PenaltyMessage*>(msg.get()));
                break;
            case MessageType::SCORING:
                incorrect_message =
                    !processScoringMessage(dynamic_cast<ScoringMessage*>(msg.get()));

                if (!incorrect_message) {
                    scoring_received = true;
                }
                break;
            default: incorrect_message = true; break;
        }

        if (incorrect_message) {
            log_stderr("bad message from " + full_info + ": " + msg->toRawString());
        }
    }

    if (!scoring_received) {
        fatal("unexpected server disconnect");
    }
}

bool ClientLogic::processCoeffMessage(CoeffMessage* msg) {
    // We already know it is a first message from the server.
    // Correctness of the message has already been checked by the message parser.
    coeffs = msg->getCoeffs();
    N = coeffs.size() - 1;

    std::string log_msg = "Received coefficients: " +
                          std::accumulate(coeffs.begin(), coeffs.end(), std::string(),
                                          [](std::string a, double b) {
                                              return a + Message::doubleToString(b) + " ";
                                          });
    log_msg.pop_back(); // Remove the last space
    log_stdout(log_msg);

    // Since K >= 1 we can already tell that:
    current_approximation.resize(2);
    real_values.resize(2);
    current_approximation[0] = 0;
    current_approximation[1] = 0;
    real_values[0] = poly_at(0);
    real_values[1] = poly_at(1);

    // We can now put
    decrement_puts_without_answer();

    return true;
}

bool ClientLogic::processBadPutMessage(BadPutMessage* msg) {
    log_stdout("Received bad put response (" + Message::doubleToString(msg->getValue()) +
               " in " + std::to_string(msg->getPoint()) + ")");
    if (is_auto_strategy) {
        decrement_puts_without_answer();
    }
    return true;
}

bool ClientLogic::processStateMessage(StateMessage* msg) {
    log_stdout("Received state: " + msg->toRawString().substr(std::string("STATE ").length()));

    if (is_auto_strategy && !K_set.load()) {
        std::scoped_lock<std::mutex> lock(poly_value_mutex);
        int K_from_server = msg->getApproxValues().size() - 1;

        K.store(K_from_server);
        K_set.store(true);

        current_approximation.resize(K_from_server + 1, 0);
        real_values.resize(K_from_server + 1);

        for (int i = 0; i <= K_from_server; i++) {
            real_values[i] = poly_at(i);
        }

        decrement_puts_without_answer();
        return true;
    }
    if (is_auto_strategy) {
        return decrement_puts_without_answer();
    }
    return true;
}

bool ClientLogic::processPenaltyMessage(PenaltyMessage* msg) {
    log_stdout("Received penalty response (" + Message::doubleToString(msg->getValue()) +
               " in " + std::to_string(msg->getPoint()) + ")");
    return true;
}

bool ClientLogic::processScoringMessage(ScoringMessage* msg) {
    log_stdout("Game end, scoring: " +
               msg->toRawString().substr(std::string("SCORING ").length()));

    game_over.store(true);
    return true;
}

void ClientLogic::increment_puts_without_answer() {
    std::scoped_lock<std::mutex> lock(puts_without_answer_mutex);
    puts_without_answer++;
}

bool ClientLogic::decrement_puts_without_answer() {
    std::scoped_lock<std::mutex> lock(puts_without_answer_mutex);
    if (puts_without_answer > 0) {
        puts_without_answer--;
        if (puts_without_answer == 0) {
            waiting_for_put_response.notify_one();
        }
        return true;
    }
    return false;
}

bool ClientLogic::wait_for_puts(std::chrono::milliseconds timeout) {
    std::unique_lock<std::mutex> lock(puts_without_answer_mutex);
    waiting_for_put_response.wait_for(lock, timeout,
                                      [this] { return puts_without_answer == 0; });
    return puts_without_answer == 0;
}

void ClientLogic::send_put_message(int point, double value) {
    std::unique_ptr<Message> msg = PutMessage::createMessage(point, value);
    log_stdout("Putting " + Message::doubleToString(value) + " in point " +
               std::to_string(point));
    outgoing_messages.push(std::move(msg));
}

void ClientLogic::send_hello_message() {
    std::unique_ptr<Message> msg = HelloMessage::createMessage(player_id);
    outgoing_messages.push(std::move(msg));
}

std::pair<int, double> ClientLogic::get_best_put() {
    std::scoped_lock<std::mutex> lock(poly_value_mutex);
    // If K is not known yet, we can only safely put in points 0 and 1
    int max_point = K_set.load() ? K.load() : 1;

    std::vector<double> squared_differences;
    squared_differences.reserve(max_point + 1);

    std::transform(current_approximation.begin(),
                   current_approximation.begin() + max_point + 1, real_values.begin(),
                   std::back_inserter(squared_differences),
                   [](double a, double b) { return (a - b) * (a - b); });

    auto max_it = std::max_element(squared_differences.begin(), squared_differences.end());
    int max_idx = std::distance(squared_differences.begin(), max_it);
    double diff = real_values[max_idx] - current_approximation[max_idx];
    double value_to_put = std::clamp(diff, constants::min_put_value, constants::max_put_value);

    current_approximation[max_idx] += value_to_put;
    return std::make_pair(max_idx, value_to_put);
}

double ClientLogic::poly_at(int x) {
    double result = 0;
    int x_pow = 1;
    for (int i = 0; i <= N; i++) {
        result += coeffs[i] * x_pow;
        x_pow *= x;
    }

    return result;
}
