CXX = g++
CXXFLAGS = -std=c++17 -DNDEBUG

TARGET_SERVER = approx-server
TARGET_CLIENT = approx-client

OBJS_COMMON = arg_parser.o err.o msg_parser.o networking.o

OBJS_SERVER = approx-server.o $(OBJS_COMMON) server_logic.o server_events.o
OBJS_CLIENT = approx-client.o $(OBJS_COMMON) client_logic.o

all: $(TARGET_CLIENT) $(TARGET_SERVER)

$(TARGET_SERVER): $(OBJS_SERVER)
	$(CXX) $(CXXFLAGS) -o $@ $^

$(TARGET_CLIENT): $(OBJS_CLIENT)
	$(CXX) $(CXXFLAGS) -o $@ $^

# Settings for debug build
debug: CXXFLAGS = -Wall -Wextra -Werror -pedantic -std=c++17 -g
debug: all

# Dependencies
approx-client.o: approx-client.cpp arg_parser.h err.h client_logic.h \
 msg_parser.h constants.h ts_queue.h networking.h
approx-server.o: approx-server.cpp arg_parser.h err.h constants.h \
 msg_parser.h networking.h server_events.h server_logic.h
arg_parser.o: arg_parser.cpp arg_parser.h err.h constants.h
client_logic.o: client_logic.cpp client_logic.h msg_parser.h constants.h \
 ts_queue.h err.h
err.o: err.cpp err.h
msg_parser.o: msg_parser.cpp msg_parser.h constants.h
networking.o: networking.cpp networking.h err.h
server_events.o: server_events.cpp server_events.h server_logic.h \
 arg_parser.h err.h msg_parser.h constants.h
server_logic.o: server_logic.cpp server_logic.h arg_parser.h err.h \
 msg_parser.h constants.h server_events.h

clean:
	rm -f $(OBJS_SERVER) $(OBJS_CLIENT) $(TARGET_SERVER) $(TARGET_CLIENT)

.PHONY: all clean