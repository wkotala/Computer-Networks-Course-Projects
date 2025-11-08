#ifndef SERVER_EVENTS_H
#define SERVER_EVENTS_H

#include <chrono>
#include <functional>
#include <map>
#include <string>

class EventManager {
 public:
    EventManager() : events() {}

    // Adds an event to the event manager.
    void add_event(std::function<void()> callback,
                   std::chrono::steady_clock::time_point deadline);

    // Calls all events that are due.
    void check_timers();

    // Resets the event manager.
    void reset();

 private:
    std::multimap<std::chrono::steady_clock::time_point, std::function<void()>> events;
};

#endif // SERVER_EVENTS_H