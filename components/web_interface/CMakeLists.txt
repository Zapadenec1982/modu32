idf_component_register(
    SRC_DIRS
        "src"
    INCLUDE_DIRS
        "include"
    REQUIRES
        mongoose
        freertos
        esp_event
        esp_http_server
        log
        esp_system
        json
        core
)

# Включаємо HTML файли як бінарні дані до компонента
set(WEB_UI_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/html")

# Додаємо HTML файли як ресурси
set(html_files
    "${WEB_UI_SRC_DIR}/index.html"
    "${WEB_UI_SRC_DIR}/style.css"
    "${WEB_UI_SRC_DIR}/script.js"
)

# Копіюємо HTML файли в build директорію для спрощення доступу
file(COPY ${html_files} DESTINATION ${CMAKE_BINARY_DIR}/html)

# Отримуємо поточний компонент
idf_component_get_property(component_lib web_interface COMPONENT_LIB)

# Додаємо всі HTML файли як бінарні ресурси
foreach(html_file ${html_files})
    get_filename_component(file_name ${html_file} NAME)
    target_add_binary_data(${component_lib} ${html_file} BINARY)
    message(STATUS "Додано HTML файл: ${file_name}")
endforeach()

# Зауваження: Для LittleFS потрібен окремий інструмент mklittlefs
# Якщо він недоступний, можна використовувати вбудовані HTML файли як заміну
# А для продакшн-версії встановити mklittlefs та розкоментувати код нижче

#[[
# --- Налаштування LittleFS образу (закоментовано) ---
set(LITTLEFS_IMAGE_DIR "${CMAKE_BINARY_DIR}/littlefs_image")
set(LITTLEFS_IMAGE "${LITTLEFS_IMAGE_DIR}/littlefs.bin")
set(LITTLEFS_PARTITION_SIZE 1048576)  # 1MB як приклад

file(MAKE_DIRECTORY ${LITTLEFS_IMAGE_DIR})

add_custom_command(
    OUTPUT ${LITTLEFS_IMAGE}
    COMMAND mklittlefs -c ${WEB_UI_SRC_DIR} -d 5 -b 4096 -p 256 -s ${LITTLEFS_PARTITION_SIZE} ${LITTLEFS_IMAGE}
    DEPENDS ${html_files}
    COMMENT "Створення образу LittleFS з HTML файлів"
)

add_custom_target(littlefs_image ALL DEPENDS ${LITTLEFS_IMAGE})
idf_build_add_part("storage" ${LITTLEFS_IMAGE})
]]
