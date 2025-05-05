# main/CMakeLists.txt

idf_component_register(SRCS "main.cpp"          # Ключове слово SRCS, потім назва файлу в лапках
                      INCLUDE_DIRS "."          # Директорія для include файлів
                      REQUIRES core             # Список залежностей компонента main
                              # Додайте інші залежності сюди, якщо потрібно
                     )