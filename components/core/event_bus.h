#ifndef CORE_EVENT_BUS_H
#define CORE_EVENT_BUS_H

#include "esp_err.h"
#include <string>
#include <map>
#include <vector>
#include <functional>
#include <cstdint>

using EventCallback = std::function<void(const std::string& event_name, void* event_data)>;
using EventSubscriptionHandle = uint32_t;

class EventBus {
public:
    static esp_err_t init(size_t queue_size = 10, uint32_t task_stack_size = 4096);
    static esp_err_t publish(const std::string& event_name, void* event_data = nullptr);
    static EventSubscriptionHandle subscribe(const std::string& event_name, EventCallback callback);
    static void unsubscribe(EventSubscriptionHandle handle);
};

#endif // CORE_EVENT_BUS_H 