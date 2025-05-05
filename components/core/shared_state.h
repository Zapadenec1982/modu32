#ifndef CORE_SHARED_STATE_H
#define CORE_SHARED_STATE_H

#include <map>
#include <string>
#include <vector>
#include <functional>
#include <variant>
#include <cstdint>
#include <atomic> // Для генерації хендлів
#include <utility> // Для std::pair
#include <algorithm> // Для std::remove_if
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_log.h" // Для логування

// Типи даних
using ValueType = std::variant<int, float, bool, std::string>;
using StateCallback = std::function<void(const ValueType&)>;
using SubscriptionHandle = uint32_t;

class SharedState {
public:
    static void init();

    // --- Шаблонний метод Set ---
    template <typename T>
    static void set(const std::string& key, T value) {
        // Створюємо ValueType з переданого значення
        // Потребує перевірки, чи тип T взагалі підтримується у ValueType
        ValueType new_value = value;

        std::vector<std::pair<SubscriptionHandle, StateCallback>> callbacks_to_call;

        if (xSemaphoreTake(get_mutex(), portMAX_DELAY) == pdTRUE) {
            // Оновлюємо або додаємо значення
            state_map[key] = new_value;

            // Отримуємо список підписників для цього ключа (якщо є)
            auto it = subscribers.find(key);
            if (it != subscribers.end()) {
                // Копіюємо список callback'ів, щоб викликати їх *після* звільнення м'ютекса
                callbacks_to_call = it->second;
            }
            xSemaphoreGive(get_mutex());
        } else {
             ESP_LOGE(TAG, "Не вдалося захопити м'ютекс для set(%s)", key.c_str());
             return;
        }

        // Викликаємо callback'и поза м'ютексом
        // ESP_LOGD(TAG, "Виклик %d callback'ів для ключа %s", callbacks_to_call.size(), key.c_str());
        for (const auto& [handle, cb] : callbacks_to_call) {
            if (cb) {
                // TODO: Розглянути можливість виклику callback'ів в окремій задачі,
                // щоб уникнути блокування потоку, що викликав set,
                // особливо якщо callback'и можуть бути тривалими.
                // Для початку - прямий виклик.
                cb(new_value);
            }
        }
    }

    // --- Шаблонний метод Get ---
    template <typename T>
    static T get(const std::string& key, T default_value) {
        if (xSemaphoreTake(get_mutex(), portMAX_DELAY) != pdTRUE) {
             ESP_LOGE(TAG, "Не вдалося захопити м'ютекс для get(%s)", key.c_str());
            return default_value;
        }

        T result = default_value;
        auto it = state_map.find(key);
        if (it != state_map.end()) {
            // Намагаємось отримати значення потрібного типу з варіанту
            try {
                 if constexpr (std::is_convertible_v<T, ValueType> || std::is_same_v<T, int> || std::is_same_v<T, float> || std::is_same_v<T, bool> || std::is_same_v<T, std::string>) {
                    result = std::get<T>(it->second);
                 } else {
                     // Тип не підтримується варіантом напряму
                     // Можна додати обробку або логування
                 }

            } catch (const std::bad_variant_access& ex) {
                // Тип у варіанті не збігається з запитуваним типом T
                ESP_LOGW(TAG, "Невідповідність типу для ключа '%s'. Запитуваний тип не відповідає збереженому.", key.c_str());
            }
        }

        xSemaphoreGive(get_mutex());
        return result;
    }

    // Методи підписки/відписки
    static SubscriptionHandle subscribe(const std::string& key, StateCallback callback);
    static void unsubscribe(SubscriptionHandle handle);

private:
    // Приватні статичні члени
    static std::map<std::string, ValueType> state_map;
    // Ключ -> Вектор пар {Хендл, Callback}
    static std::map<std::string, std::vector<std::pair<SubscriptionHandle, StateCallback>>> subscribers;
    // Хендл -> Ключ (для швидкої відписки)
    static std::map<SubscriptionHandle, std::string> handle_to_key;
    // М'ютекс для захисту доступу
    static SemaphoreHandle_t state_mutex_handle;
    // Лічильник для генерації унікальних хендлів
    static std::atomic<uint32_t> next_handle;
    static const char* TAG;

    // Доступ до м'ютекса
    static SemaphoreHandle_t get_mutex() { return state_mutex_handle; }

    // Забороняємо створення екземплярів
    SharedState() = delete;
    ~SharedState() = delete;
    SharedState(const SharedState&) = delete;
    SharedState& operator=(const SharedState&) = delete;
};

#endif // CORE_SHARED_STATE_H