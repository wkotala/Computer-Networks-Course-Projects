#include "server_logic.h"

#include <cassert>
#include <iostream>
#include <sstream>
#include <string>

#include "constants.h"
#include "error.h"
#include "server_events.h"

ServerLogic::ServerLogic(int K, int N, int M, const std::string& file_name,
                         EventManager& event_manager)
    : K(K),
      N(N),
      M(M),
      file_name(file_name),
      total_correct_puts(0),
      coeff_file(file_name, std::ios_base::in),
      players(),
      event_manager(event_manager),
      stopping(false) {
    if (coeff_file.rdstate() == std::ios_base::failbit || !coeff_file.is_open()) {
        syserr("could not open coefficients file: %s", file_name.c_str());
    }
}

void ServerLogic::register_new_client(int client_fd, const std::string& ip, int port) {
    std::cout << "New client [" << ip << "]:" << port << std::endl;
    PlayerInfo new_player;
    new_player.id = "UNKNOWN";
    new_player.ip = ip;
    new_player.port = port;
    new_player.messages = std::deque<std::string>();
    new_player.approximations = std::vector<double>(K + 1, 0.0);
    new_player.coefficients = std::vector<double>(N + 1);
    new_player.penalty = 0.0;
    new_player.is_known = false;
    new_player.correct_puts = 0;
    new_player.can_put = false;
    new_player.delay = 0;

    players[client_fd] = std::move(new_player);
}

bool ServerLogic::is_client_connected(int client_fd) const {
    return players.find(client_fd) != players.end();
}

bool ServerLogic::validate_client(int client_fd, const std::string& ip, int port) const {
    if (!is_client_connected(client_fd)) {
        return false;
    }

    const PlayerInfo& player = players.at(client_fd);
    return player.ip == ip && player.port == port;
}

bool ServerLogic::has_pending_messages(int client_fd) const {
    assert(is_client_connected(client_fd));
    return !players.at(client_fd).messages.empty();
}

std::string ServerLogic::take_next_message_str(int client_fd) {
    assert(is_client_connected(client_fd));
    std::string msg_str = players.at(client_fd).messages.front();
    players[client_fd].messages.pop_front();
    return msg_str;
}

void ServerLogic::append_message_front(int client_fd, const std::string& msg) {
    assert(is_client_connected(client_fd));
    players[client_fd].messages.push_front(msg);
}

void ServerLogic::append_message_back(int client_fd, const std::string& msg) {
    assert(is_client_connected(client_fd));
    players[client_fd].messages.push_back(msg);
}

std::string ServerLogic::getClientPlayerID(int client_fd) const {
    assert(is_client_connected(client_fd));
    return players.at(client_fd).id;
}
std::string ServerLogic::getClientIP(int client_fd) const {
    assert(is_client_connected(client_fd));
    return players.at(client_fd).ip;
}
int ServerLogic::getClientPort(int client_fd) const {
    assert(is_client_connected(client_fd));
    return players.at(client_fd).port;
}
const PlayerInfo& ServerLogic::getPlayerInfo(int client_fd) const {
    assert(is_client_connected(client_fd));
    return players.at(client_fd);
}
bool ServerLogic::is_stopping() const {
    return stopping;
}

void ServerLogic::handle_client_disconnect(int client_fd) {
    assert(is_client_connected(client_fd));
    total_correct_puts -= players[client_fd].correct_puts;
    players.erase(client_fd);
}

bool ServerLogic::handle_client_message(int client_fd, std::unique_ptr<Message> msg) {
    assert(is_client_connected(client_fd));
    switch (msg->getType()) {
        case MessageType::HELLO:
            return handle_hello(client_fd, dynamic_cast<HelloMessage*>(msg.get()));
        case MessageType::PUT:
            return handle_put(client_fd, dynamic_cast<PutMessage*>(msg.get()));
        default: return false;
    }
}

bool ServerLogic::handle_hello(int client_fd, HelloMessage* msg) {
    PlayerInfo& player = players[client_fd];

    if (player.is_known) {
        return false;
    }

    player.id = msg->getPlayerId();
    player.delay = std::count_if(player.id.begin(), player.id.end(),
                                 [](char c) { return std::islower(c); });

    std::cout << "[" << player.ip << "]:" << player.port << " is now known as " << player.id
              << "." << std::endl;

    player.is_known = true;
    player.can_put = true;

    std::string coeffs_str;
    std::getline(coeff_file, coeffs_str, '\n');
    coeffs_str += '\n';

    std::unique_ptr<Message> coeff_msg = Message::createMessage(coeffs_str);
    if (!coeff_msg) {
        fatal("could not create coeff message");
    }

    player.coefficients = dynamic_cast<CoeffMessage*>(coeff_msg.get())->getCoeffs();

    std::cout << player.id << "'s coefficients are "
              << coeffs_str.substr(0, coeffs_str.find(constants::crlf)) << std::endl;

    append_message_back(client_fd, coeff_msg->getRawMessage());
    return true;
}

bool ServerLogic::handle_put(int client_fd, PutMessage* msg) {
    PlayerInfo& player = players[client_fd];

    if (!player.is_known) {
        return false;
    }

    bool successful_put = true;

    if (!player.can_put) {
        successful_put = false;
        std::cout << player.id << " tried to put " << msg->getValue() << " in "
                  << msg->getPoint() << " before it could put." << std::endl;
        respond_with_penalty(client_fd, msg->getPoint(), msg->getValue());
    }

    player.can_put = false;

    if (msg->getPoint() < 0 || msg->getPoint() > K ||
        msg->getValue() + constants::eps < constants::min_put_value ||
        msg->getValue() - constants::eps > constants::max_put_value) {
        successful_put = false;
        std::cout << player.id << " tried to put " << msg->getValue() << " in "
                  << msg->getPoint() << " which is out of range." << std::endl;
        respond_with_bad_put(client_fd, msg->getPoint(), msg->getValue());
    }

    if (!successful_put) {
        return false;
    }

    player.correct_puts++;
    total_correct_puts++;
    player.approximations[msg->getPoint()] += msg->getValue();

    std::unique_ptr<Message> state_msg =
        StateMessage::createMessage(this->players[client_fd].approximations);

    std::cout << player.id << " puts " << msg->getValue() << " in " << msg->getPoint()
              << ", current state "
              << state_msg->toRawString().substr(std::string("STATE ").length()) << std::endl;

    respond_with_state(client_fd, state_msg->getRawMessage());

    if (total_correct_puts >= M) {
        game_over();
    }

    return true;
}

void ServerLogic::respond_with_penalty(int client_fd, int point, double value) {
    PlayerInfo& player = players[client_fd];
    player.penalty += constants::early_put_penalty;
    player.can_put = true;
    std::unique_ptr<Message> penalty_msg = PenaltyMessage::createMessage(point, value);
    append_message_back(client_fd, penalty_msg->getRawMessage());
}

void ServerLogic::respond_with_bad_put(int client_fd, int point, double value) {
    PlayerInfo& player = players[client_fd];
    player.penalty += constants::bad_put_penalty;

    event_manager.add_event(
        [this, client_fd, point, value, client_ip = this->getClientIP(client_fd),
         client_port = this->getClientPort(client_fd),
         client_id = this->getClientPlayerID(client_fd)]() {
            if (!this->validate_client(client_fd, client_ip, client_port) ||
                this->getClientPlayerID(client_fd) != client_id) {
                return; // client disconnected
            }
            PlayerInfo& player = this->players[client_fd];
            player.can_put = true;
            std::unique_ptr<Message> bad_put_msg = BadPutMessage::createMessage(point, value);
            this->append_message_back(client_fd, bad_put_msg->getRawMessage());
        },
        std::chrono::steady_clock::now() + std::chrono::seconds(constants::bad_put_delay));
}

void ServerLogic::respond_with_state(int client_fd, const std::string& state_msg) {
    event_manager.add_event(
        [this, client_fd, state_msg, client_ip = this->getClientIP(client_fd),
         client_port = this->getClientPort(client_fd),
         client_id = this->getClientPlayerID(client_fd)]() {
            if (!this->validate_client(client_fd, client_ip, client_port) ||
                this->getClientPlayerID(client_fd) != client_id) {
                return; // client disconnected
            }
            this->append_message_back(client_fd, state_msg);
            std::cout << "Sending state "
                      << state_msg.substr(
                             std::string("STATE ").length(),
                             state_msg.find(constants::crlf) - std::string("STATE ").length())
                      << " to " << client_id << "." << std::endl;
            this->players[client_fd].can_put = true;
        },
        std::chrono::steady_clock::now() + std::chrono::seconds(players[client_fd].delay));
}

void ServerLogic::game_over() {
    send_scoring_messages();
    stopping = true;
}

void ServerLogic::send_scoring_messages() {
    std::vector<std::string> ids{};
    std::vector<double> scores{};

    for (const auto& [client_fd, player] : players) {
        if (player.is_known) {
            ids.push_back(player.id);
            scores.push_back(calculate_score(player));
        }
    }

    std::unique_ptr<Message> scoring_msg = ScoringMessage::createMessage(ids, scores);
    for (const auto& [client_fd, player] : players) {
        if (player.is_known) {
            append_message_back(client_fd, scoring_msg->getRawMessage());
        }
    }

    std::cout << "Game end, scoring: "
              << scoring_msg->toRawString().substr(std::string("SCORING ").length())
              << std::endl;
}

double ServerLogic::calculate_score(const PlayerInfo& player) {
    double score = 0.0;
    for (int x = 0; x <= K; x++) {
        double real_value = player_poly_at(player, x);
        double approx_value = player.approximations[x];
        score += (real_value - approx_value) * (real_value - approx_value);
    }
    return score + player.penalty;
}

double ServerLogic::player_poly_at(const PlayerInfo& player, int x) const {
    double result = 0;
    double x_pow = 1;
    for (int i = 0; i <= N; i++) {
        result += player.coefficients[i] * x_pow;
        x_pow *= x;
    }
    return result;
}

void ServerLogic::reset() {
    event_manager.reset();
    total_correct_puts = 0;
    players.clear();
    stopping = false;
}
