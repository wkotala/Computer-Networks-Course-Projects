#include <arpa/inet.h>
#include <poll.h>
#include <unistd.h>

#include <iomanip>
#include <ios>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <thread> // server is not multithreaded; this is imported for `sleep_for` only
#include <vector>

#include "arg_parser.h"
#include "constants.h"
#include "msg_parser.h"
#include "networking.h"
#include "server_events.h"
#include "server_logic.h"

namespace {
std::map<int, std::string> client_buffers;
constexpr size_t buffer_size = 65535;
char buffer[buffer_size];
std::vector<struct pollfd> poll_fds;
} // namespace

void disconnect_client(int client_fd, size_t& i, ServerLogic& server_logic,
                       std::string player_id) {
    std::cout << "Disconnecting " << player_id << std::endl;
    server_logic.handle_client_disconnect(client_fd);
    close(client_fd);
    poll_fds.erase(poll_fds.begin() + i);
    client_buffers.erase(client_fd);
    i--; // adjust index after erasing
}

void handle_new_connection(int listening_fd, ServerLogic& server_logic,
                           EventManager& event_manager) {
    struct sockaddr_storage client_addr;
    socklen_t client_addr_len = sizeof(client_addr);

    int client_fd = accept_new_connection(listening_fd, &client_addr, &client_addr_len);
    if (client_fd < 0) {
        return;
    }

    // Get client IP and port
    char ip_str[INET6_ADDRSTRLEN]; // enough for IPv4 and IPv6
    int port;
    if (client_addr.ss_family == AF_INET) {
        struct sockaddr_in* ipv4 = (struct sockaddr_in*)&client_addr;
        inet_ntop(AF_INET, &(ipv4->sin_addr), ip_str, INET_ADDRSTRLEN);
        port = ntohs(ipv4->sin_port);
    } else {
        struct sockaddr_in6* ipv6 = (struct sockaddr_in6*)&client_addr;
        inet_ntop(AF_INET6, &(ipv6->sin6_addr), ip_str, INET6_ADDRSTRLEN);
        port = ntohs(ipv6->sin6_port);
    }

    // Initially we only want to read (HELLO) from client.
    poll_fds.push_back({client_fd, POLLIN, 0});

    server_logic.register_new_client(client_fd, ip_str, port);
    client_buffers[client_fd] = "";

    // Wait for hello message
    event_manager.add_event(
        [&server_logic, client_fd, ip_str, port]() {
            if (!server_logic.validate_client(client_fd, ip_str, port)) {
                return; // client disconnected
            }

            if (!server_logic.getPlayerInfo(client_fd).is_known) {
                size_t i = 0;
                for (const auto& pollfd : poll_fds) {
                    if (pollfd.fd == client_fd) {
                        break;
                    }
                    i++;
                }
                std::cout << "Did not receive hello from [" << ip_str << "]:" << port << "."
                          << std::endl;
                disconnect_client(client_fd, i, server_logic,
                                  server_logic.getClientPlayerID(client_fd));
            }
        },
        std::chrono::steady_clock::now() + std::chrono::seconds(constants::hello_wait_time));
}

// Returns whether the client is still connected
bool handle_read_from_client(ServerLogic& server_logic, size_t& i) {
    auto& pollfd = poll_fds[i];
    ssize_t bytes_read = recv(pollfd.fd, buffer, buffer_size, 0);
    const std::string player_id = server_logic.getClientPlayerID(pollfd.fd);

    if (bytes_read < 0) {
        error("error reading from client %s", player_id.c_str());
        disconnect_client(pollfd.fd, i, server_logic, player_id);
        return false;
    } else if (bytes_read == 0) {
        disconnect_client(pollfd.fd, i, server_logic, player_id);
        return false;
    }

    // successful read from client
    client_buffers[pollfd.fd].append(buffer, bytes_read);

    size_t crlf_pos = client_buffers[pollfd.fd].find(constants::crlf);
    while (crlf_pos != std::string::npos) {
        std::string msg_str = client_buffers[pollfd.fd].substr(0, crlf_pos);
        client_buffers[pollfd.fd].erase(0, crlf_pos + constants::crlf.size());
        crlf_pos = client_buffers[pollfd.fd].find(constants::crlf);

        std::unique_ptr<Message> msg = Message::createMessageWithCRLF(msg_str);
        if (!msg || !server_logic.handle_client_message(pollfd.fd, std::move(msg))) {
            error("bad message from [%s]:%d, %s: %s",
                  server_logic.getClientIP(pollfd.fd).c_str(),
                  server_logic.getClientPort(pollfd.fd), player_id.c_str(), msg_str.c_str());
        }

        if (!server_logic.getPlayerInfo(pollfd.fd).is_known) {
            std::cout << "Client sent message before hello." << std::endl;
            disconnect_client(pollfd.fd, i, server_logic, player_id);
            break;
        }

        if (server_logic.is_stopping()) {
            break;
        }
    }

    return true;
}

// Returns whether the client is still connected
bool handle_write_to_client(ServerLogic& server_logic, size_t& i) {
    auto& pollfd = poll_fds[i];
    const std::string player_id = server_logic.getClientPlayerID(pollfd.fd);

    if (!server_logic.has_pending_messages(pollfd.fd)) {
        pollfd.events &= ~POLLOUT; // no need to listen for write events
        return true;
    }

    std::string msg_str = server_logic.take_next_message_str(pollfd.fd);
    if (msg_str.empty()) {
        return true;
    }

    ssize_t bytes_written = send(pollfd.fd, msg_str.c_str(), msg_str.size(), 0);

    if (bytes_written < 0) {
        error("error writing to client %s", player_id.c_str());
        disconnect_client(pollfd.fd, i, server_logic, player_id);
        return false;
    } else if (bytes_written == 0) {
        disconnect_client(pollfd.fd, i, server_logic, player_id);
        return false;
    }

    // Successful write to client.
    std::string msg_rest = msg_str.substr(bytes_written);
    if (!msg_rest.empty()) {
        server_logic.append_message_front(pollfd.fd, msg_rest);
    }

    return true;
}

// This function is called when the server is stopping.
// It tries to send all pending messages (SCORING) and disconnects all clients.
// After one second, server begins a new game.
void reset_server(ServerLogic& server_logic) {
    // Send pending messages to clients
    for (size_t i = 1; i < poll_fds.size(); i++) {
        auto& pollfd = poll_fds[i];
        while (server_logic.has_pending_messages(pollfd.fd)) {
            std::string msg_str = server_logic.take_next_message_str(pollfd.fd);
            if (msg_str.empty()) {
                break;
            }
            ssize_t bytes_written = send(pollfd.fd, msg_str.c_str(), msg_str.size(), 0);
            if (bytes_written != (ssize_t)msg_str.size()) {
                break;
            }
        }
    }

    // Disconnect clients
    for (size_t i = 1; i < poll_fds.size(); i++) {
        auto& pollfd = poll_fds[i];
        close(pollfd.fd);
    }
    poll_fds.resize(1); // only listening socket remains
    client_buffers.clear();

    // Wait for one second before starting a new game
    std::this_thread::sleep_for(std::chrono::milliseconds(constants::reset_delay));
}

int main(int argc, char* argv[]) {
    std::cout << std::fixed << std::setprecision(constants::max_fractional_digits);
    std::cerr << std::fixed << std::setprecision(constants::max_fractional_digits);
    ServerArgParser arg_parser(argc, argv);
    arg_parser.logInfo();

    int listening_fd =
        setup_listening_socket(arg_parser.getPort(), constants::listening_socket_backlog);

    poll_fds.push_back({listening_fd, POLLIN, 0});

    EventManager event_manager{};
    ServerLogic server_logic(arg_parser.getK(), arg_parser.getN(), arg_parser.getM(),
                             arg_parser.getFile(), event_manager);

    constexpr int poll_timeout = 100; // milliseconds
    while (true) {
        int ready = poll(poll_fds.data(), poll_fds.size(), poll_timeout);
        if (ready < 0) {
            syserr("poll");
        }

        if (server_logic.is_stopping()) {
            reset_server(server_logic);
            server_logic.reset();
            continue;
        }

        event_manager.check_timers();

        for (size_t i = 1; i < poll_fds.size(); i++) {
            if (server_logic.has_pending_messages(poll_fds[i].fd)) {
                poll_fds[i].events |= POLLOUT;
            }
        }

        if (ready == 0) { // no revents (poll timeout)
            continue;
        }

        if (poll_fds[0].revents & POLLIN) { // new connection
            handle_new_connection(listening_fd, server_logic, event_manager);
        }

        for (size_t i = 1; i < poll_fds.size(); i++) { // client sockets
            auto& pollfd = poll_fds[i];

            if (pollfd.revents & POLLHUP) {
                disconnect_client(pollfd.fd, i, server_logic,
                                  server_logic.getClientPlayerID(pollfd.fd));
                continue;
            }

            if (pollfd.revents & (POLLIN | POLLERR)) {
                if (!handle_read_from_client(server_logic, i)) {
                    continue;
                }
            }

            if (server_logic.is_stopping()) {
                break;
            }

            if (pollfd.revents & POLLOUT) {
                if (!handle_write_to_client(server_logic, i)) {
                    continue;
                }
            }
        } // client sockets
    } // main server loop

    for (const auto& pollfd : poll_fds) {
        close(pollfd.fd);
    }

    return 0;
}