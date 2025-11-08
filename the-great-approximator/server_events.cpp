#include "server_events.h"

#include <iostream>

#include "server_logic.h"

void EventManager::add_event(std::function<void()> callback,
                             std::chrono::steady_clock::time_point deadline) {
    events.insert({deadline, callback});
}

void EventManager::check_timers() {
    auto now = std::chrono::steady_clock::now();
    while (!events.empty() && events.begin()->first <= now) {
        events.begin()->second();     // call the callback
        events.erase(events.begin()); // remove the event
    }
}

void EventManager::reset() {
    events.clear();
}