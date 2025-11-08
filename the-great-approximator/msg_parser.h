#ifndef MSG_PARSER_H
#define MSG_PARSER_H

#include <memory>
#include <string>
#include <vector>

#include "constants.h"

enum class MessageType { HELLO, COEFF, PUT, BAD_PUT, STATE, PENALTY, SCORING };

class Message {
 public:
    Message() = default;
    virtual ~Message() = default;

    // Simple getters.
    const std::string& getRawMessage() const { return raw_message; }
    const std::vector<std::string>& getParams() const { return params; }
    MessageType getType() const { return type; }

    // Returns raw message without CRLF.
    std::string toRawString() const {
        return raw_message.substr(0, raw_message.size() - constants::crlf.length());
    }

    // Factory methods.
    static std::unique_ptr<Message> createMessage(const std::string& line);
    static std::unique_ptr<Message> createMessageWithCRLF(const std::string& line) {
        return createMessage(line + constants::crlf);
    }

    // Returns whether string is alphanumeric.
    static bool isAlphanumeric(const std::string& str);

    // Returns whether string is a valid integer.
    // If it is, saves it to out_val.
    static bool parseInteger(const std::string& str, int& out_val);

    // Returns whether string is a valid double.
    // If it is, saves it to out_val.
    static bool parseDouble(const std::string& str, double& out_val);

    // Divides string into command and parameters.
    // On success, saves command and parameters to out_command and out_params, returning true.
    static bool extractCommandAndParams(const std::string& line, std::string& out_command,
                                        std::string& out_params);

    // Splits parameters into a vector of strings.
    // On success, saves parameters to out_vec, returning true.
    static bool splitParams(const std::string& params, std::vector<std::string>& out_vec);

    // Validates whether parameters given by vector are a valid integer and double pair.
    // If they are, saves them to out_point and out_value, returning true.
    static bool validateIntDoublePair(const std::vector<std::string>& vec, int& out_point,
                                      double& out_value);

    // Converts double to string with precision constants::max_fractional_digits.
    static std::string doubleToString(double val);

 protected:
    bool validateIntDoublePairInParams(int& out_point, double& out_value);
    void setType(MessageType type) { this->type = type; }

 private:
    std::string raw_message; // contains CRLF
    std::vector<std::string> params;
    MessageType type;

    static bool isValidIntegerStringFormat(const std::string& str);
    static bool isValidDoubleStringFormat(const std::string& str);

    virtual bool parseMessage() = 0;
};

class HelloMessage : public Message {
 public:
    static std::unique_ptr<Message> createMessage(const std::string& player_id);
    const std::string& getPlayerId() const { return player_id; }

 private:
    std::string player_id;

    bool parseMessage() override;
};

class CoeffMessage : public Message {
 public:
    static std::unique_ptr<Message> createMessage(const std::vector<double>& coeffs);
    const std::vector<double>& getCoeffs() const { return coeffs; }

 private:
    std::vector<double> coeffs;

    bool parseMessage() override;
};

class PutMessage : public Message {
 public:
    static std::unique_ptr<Message> createMessage(int point, double value);
    int getPoint() const { return point; }
    double getValue() const { return value; }

 private:
    int point;
    double value;

    // Does not check if point and value are in correct range.
    bool parseMessage() override;
};

class BadPutMessage : public Message {
 public:
    static std::unique_ptr<Message> createMessage(int point, double value);
    int getPoint() const { return point; }
    double getValue() const { return value; }

 private:
    int point;
    double value;

    // Does not check if point and value are in correct range.
    bool parseMessage() override;
};

class StateMessage : public Message {
 public:
    static std::unique_ptr<Message> createMessage(const std::vector<double>& approx_values);
    const std::vector<double>& getApproxValues() const { return approx_values; }

 private:
    std::vector<double> approx_values;

    // Does not check if values are correct nor if there is correct number of them
    bool parseMessage() override;
};

class PenaltyMessage : public Message {
 public:
    static std::unique_ptr<Message> createMessage(int point, double value);
    int getPoint() const { return point; }
    double getValue() const { return value; }

 private:
    int point;
    double value;

    // Does not check if point and value are in correct range.
    bool parseMessage() override;
};

class ScoringMessage : public Message {
 public:
    static std::unique_ptr<Message> createMessage(const std::vector<std::string>& player_ids,
                                                  const std::vector<double>& scores);
    const std::vector<std::string>& getPlayerIds() const { return player_ids; }
    const std::vector<double>& getScores() const { return scores; }

 private:
    std::vector<std::string> player_ids;
    std::vector<double> scores;

    bool parseMessage() override;
};

#endif // MSG_PARSER_H