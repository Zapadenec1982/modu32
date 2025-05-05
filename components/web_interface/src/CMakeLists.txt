idf_component_register(
    SRCS
        "web_interface.cpp"
        "server_mongoose.cpp"
        "rpc_api.cpp"
        "websocket_manager.cpp"
    INCLUDE_DIRS "../include"
    PRIV_INCLUDE_DIRS "."
    REQUIRES
        mongoose
        freertos
        esp_event
        esp_log
        json
        core
)

#  --- Генерація образу LittleFS для Веб-інтерфейсу ---
set(WEBUI_SRCDIR "${CMAKE_CURRENT_LIST_DIR}/html") #  Шлях до вихідних файлів UI
set(WEBUI_BINDIR "${CMAKE_BINARY_DIR}/littlefs_img")        #  Тимчасова директорія для образу
set(WEBUI_BIN "${WEBUI_BINDIR}/littlefs.bin")              #  Шлях до готового образу ФС
set(WEBUI_PARTITION_LABEL "storage")                     #  Мітка розділу з partitions.csv - ЗМІНІТЬ ЗА ПОТРЕБИ!
set(LITTLEFS_MKIMAGE_TOOL "mk_littlefs")                 #  Інструмент для створення образу (або mkspiffs)

#  ВАЖЛИВО: Розмір розділу в байтах. Має ТОЧНО відповідати partitions.csv!
set(LITTLEFS_PARTITION_SIZE 1048576) #  Приклад: 1MB = 1 * 1024 * 1024

set(LITTLEFS_BLOCK_SIZE 4096)      #  Розмір блоку флеш-пам'яті (зазвичай 4096)
set(LITTLEFS_PAGE_SIZE 256)        #  Розмір сторінки флеш-пам'яті (зазвичай 256)

#  Перевіряємо, чи існує директорія з UI файлами
if(EXISTS ${WEBUI_SRCDIR})
    message(STATUS "Налаштування генерації образу LittleFS для WebUI")

    #  Створюємо тимчасову директорію
    file(MAKE_DIRECTORY ${WEBUI_BINDIR})

    #  Отримуємо властивості поточного компонента для залежності
    idf_component_get_property(web_interface_lib web_interface COMPONENT_LIB)

    #  Додаємо команду для генерації образу
    add_custom_command(
        OUTPUT ${WEBUI_BIN}
        COMMAND ${LITTLEFS_MKIMAGE_TOOL} ARGS -c ${WEBUI_SRCDIR} -b ${LITTLEFS_BLOCK_SIZE} -p ${LITTLEFS_PAGE_SIZE} -s ${LITTLEFS_PARTITION_SIZE} ${WEBUI_BIN}
        DEPENDS ${web_interface_lib} #  Залежність від поточного компонента
        VERBATIM
        COMMENT "Генерація образу LittleFS (${WEBUI_BIN}) з ${WEBUI_SRCDIR}"
    )

    #  Додаємо ціль, яка залежить від створеного образу
    add_custom_target(littlefs_image ALL DEPENDS ${WEBUI_BIN})

    #  Додаємо створений образ до списку для прошивки у вказаний розділ
    idf_build_add_part "${WEBUI_PARTITION_LABEL}" "${WEBUI_BIN}"

    message(STATUS "Образ LittleFS '${WEBUI_BIN}' буде прошито у розділ '${WEBUI_PARTITION_LABEL}'")

else()
    #  Попередження, якщо директорію не знайдено
    message(WARNING "Директорія для WebUI '${WEBUI_SRCDIR}' не знайдена. Образ LittleFS не буде створено.")
endif()

#  --- Кінець налаштування LittleFS ---