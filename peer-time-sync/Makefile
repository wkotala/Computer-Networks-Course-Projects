# C compiler and flags
CC = gcc
CFLAGS = -Wall -Wextra -O2 -std=gnu17 -DNDEBUG

# C++ compiler and flags
CXX = g++
CXXFLAGS = -Wall -Wextra -O2 -std=gnu++17 -DNDEBUG


TARGET = peer-time-sync
OBJS = main.o err.o networking.o arg_parser.o messages.o peers.o clock.o

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^

# Settings for debug build
debug: CFLAGS = -Wall -Wextra -std=gnu17 -g
debug: CXXFLAGS = -Wall -Wextra -std=gnu++17 -g
debug: $(TARGET)

# Dependencies
arg_parser.o: arg_parser.c arg_parser.h err.h node_data.h networking.h
clock.o: clock.c clock.h node_data.h err.h
err.o: err.c err.h node_data.h
main.o: main.c arg_parser.h clock.h node_data.h err.h messages.h networking.h peers.h
messages.o: messages.c messages.h node_data.h clock.h err.h networking.h peers.h
networking.o: networking.c networking.h err.h node_data.h
peers.o: peers.cpp peers.h err.h node_data.h

clean:
	rm -f $(TARGET) $(OBJS)

.PHONY: all clean debug