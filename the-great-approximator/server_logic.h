#ifndef SERVER_LOGIC_H
#define SERVER_LOGIC_H

#include <deque>
#include <fstream>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "arg_parser.h"
#include "msg_parser.h"
#include "server_events.h"

struct PlayerInfo {
    std::string id;
    std::string ip;
    int port;
    std::deque<std::string> messages;
    std::vector<double> approximations;
    std::vector<double> coefficients;
    double penalty;
    bool is_known;
    int correct_puts;
    bool can_put;
    int delay; // number of small letters in player id
};

class ServerLogic {
 public:
    ServerLogic(int K, int N, int M, const std::string& file_name,
                EventManager& event_manager);

    // Dealing with clients.
    void register_new_client(int client_fd, const std::string& ip, int port);
    void handle_client_disconnect(int client_fd);

    // Dealing with messages.
    bool has_pending_messages(int client_fd) const;
    std::string take_next_message_str(int client_fd);
    void append_message_front(int client_fd, const std::string& msg);
    void append_message_back(int client_fd, const std::string& msg);

    // Simple getters.
    std::string getClientPlayerID(int client_fd) const;
    std::string getClientIP(int client_fd) const;
    int getClientPort(int client_fd) const;
    const PlayerInfo& getPlayerInfo(int client_fd) const;

    // Returns true if the server is stopping due to game over (#puts == M).
    bool is_stopping() const;

    // Handles message from client.
    // Returns false if message was unexpected at this point.
    bool handle_client_message(int client_fd, std::unique_ptr<Message> msg);

    // Resets the server state.
    void reset();

    // Returns whether client_fd refers to a valid client with given IP and port.
    // Useful when scheduling events in the future, when client might have disconnected.
    bool validate_client(int client_fd, const std::string& ip, int port) const;

 private:
    int K;
    int N;
    int M;
    std::string file_name;
    int total_correct_puts;
    std::ifstream coeff_file;
    std::map<int, PlayerInfo> players; // client_fd -> PlayerInfo
    EventManager& event_manager;
    bool stopping;

    bool handle_hello(int client_fd, HelloMessage* msg);
    bool handle_put(int client_fd, PutMessage* msg);

    void game_over(); // called when #puts == M
    void respond_with_penalty(int client_fd, int point, double value);
    void respond_with_bad_put(int client_fd, int point, double value);
    void respond_with_state(int client_fd, const std::string& state_msg);
    void send_scoring_messages();
    double calculate_score(const PlayerInfo& player);
    double player_poly_at(const PlayerInfo& player, int x) const;
    bool is_client_connected(int client_fd) const;
};

#endif // SERVER_LOGIC_H