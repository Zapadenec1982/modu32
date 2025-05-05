# CMakeLists.txt (Проєкт ModuChill - КОРІНЬ ПРОЄКТУ)

# Мінімальна версія CMake (стандарт для ESP-IDF v5+)
cmake_minimum_required(VERSION 3.16)

# Включаємо стандартні функції проєкту ESP-IDF
# Переконайтесь, що змінна середовища IDF_PATH вказує на ваш каталог ESP-IDF
# (Зазвичай це налаштовується автоматично при активації середовища ESP-IDF: . $IDF_PATH/export.sh або export.bat)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# Назва вашого проєкту
project(moduchill)

# --- Шляхи до компонентів ---
# Вказуємо ESP-IDF, де шукати компоненти
# Стандартні папки 'main' та 'components' зазвичай додаються автоматично,
# але для явності можна вказати.
set(EXTRA_COMPONENT_DIRS
    ${CMAKE_CURRENT_LIST_DIR}/main
    ${CMAKE_CURRENT_LIST_DIR}/components
    ${CMAKE_CURRENT_LIST_DIR}/config
    ${CMAKE_CURRENT_LIST_DIR}/modules  
)

# --- Вбудовування файлу дефолтної конфігурації ---
# Файл конфігурації знаходиться в 'config/default_config.json' відносно кореня проєкту
set(DEFAULT_CONFIG_FILE "${CMAKE_CURRENT_LIST_DIR}/config/default_config.json")
if(EXISTS "${DEFAULT_CONFIG_FILE}")
    # Отримуємо змінну, що представляє бібліотеку компонента 'main'
    idf_component_get_property(main_comp main COMPONENT_LIB)
    # Вбудовуємо файл як бінарні дані
    target_add_binary_data(${main_comp} "${DEFAULT_CONFIG_FILE}" BINARY)
    message(STATUS "Вбудовування default_config.json")
else()
    message(WARNING "Файл ${DEFAULT_CONFIG_FILE} не знайдено! Прошивка може не працювати коректно.")
endif()

# --- Генерація образу LittleFS для Веб-інтерфейсу ---
# ВАЖЛИВО: Переконайтесь, що мітка розділу та розмір відповідають вашому partitions.csv!

set(WEBUI_SRCDIR ${CMAKE_CURRENT_LIST_DIR}/components/web_interface/html) # Шлях до вихідних файлів UI
set(WEBUI_BINDIR ${CMAKE_BINARY_DIR}/littlefs_img)        # Тимчасова директорія для образу
set(WEBUI_BIN ${WEBUI_BINDIR}/littlefs.bin)              # Шлях до готового образу ФС
set(WEBUI_PARTITION_LABEL "storage")                     # Мітка розділу з partitions.csv - ЗМІНІТЬ ЗА ПОТРЕБИ!
set(LITTLEFS_MKIMAGE_TOOL "mk_littlefs")                 # Інструмент для створення образу (або mkspiffs)

# ВАЖЛИВО: Розмір розділу в байтах. Має ТОЧНО відповідати partitions.csv!
# Замініть це значення на реальний розмір вашого розділу "storage".
set(LITTLEFS_PARTITION_SIZE 1048576) # Приклад: 1MB = 1 * 1024 * 1024

set(LITTLEFS_BLOCK_SIZE 4096)      # Розмір блоку флеш-пам'яті (зазвичай 4096)
set(LITTLEFS_PAGE_SIZE 256)        # Розмір сторінки флеш-пам'яті (зазвичай 256)

# Перевіряємо, чи існує директорія з UI файлами
if(EXISTS ${WEBUI_SRCDIR})
    message(STATUS "Налаштування генерації образу LittleFS для WebUI")

    # Створюємо тимчасову директорію
    file(MAKE_DIRECTORY ${WEBUI_BINDIR})

    # Отримуємо властивості компонента main для залежності
    # Якщо у вас є окремий компонент для UI файлів, краще залежати від нього
    idf_component_get_property(main_comp main COMPONENT_LIB)

    # Додаємо команду для генерації образу
    add_custom_command(
        OUTPUT ${WEBUI_BIN}
        COMMAND ${LITTLEFS_MKIMAGE_TOOL} ARGS -c ${WEBUI_SRCDIR} -b ${LITTLEFS_BLOCK_SIZE} -p ${LITTLEFS_PAGE_SIZE} -s ${LITTLEFS_PARTITION_SIZE} ${WEBUI_BIN}
        DEPENDS ${main_comp} # Залежність від компонента main (або іншого)
        VERBATIM
        COMMENT "Генерація образу LittleFS (${WEBUI_BIN}) з ${WEBUI_SRCDIR}"
    )

    # Додаємо ціль, яка залежить від створеного образу
    add_custom_target(littlefs_image ALL DEPENDS ${WEBUI_BIN})

    # Додаємо створений образ до списку для прошивки у вказаний розділ
    #idf_build_add_part(${WEBUI_PARTITION_LABEL} ${WEBUI_BIN})

    message(STATUS "Образ LittleFS '${WEBUI_BIN}' буде прошито у розділ '${WEBUI_PARTITION_LABEL}'")

else()
    # Попередження, яке ви бачили раніше
    message(WARNING "Директорія для WebUI '${WEBUI_SRCDIR}' не знайдена. Образ LittleFS не буде створено.")
endif()

# --- Кінець налаштування LittleFS ---