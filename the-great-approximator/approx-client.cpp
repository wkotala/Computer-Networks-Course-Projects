#include <unistd.h>

#include <iomanip>
#include <ios>
#include <iostream>
#include <string>

#include "arg_parser.h"
#include "client_logic.h"
#include "networking.h"

int make_connection(ClientLogic& logic, ClientArgParser& arg_parser) {
    std::string server_ip;
    int server_port;
    int sockfd = connect_to_server(
        arg_parser.getServerAddress(), std::to_string(arg_parser.getServerPort()),
        arg_parser.isIPv4Forced(), arg_parser.isIPv6Forced(), server_ip, server_port);

    set_receive_timeout(sockfd, constants::client_timeout.count());

    logic.register_connection(server_ip, server_port, sockfd);

    return sockfd;
}

int main(int argc, char* argv[]) {
    std::cout << std::fixed << std::setprecision(constants::max_fractional_digits);
    std::cerr << std::fixed << std::setprecision(constants::max_fractional_digits);

    ClientArgParser arg_parser(argc, argv);
    arg_parser.logInfo();

    ClientLogic logic(arg_parser.getPlayerId(), arg_parser.isAutoStrategy());
    int sockfd = make_connection(logic, arg_parser);

    logic.start_threads_and_send_hello();
    logic.join_threads();

    close(sockfd);
    return 0;
}