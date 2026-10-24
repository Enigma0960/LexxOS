# Сторонние компоненты LexxOS

[Apache-2.0](LICENSE) применяется к собственному коду LexxOS. Сторонние компоненты,
SDK, ресурсы и файлы с отдельными copyright/licensing notices сохраняют свои лицензии.
Этот перечень не заменяет файлы лицензий конкретного SDK и его зависимостей.

| Компонент | Расположение / источник | Лицензия и уведомления |
|---|---|---|
| FreeRTOS Kernel | `third_party/FreeRTOS` (git submodule) | [MIT](third_party/FreeRTOS/LICENSE.md); отдельные порты сохраняют собственные notices |
| FatFs R0.16 | `third_party/fatfs` | [Условия ChaN](third_party/fatfs/LICENSE.txt), copyright также в заголовках исходников |
| BMI270 configuration data | `src/lexxos/devices/sensors/bmi270_config.cpp` | Bosch Sensortec, [BSD-3-Clause](licenses/BMI270-BSD-3-Clause.txt); источник указан в файле |
| Pico SDK и зависимости | Внешняя установка SDK | Лицензии выбранной версии SDK, CYW43, lwIP, TinyUSB и других включённых компонентов |
| ESP-IDF и managed components | Внешний SDK; версии в `ports/espidf/locks` | Лицензии SDK и каждого компонента выбранного target |
| PySide6/Qt, sounddevice и Python-зависимости | `emulator/pyproject.toml`, `emulator/uv.lock` | Условия установленных пакетов; не перелицензируются под Apache-2.0 |

При подготовке бинарного релиза необходимо включить относящиеся к нему лицензии
и notices зависимостей. Архив firmware и комплект Python GUI могут иметь разный
состав зависимостей. [Релизный упаковщик](docs/release-packaging.md) включает notices SDK и зависимостей.
При изменении состава зависимостей проверку лицензий итогового набора нужно повторить.
