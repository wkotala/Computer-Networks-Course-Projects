#include "msg_parser.h"

#include <algorithm>
#include <cctype>
#include <iomanip>
#include <ios>
#include <limits>
#include <regex>
#include <sstream>
#include <utility>

bool Message::isAlphanumeric(const std::string& str) {
    return std::all_of(str.begin(), str.end(),
                       [](char c) { return std::isalnum(static_cast<unsigned char>(c)); });
}

bool Message::isValidIntegerStringFormat(const std::string& str) {
    if (str.empty())
        return false;
    size_t start_idx = 0;
    if (str.front() == '-') {
        if (str.length() == 1)
            return false;
        start_idx = 1;
    }
    return std::all_of(str.begin() + start_idx, str.end(), [](unsigned char c) {
        return std::isdigit(static_cast<unsigned char>(c));
    });
}

bool Message::parseInteger(const std::string& str, int& out_val) {
    if (!isValidIntegerStringFormat(str))
        return false;
    try {
        size_t processed_chars = 0;
        long long temp_val = std::stoll(str, &processed_chars);
        if (processed_chars != str.length())
            return false;
        if (temp_val < std::numeric_limits<int>::min() ||
            temp_val > std::numeric_limits<int>::max())
            return false;
        out_val = static_cast<int>(temp_val);
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

bool Message::isValidDoubleStringFormat(const std::string& str) {
    if (str.empty())
        return false;
    size_t i = 0;
    if (str.front() == '-') {
        if (str.length() == 1)
            return false;
        i = 1;
    }

    bool has_leading_digits = false;
    while (i < str.length() && std::isdigit(static_cast<unsigned char>(str[i]))) {
        has_leading_digits = true;
        i++;
    }

    if (i < str.length() && str[i] == '.') {
        i++;
    }

    bool has_fractional_digits = false;
    size_t fractional_digit_count = 0;
    while (i < str.length() && std::isdigit(static_cast<unsigned char>(str[i]))) {
        has_fractional_digits = true;
        fractional_digit_count++;
        if (fractional_digit_count > constants::max_fractional_digits)
            return false;
        i++;
    }

    if (!has_leading_digits && !has_fractional_digits) // must have at least one digit part
        return false;

    if (i != str.length()) // all characters consumed?
        return false;

    return true;
}

bool Message::parseDouble(const std::string& str, double& out_val) {
    if (!isValidDoubleStringFormat(str))
        return false;
    try {
        size_t processed_chars = 0;
        out_val = std::stod(str, &processed_chars);
        return processed_chars == str.length();
    } catch (const std::exception&) {
        return false;
    }
}

bool Message::extractCommandAndParams(const std::string& line, std::string& out_command,
                                      std::string& out_params) {
    if (line.empty())
        return false;

    size_t space_pos = line.find(' ');
    if (space_pos == std::string::npos) { // no parameters, whole line is a command
        out_command = line;
        out_params.clear();
        return true;
    } else if (space_pos == 0) { // no command
        return false;
    } else if (space_pos + 1 == line.length()) { // "CMD " with nothing after
        return false;
    } else {
        out_command = line.substr(0, space_pos);
        out_params = line.substr(space_pos + 1);
        return true;
    }
}

bool Message::splitParams(const std::string& params, std::vector<std::string>& out_vec) {
    out_vec.clear();
    if (params.empty()) {
        return true;
    }

    static const std::regex params_regex("([a-zA-Z0-9\\-\\.]+ )*[a-zA-Z0-9\\-\\.]+");
    if (!std::regex_match(params, params_regex)) {
        return false;
    }

    std::istringstream iss(params);

    std::string param;
    while (iss >> param) {
        out_vec.push_back(param);
    }

    return true;
}

std::unique_ptr<Message> Message::createMessage(const std::string& line) {
    if (line.size() < 2 || line.substr(line.size() - 2) != constants::crlf) {
        return nullptr; // missing CRLF
    }

    std::string cmd_params_str = line.substr(0, line.size() - 2);
    std::string command_str, params_str;
    if (!extractCommandAndParams(cmd_params_str, command_str, params_str)) {
        return nullptr;
    }

    std::vector<std::string> params;
    if (!splitParams(params_str, params)) {
        return nullptr;
    }

    std::unique_ptr<Message> msg;
    if (command_str == "HELLO") {
        msg = std::make_unique<HelloMessage>();
    } else if (command_str == "COEFF") {
        msg = std::make_unique<CoeffMessage>();
    } else if (command_str == "PUT") {
        msg = std::make_unique<PutMessage>();
    } else if (command_str == "BAD_PUT") {
        msg = std::make_unique<BadPutMessage>();
    } else if (command_str == "STATE") {
        msg = std::make_unique<StateMessage>();
    } else if (command_str == "PENALTY") {
        msg = std::make_unique<PenaltyMessage>();
    } else if (command_str == "SCORING") {
        msg = std::make_unique<ScoringMessage>();
    } else {
        return nullptr; // unknown command
    }

    if (!msg)
        return nullptr;

    msg->raw_message = line;
    msg->params = std::move(params);

    if (msg->parseMessage()) {
        return msg;
    }

    return nullptr;
}

bool Message::validateIntDoublePair(const std::vector<std::string>& vec, int& out_point,
                                    double& out_value) {
    if (vec.size() != 2) {
        return false;
    }

    if (!parseInteger(vec[0], out_point) || !parseDouble(vec[1], out_value)) {
        return false;
    }

    return true;
}

std::string Message::doubleToString(double val) {
    std::stringstream ss;
    ss << std::fixed << std::setprecision(constants::max_fractional_digits) << val;
    return ss.str();
}

bool Message::validateIntDoublePairInParams(int& out_point, double& out_value) {
    return validateIntDoublePair(getParams(), out_point, out_value);
}

bool HelloMessage::parseMessage() {
    setType(MessageType::HELLO);
    const std::vector<std::string>& params = getParams();

    if (params.size() != 1 || !isAlphanumeric(params[0])) {
        return false;
    }

    player_id = params[0];
    return true;
}

bool CoeffMessage::parseMessage() {
    setType(MessageType::COEFF);
    const std::vector<std::string>& params = getParams();

    if (params.size() < 1 || params.size() > constants::max_n + 1) {
        return false;
    }

    coeffs.clear();
    coeffs.reserve(params.size());

    for (const std::string& param : params) {
        coeffs.push_back(0.0);
        if (!parseDouble(param, coeffs.back())) {
            return false;
        }
        if (coeffs.back() + constants::eps < constants::min_coeff ||
            coeffs.back() - constants::eps > constants::max_coeff) {
            return false;
        }
    }

    return true;
}

bool PutMessage::parseMessage() {
    setType(MessageType::PUT);
    return validateIntDoublePairInParams(point, value);
}

bool BadPutMessage::parseMessage() {
    setType(MessageType::BAD_PUT);
    return validateIntDoublePairInParams(point, value);
}

bool StateMessage::parseMessage() {
    setType(MessageType::STATE);
    const std::vector<std::string>& params = getParams();

    if (params.size() < 1 || params.size() > constants::max_k + 1) {
        return false;
    }

    approx_values.clear();
    approx_values.reserve(params.size());

    for (const std::string& param : params) {
        approx_values.push_back(0.0);
        if (!parseDouble(param, approx_values.back())) {
            return false;
        }
    }

    return true;
}

bool PenaltyMessage::parseMessage() {
    setType(MessageType::PENALTY);
    return validateIntDoublePairInParams(point, value);
}

bool ScoringMessage::parseMessage() {
    setType(MessageType::SCORING);
    const std::vector<std::string>& params = getParams();
    size_t num_players = params.size() / 2;

    if (num_players * 2 != params.size()) {
        return false;
    }

    player_ids.clear();
    player_ids.reserve(num_players);
    scores.clear();
    scores.reserve(num_players);

    for (size_t i = 0; i < num_players; ++i) {
        player_ids.push_back(params[i * 2]);
        scores.push_back(0.0);
        if (!isAlphanumeric(player_ids.back())) {
            return false;
        }
        if (!parseDouble(params[i * 2 + 1], scores.back())) {
            return false;
        }
    }

    return true;
}

std::unique_ptr<Message> HelloMessage::createMessage(const std::string& player_id) {
    return createMessageWithCRLF("HELLO " + player_id);
}

std::unique_ptr<Message> CoeffMessage::createMessage(const std::vector<double>& coeffs) {
    if (coeffs.empty()) {
        return nullptr;
    }

    std::string coeffs_str = "";
    for (const auto& coeff : coeffs) {
        coeffs_str += doubleToString(coeff) + " ";
    }
    coeffs_str.pop_back(); // remove last space
    return createMessageWithCRLF("COEFF " + coeffs_str);
}

std::unique_ptr<Message> PutMessage::createMessage(int point, double value) {
    return createMessageWithCRLF("PUT " + std::to_string(point) + " " + doubleToString(value));
}

std::unique_ptr<Message> BadPutMessage::createMessage(int point, double value) {
    return createMessageWithCRLF("BAD_PUT " + std::to_string(point) + " " +
                                 doubleToString(value));
}

std::unique_ptr<Message> StateMessage::createMessage(
    const std::vector<double>& approx_values) {
    if (approx_values.empty()) {
        return nullptr;
    }

    std::string approx_values_str = "";
    for (const auto& approx_value : approx_values) {
        approx_values_str += doubleToString(approx_value) + " ";
    }

    approx_values_str.pop_back(); // remove last space
    return createMessageWithCRLF("STATE " + approx_values_str);
}

std::unique_ptr<Message> PenaltyMessage::createMessage(int point, double value) {
    return createMessageWithCRLF("PENALTY " + std::to_string(point) + " " +
                                 doubleToString(value));
}

std::unique_ptr<Message> ScoringMessage::createMessage(
    const std::vector<std::string>& player_ids, const std::vector<double>& scores) {
    if (player_ids.size() != scores.size()) {
        return nullptr;
    }

    // Sort player_ids and scores by player_ids
    std::vector<std::pair<std::string, double>> player_scores;
    for (size_t i = 0; i < player_ids.size(); ++i) {
        player_scores.emplace_back(player_ids[i], scores[i]);
    }
    std::sort(player_scores.begin(), player_scores.end());

    // Create the message
    int num_players = player_scores.size();
    std::string msg_str = "SCORING ";
    for (int i = 0; i < num_players; ++i) {
        msg_str +=
            player_scores[i].first + " " + doubleToString(player_scores[i].second) + " ";
    }
    msg_str.pop_back(); // remove last space
    return createMessageWithCRLF(msg_str);
}