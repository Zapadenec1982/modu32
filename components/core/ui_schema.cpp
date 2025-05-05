#include "shared_state.h"
#include "esp_log.h"

// Визначення статичних членів класу
const char* SharedState::TAG = "SharedState";
std::map<std::string, ValueType> SharedState::state_map;
std::map<std::string, std::vector<std::pair<SubscriptionHandle, StateCallback>>> SharedState::subscribers;
std::map<SubscriptionHandle, std::string> SharedState::handle_to_key;
SemaphoreHandle_t SharedState::state_mutex_handle = nullptr;
std::atomic<uint32_t> SharedState::next_handle{1}; // Починаємо з 1


// --- Реалізація нешаблонних статичних методів ---

void SharedState::init() {
    if (state_mutex_handle == nullptr) {
        state_mutex_handle = xSemaphoreCreateMutex();
        if (state_mutex_handle == nullptr) {
            ESP_LOGE(TAG, "Не вдалося створити м'ютекс!");
            // Критична помилка, можливо варто зупинити виконання?
            return;
        }
    }

    // Очистка при ініціалізації (якщо потрібно)
    if (xSemaphoreTake(state_mutex_handle, portMAX_DELAY) == pdTRUE) {
        ESP_LOGI(TAG, "Ініціалізація SharedState...");
        state_map.clear();
        subscribers.clear();
        handle_to_key.clear();
        next_handle.store(1); // Скидаємо лічильник хендлів
        xSemaphoreGive(state_mutex_handle);
    } else {
         ESP_LOGE(TAG, "Не вдалося захопити м'ютекс для init!");
    }
}

SubscriptionHandle SharedState::subscribe(const std::string& key, StateCallback callback) {
    if (!callback) {
         ESP_LOGW(TAG, "Спроба підписки на '%s' з нульовим callback", key.c_str());
         return 0; // Невалідний хендл
    }
     if (state_mutex_handle == nullptr) {
         ESP_LOGE(TAG, "М'ютекс не ініціалізовано для subscribe!");
         return 0;
     }

    SubscriptionHandle handle = next_handle.fetch_add(1); // Атомарно отримуємо наступний хендл

    if (xSemaphoreTake(state_mutex_handle, portMAX_DELAY) == pdTRUE) {
        ESP_LOGD(TAG, "Підписка на ключ '%s' з хендлом %lu", key.c_str(), handle);
        subscribers[key].emplace_back(handle, callback);
        handle_to_key[handle] = key; // Зберігаємо зворотнє посилання
        xSemaphoreGive(state_mutex_handle);
    } else {
         ESP_LOGE(TAG, "Не вдалося захопити м'ютекс для subscribe(%s)", key.c_str());
         // Повертаємо 0, якщо не вдалося підписатись? Або згенерували хендл даремно?
         // Можливо, краще генерувати хендл після захоплення м'ютекса.
         return 0; // Повертаємо невалідний хендл
    }

    return handle;
}

void SharedState::unsubscribe(SubscriptionHandle handle) {
     if (handle == 0) return; // Ігноруємо невалідний хендл
     if (state_mutex_handle == nullptr) {
          ESP_LOGE(TAG, "М'ютекс не ініціалізовано для unsubscribe!");
         return;
     }

    if (xSemaphoreTake(state_mutex_handle, portMAX_DELAY) == pdTRUE) {
        ESP_LOGD(TAG, "Відписка хендла %lu", handle);
        auto key_it = handle_to_key.find(handle);
        if (key_it != handle_to_key.end()) {
            const std::string& key = key_it->second;
            auto subs_it = subscribers.find(key);
            if (subs_it != subscribers.end()) {
                auto& vec = subs_it->second;
                // Видаляємо елемент з вектора за хендлом
                auto original_size = vec.size();
                vec.erase(std::remove_if(vec.begin(), vec.end(),
                                         [handle](const auto& p) { return p.first == handle; }),
                          vec.end());

                // Якщо після видалення вектор для цього ключа став порожнім, видаляємо сам ключ з subscribers
                if (vec.empty()) {
                     ESP_LOGD(TAG, "Видалення запису підписників для ключа '%s' (був порожнім)", key.c_str());
                    subscribers.erase(subs_it);
                } else if (vec.size() == original_size) {
                     ESP_LOGW(TAG, "Підписка з хендлом %lu не знайдена у векторі для ключа '%s'", handle, key.c_str());
                }
            } else {
                 ESP_LOGW(TAG, "Не знайдено вектор підписників для ключа '%s' при відписці хендла %lu", key.c_str(), handle);
            }
            // Видаляємо запис з карти хендлів
            handle_to_key.erase(key_it);
        } else {
             ESP_LOGW(TAG, "Не знайдено ключ для хендла %lu при відписці", handle);
        }
        xSemaphoreGive(state_mutex_handle);
    } else {
         ESP_LOGE(TAG, "Не вдалося захопити м'ютекс для unsubscribe(%lu)", handle);
    }
}

// Реалізація шаблонних методів get/set тепер знаходиться у shared_state.h