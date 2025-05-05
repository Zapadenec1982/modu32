#include "shared_state.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

// Ініціалізація статичних членів класу
std::map<std::string, ValueType> SharedState::state_map;
std::map<std::string, std::vector<std::pair<SubscriptionHandle, StateCallback>>> SharedState::subscribers;
std::map<SubscriptionHandle, std::string> SharedState::handle_to_key;
SemaphoreHandle_t SharedState::state_mutex_handle = nullptr;
std::atomic<uint32_t> SharedState::next_handle{1};
const char* SharedState::TAG = "SharedState";

void SharedState::init() {
    ESP_LOGI(TAG, "Ініціалізація SharedState...");
    
    // Створюємо м'ютекс, якщо він ще не створений
    if (!state_mutex_handle) {
        state_mutex_handle = xSemaphoreCreateMutex();
        if (!state_mutex_handle) {
            ESP_LOGE(TAG, "Не вдалося створити м'ютекс!");
            return;
        }
    }
    
    // Очищаємо всі контейнери
    if (xSemaphoreTake(state_mutex_handle, portMAX_DELAY) == pdTRUE) {
        state_map.clear();
        subscribers.clear();
        handle_to_key.clear();
        xSemaphoreGive(state_mutex_handle);
    } else {
        ESP_LOGE(TAG, "Не вдалося захопити м'ютекс для ініціалізації!");
    }
    
    // Скидаємо лічильник хендлів
    next_handle.store(1);
    
    ESP_LOGI(TAG, "SharedState ініціалізовано");
}

SubscriptionHandle SharedState::subscribe(const std::string& key, StateCallback callback) {
    if (!callback) {
        ESP_LOGW(TAG, "Спроба підписатись з порожнім callback на ключ '%s'", key.c_str());
        return 0; // Невалідний хендл
    }
    
    SubscriptionHandle handle = next_handle.fetch_add(1);
    
    if (xSemaphoreTake(state_mutex_handle, portMAX_DELAY) == pdTRUE) {
        // Додаємо підписку
        subscribers[key].push_back({handle, callback});
        handle_to_key[handle] = key;
        xSemaphoreGive(state_mutex_handle);
        
        ESP_LOGD(TAG, "Додано підписку %u на ключ '%s'", handle, key.c_str());
        
        // Якщо значення вже існує, викликаємо callback із поточним значенням
        auto it = state_map.find(key);
        if (it != state_map.end()) {
            callback(it->second);
        }
    } else {
        ESP_LOGE(TAG, "Не вдалося захопити м'ютекс для підписки на '%s'", key.c_str());
        return 0; // Невалідний хендл
    }
    
    return handle;
}

void SharedState::unsubscribe(SubscriptionHandle handle) {
    if (handle == 0) {
        ESP_LOGW(TAG, "Спроба відписатись з невалідним хендлом 0");
        return;
    }
    
    if (xSemaphoreTake(state_mutex_handle, portMAX_DELAY) == pdTRUE) {
        // Знаходимо ключ для цього хендла
        auto it_handle = handle_to_key.find(handle);
        if (it_handle != handle_to_key.end()) {
            std::string key = it_handle->second;
            
            // Видаляємо підписку з вектора
            auto it_subs = subscribers.find(key);
            if (it_subs != subscribers.end()) {
                auto& subs_vec = it_subs->second;
                subs_vec.erase(
                    std::remove_if(subs_vec.begin(), subs_vec.end(), 
                                 [handle](const auto& pair) { return pair.first == handle; }),
                    subs_vec.end()
                );
                
                // Якщо вектор став порожнім, видаляємо запис
                if (subs_vec.empty()) {
                    subscribers.erase(it_subs);
                }
            }
            
            // Видаляємо запис із мапування хендл->ключ
            handle_to_key.erase(it_handle);
            ESP_LOGD(TAG, "Видалено підписку %u на ключ '%s'", handle, key.c_str());
        } else {
            ESP_LOGW(TAG, "Спроба відписатись з невідомим хендлом %u", handle);
        }
        
        xSemaphoreGive(state_mutex_handle);
    } else {
        ESP_LOGE(TAG, "Не вдалося захопити м'ютекс для відписки з хендлом %u", handle);
    }
}