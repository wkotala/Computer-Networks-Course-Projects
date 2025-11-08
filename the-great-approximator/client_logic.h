#ifndef CLIENT_LOGIC_H
#define CLIENT_LOGIC_H

#include <atomic>
#include <condition_variable>
#include <string>
#include <thread>
#include <vector>

#include "msg_parser.h"
#include "ts_queue.h"

class ClientLogic {
 public:
    ClientLogic(const std::string& player_id, bool is_auto_strategy);

    void register_connection(const std::string& server_ip, int server_port, int sockfd);
    void start_threads_and_send_hello();
    void join_threads();

 private:
    std::string player_id;       // set in constructor
    bool is_auto_strategy;       // set in constructor
    int sockfd;                  // set in register_connection()
    std::string server_ip;       // set in register_connection()
    int server_port;             // set in register_connection()
    std::string server_info;     // set in register_connection()
    std::string full_info;       // set in register_connection()
    std::vector<double> coeffs;  // set by first COEFF message internally
    int N;                       // set by first COEFF message internally
    std::atomic<bool> game_over; // initialized in constructor

    ThreadSafeQueue<std::unique_ptr<Message>> incoming_messages;
    ThreadSafeQueue<std::unique_ptr<Message>> outgoing_messages;
    ThreadSafeQueue<std::pair<std::string, bool>> logs; // (message, is_error)

    // For auto strategy.
    std::atomic<int> K;
    std::atomic<bool> K_set;
    int puts_without_answer;
    std::mutex puts_without_answer_mutex;
    std::condition_variable waiting_for_put_response;
    void increment_puts_without_answer();
    bool decrement_puts_without_answer();
    bool wait_for_puts(std::chrono::milliseconds timeout);
    std::vector<double> current_approximation;
    std::vector<double> real_values;
    std::mutex poly_value_mutex;

    // Threads.
    std::thread log_thread;
    std::thread strategy_thread;
    std::thread network_receiver_thread;
    std::thread network_sender_thread;
    std::thread message_processor_thread;
    void log_printer();
    void manual_strategy();
    void auto_strategy();
    void network_receiver();
    void network_sender();
    void message_processor();
    void join_thread(std::thread& thread);

    // Message processing.
    bool processCoeffMessage(CoeffMessage* msg);
    bool processBadPutMessage(BadPutMessage* msg);
    bool processStateMessage(StateMessage* msg);
    bool processPenaltyMessage(PenaltyMessage* msg);
    bool processScoringMessage(ScoringMessage* msg);

    // Put messages in outgoing_messages queue, handled by network_sender_thread
    void send_put_message(int point, double value);
    void send_hello_message();

    // For auto strategy.
    std::pair<int, double> get_best_put();
    double poly_at(int x);

    // Logging.
    void log_stdout(const std::string& msg);
    void log_stderr(const std::string& msg);
    void print_log_to_console(std::pair<std::string, bool>& log);
};

#endif // CLIENT_LOGIC_H