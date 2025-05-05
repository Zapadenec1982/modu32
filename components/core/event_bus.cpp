#include "event_bus.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include <map>
#include <vector>
#include <string>
#include <functional>
#include <atomic>

static const char* TAG = "EventBus";

namespace {
    struct EventQueueItem {
        std::string name;
        void* data;
    };

    static std::map<std::string, std::vector<std::pair<EventSubscriptionHandle, EventCallback>>> subscribers;
    static std::map<EventSubscriptionHandle, std::string> handle_to_key;
    static std::atomic<uint32_t> next_handle{1};
    static SemaphoreHandle_t subs_mutex = nullptr;
    static QueueHandle_t event_queue = nullptr;
    static TaskHandle_t event_task_handle = nullptr;

    void event_handler_task(void* param) {
        while (true) {
            EventQueueItem item;
            if (xQueueReceive(event_queue, &item, portMAX_DELAY) == pdTRUE) {
                std::vector<std::pair<EventSubscriptionHandle, EventCallback>> callbacks;
                if (xSemaphoreTake(subs_mutex, portMAX_DELAY) == pdTRUE) {
                    auto it = subscribers.find(item.name);
                    if (it != subscribers.end()) {
                        callbacks = it->second;
                    }
                    xSemaphoreGive(subs_mutex);
                }
                for (const auto& [handle, cb] : callbacks) {
                    if (cb) cb(item.name, item.data);
                }
            }
        }
    }
}

esp_err_t EventBus::init(size_t queue_size, uint32_t task_stack_size) {
    if (!subs_mutex) subs_mutex = xSemaphoreCreateMutex();
    if (!event_queue) event_queue = xQueueCreate(queue_size, sizeof(EventQueueItem));
    if (!event_task_handle) {
        xTaskCreate(event_handler_task, "event_bus_task", task_stack_size, nullptr, 5, &event_task_handle);
    }
    ESP_LOGI(TAG, "EventBus ініціалізовано");
    return ESP_OK;
}

esp_err_t EventBus::publish(const std::string& event_name, void* event_data) {
    if (!event_queue) return ESP_FAIL;
    EventQueueItem item{event_name, event_data};
    if (xQueueSend(event_queue, &item, 0) != pdTRUE) {
        ESP_LOGW(TAG, "Черга подій переповнена");
        return ESP_ERR_TIMEOUT;
    }
    return ESP_OK;
}

EventSubscriptionHandle EventBus::subscribe(const std::string& event_name, EventCallback callback) {
    EventSubscriptionHandle handle = next_handle++;
    if (xSemaphoreTake(subs_mutex, portMAX_DELAY) == pdTRUE) {
        subscribers[event_name].emplace_back(handle, callback);
        handle_to_key[handle] = event_name;
        xSemaphoreGive(subs_mutex);
    }
    return handle;
}

void EventBus::unsubscribe(EventSubscriptionHandle handle) {
    if (xSemaphoreTake(subs_mutex, portMAX_DELAY) == pdTRUE) {
        auto it = handle_to_key.find(handle);
        if (it != handle_to_key.end()) {
            auto& vec = subscribers[it->second];
            vec.erase(std::remove_if(vec.begin(), vec.end(), [handle](const auto& p) { return p.first == handle; }), vec.end());
            handle_to_key.erase(it);
        }
        xSemaphoreGive(subs_mutex);
    }
} 