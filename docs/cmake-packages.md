# Пакеты CMake для внешних проектов

Пакеты собирают библиотеки из исходников LexxOS компилятором выбранной
платформы. CMake-цели передают проекту заголовки, определения и зависимости
для линковки. Нужен checkout LexxOS с подмодулями.

Укажите каталог `cmake` в `CMAKE_PREFIX_PATH` или задайте
`LexxSDK_DIR`, `LexxAppSDK_DIR` или `LexxCE_DIR` через `-D`.
В кросс-сборках с ограниченным поиском пакетов, например в Pico SDK,
используйте `-DLexxSDK_DIR=/path/to/lexxos/cmake`.

```sh
export CMAKE_PREFIX_PATH="/path/to/lexxos/cmake"
cmake -S . -B build
cmake --build build
```

## LexxSDK

Подключите цель `LexxSDK::LexxSDK` к своей программе или библиотеке.
Для host, Pico, STM32 и Lyra она предоставляет статическую библиотеку, для
симулятора — динамическую библиотеку и `lexxos_sim_api.h`.

```cmake
cmake_minimum_required(VERSION 3.20)
project(MySystem LANGUAGES C CXX ASM)

find_package(LexxSDK CONFIG REQUIRED)
add_executable(my_system main.cpp)
target_link_libraries(my_system PRIVATE LexxSDK::LexxSDK)
```

В своей точке входа вызовите `lexxos::app::run`. Чтобы добавить встроенную
команду в штатный shell, передайте функцию регистрации.

```cpp
#include <lexxos/app/app.h>
#include <lexxos/program/program_manager.h>

int main() {
    return lexxos::app::run([](lexxos::program::ProgramManager& programs) {
        programs.registerProgram({
            .name = "my-command",
            .help = "Run my command",
            .handler = [](auto&, auto const&, auto const&) { return 0; },
        });
    });
}
```

Платформу и профиль можно задать переменными CMake `LEXXOS_PLATFORM` и
`LEXXOS_BOARD` или одноимёнными переменными среды. Для сборок с разными
платами используйте отдельные каталоги сборки.

| Платформа | Конфигурация проекта                                                                                                                                                                                                                                           |
|-----------|----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| `host`    | Платформа по умолчанию. Профиль `host_native`.                                                                                                                                                                                                                 |
| `sim`     | Профиль `dev-sim`. Запускайте симулятор через `lexxos_sim_api.h`.                                                                                                                                                                                              |
| `pico`    | Импортируйте Pico SDK перед `project()`, вызовите `pico_sdk_init()` и найдите пакет. Профиль LexxOS определяется по `PICO_BOARD`. Явный `LEXXOS_BOARD` должен соответствовать плате Pico SDK. Примените `pico_add_extra_outputs(my_system)` для получения UF2. |
| `stm32f4` | Укажите `config/cmake/toolchains/arm-none-eabi.cmake` в `CMAKE_TOOLCHAIN_FILE`. Профиль по умолчанию `euse_m4_demo_stm32f407`.                                                                                                                                 |
| `rk3506`  | Укажите `config/cmake/toolchains/arm-none-eabi-rk3506.cmake` в `CMAKE_TOOLCHAIN_FILE`. Профиль по умолчанию `picocalc_lyra`. Точка входа приложения — `extern "C" int rk3506_shell_run()`.                                                                     |
| `espidf`  | Найдите пакет до `project()`. Если `LEXXOS_BOARD` пропущен, профиль выбирается по `IDF_TARGET`.                                                                                                                                                                |

Для STM32 и Lyra результат линковки — ELF. Его можно преобразовать в BIN
через `arm-none-eabi-objcopy -O binary`. Форматы загрузки и упаковка
прошивки Lyra описаны в руководстве платы. [PicoCalc Lyra M](boards/picocalc_lyra_m.md).

В ESP-IDF используйте свою функцию `extern "C" void app_main()` и
`idf_component_register(SRCS app_main.cpp REQUIRES lexxos)`
в компоненте приложения. Параметры разделов и настройки ESP-IDF храните
в `sdkconfig.defaults` своего проекта.

## LexxAppSDK

Для сборки гостевого пакета укажите исходники приложения и манифест.
SDK использует Python 3 и отдельный компилятор RV32I с `objcopy`.
Компилятор можно переопределить переменной `LEXXOS_APP_CXX`.

```cmake
cmake_minimum_required(VERSION 3.20)
project(MyGuest LANGUAGES NONE)

find_package(LexxAppSDK CONFIG REQUIRED)
lexxos_add_app(my_guest SOURCES main.cpp MANIFEST manifest.json)
```

Результат сборки — `build/apps/my_guest.lexx`. Память, стек, разрешения
и ресурсы берутся из манифеста. Для смешанного проекта можно указать
`C_SOURCES` и `INCLUDE_DIRECTORIES`. Цель `LexxAppSDK::Headers`
предоставляет заголовки для инструментов CMake, а `lexxos_add_app`
собирает гостевой runtime и упаковывает приложение. [API гостевых приложений](../apps/sdk/README.md).

## LexxIDF

Для внешнего проекта ESP-IDF достаточно указать `IDF_PATH` в окружении
или `-DLEXXOS_IDF_PATH=/path/to/esp-idf` в параметрах CMake. Установка
ESP-IDF и его инструментов (`install.sh` для нужного чипа) должна быть
выполнена заранее. Запуск `export.sh` вручную не требуется: пакет импортирует
окружение, выбирает Python и абсолютные пути к компиляторам поддерживаемых
LexxOS целей (`esp32`, `esp32s3`, `esp32p4`), фиксирует абсолютные пути
к Binutils (`ar`, `ranlib`, `objdump`, `ld` и другие) и передаёт их загрузчику.
При отсутствии явно заданного `IDF_PYTHON_ENV_PATH` пакет ищет установленное
окружение нужной major/minor версии ESP-IDF в `$IDF_TOOLS_PATH/python_env`
(по умолчанию `~/.espressif/python_env`). Если найдено несколько окружений,
выбирается окружение с наибольшей версией Python. Для явного выбора передайте
`-DLEXXOS_IDF_PYTHON_ENV_PATH=/path/to/idf_python_env`. Установка инструментов
и Python-зависимостей по-прежнему выполняется средствами ESP-IDF.

`LexxCE` и `LexxSDK` автоматически подключают этот механизм в режиме `espidf`.
Он также доступен отдельно: `find_package(LexxIDF CONFIG REQUIRED)` перед
`project()`. Платформу можно явно задать через `-DLEXXOS_PLATFORM=espidf`.
Профиль CLion после перехода с host на ESP-IDF необходимо пересоздать или
сбросить его кеш CMake.

## LexxCE

Для классического ESP32 укажите путь к установленному ESP-IDF и найдите пакет
перед `project()`. Пакет сам подключает ядро, адаптер ESP-IDF и выбранный
планировщик. По умолчанию используется `cooperative`. Для FreeRTOS
передайте `-DLEXXOS_CE_EXECUTION=freertos`.

```cmake
cmake_minimum_required(VERSION 3.20)

find_package(LexxCE CONFIG REQUIRED)
project(MyFirmware)
```

В `main/CMakeLists.txt` зарегистрируйте свой компонент.

```cmake
idf_component_register(SRCS app_main.cpp REQUIRES lexxce)
```

Точка входа и конфигурация выводов платы принадлежат приложению.
`LexxCE::LexxCE` доступна как CMake-цель и для других целей этого
проекта. В обычном C++-проекте подключите пакет после `project()`
и линкуйтесь с этой же целью, чтобы получить переносимое ядро и
кооперативный планировщик. Реализацию `PlatformServices` для
такой сборки предоставляет приложение. [API LexxCE](../ce/README.md).
