# Внешние программы LexxOS (*.lexx)

Документ описывает первый экспериментальный слой запуска внешних программ LexxOS. Цель слоя — дать одинаковый
переносимый формат исполняемого файла для host/sim/pico/stm32/esp32 и начать с безопасного текстового ABI, который можно
развивать без изменения имени файла и базовой модели запуска.

## Текущее состояние

На этапе 0.0.7 поддерживается запуск файла `*.lexx` из VFS/SD через команду:

```text
lexx-run [--trace] [--max-steps <count>] <file.lexx> [args...]
```

Также shell умеет прямой запуск первого токена с суффиксом `.lexx`:

```text
lexx:/> /mnt/sd/ext/hello.lexx alpha beta
lexx:/> hello.lexx
```

Внутри shell такой запуск превращается в `lexx-run <file.lexx> ...`, но аргументы guest-программы остаются такими, как
если бы пользователь запустил сам `.lexx` файл: `argv[0]` равен указанному пути, `argv[1...]` - пользовательским
аргументам.

Команда запускается как foreground-программа через `ProgramSupervisor`, поэтому она участвует в существующей модели
`run`, `stop`, `jobs`, фокуса программ и кооперативной отмены. Для долгих циклов guest-программа должна периодически
вызывать `lexx::yield()` или опрашивать ввод, чтобы не занимать систему дольше необходимого.

## Быстрая проверка демо

В репозитории добавлен минимальный guest SDK и демо-программа:

```text
sdk/lexx/lexx.h
examples/lexx/lexx.ld
examples/lexx/hello/hello.cpp
examples/lexx/hello/Makefile
examples/lexx/cat/cat.cpp
examples/lexx/cat/Makefile
tools/lexx_pack.py
```

Для сборки нужен bare-metal RISC-V GCC toolchain с `g++` и `objcopy`.

На macOS через Homebrew самый простой вариант:

```sh
brew install riscv64-elf-gcc
```

После установки проверьте, что компилятор доступен:

```sh
riscv64-elf-g++ --version
riscv64-elf-objcopy --version
```

Makefile автоматически ищет один из префиксов: `riscv64-unknown-elf`, `riscv64-elf`, `riscv32-unknown-elf`,
`riscv-none-elf`. Чтобы увидеть, какой toolchain выбран:

```sh
cd examples/lexx/hello
make toolchain-info
```

Обычная сборка:

```sh
cd examples/lexx/hello
make
```

Если нужно указать toolchain явно:

```sh
make CROSS=riscv64-elf
make CROSS=riscv32-unknown-elf
make CROSS=riscv-none-elf
```

Результат сборки:

```text
examples/lexx/hello/build/hello.elf
examples/lexx/hello/build/hello.bin
examples/lexx/hello/build/hello.lexx
```

Скопируйте `hello.lexx` на SD-карту, например в корень FAT-раздела, затем в LexxOS выполните:

```text
lexx:/> lexx-run /mnt/sd/hello.lexx alpha beta
lexx:/> /mnt/sd/hello.lexx alpha beta
```

Для запуска через supervisor-команду с явным сообщением о PID можно использовать:

```text
lexx:/> run lexx-run /mnt/sd/hello.lexx
lexx:/> jobs
lexx:/> stop <id>
```

Демо печатает приветствие, список `argv`, одну строку ожидания ввода, выводит события клавиатуры и завершается по `q`.
Ctrl+C воспринимается как сигнал отмены.

## Guest SDK

Пользовательский код должен обращаться к системным возможностям через namespace `lexx`. Низкоуровневые `ecall` спрятаны
в `sdk/lexx/lexx.h`.

Минимальные функции 0.0.7:

```cpp
lexx::write("text\n");
lexx::write(data, size);
lexx::write_error("error\n");
lexx::read(fd, data, size);
lexx::open_read(path);
lexx::close(fd);
lexx::poll_key(event);
lexx::argc();
lexx::argv();
lexx::argument(index);
lexx::yield();
lexx::exit(code);
```

Пример точки входа:

```cpp
#include <lexx/lexx.h>

int app_main() {
    lexx::write("hello from .lexx\n");
    return 0;
}

extern "C" [[noreturn]] void _start() {
    lexx::exit(app_main());
}
```

`lexx::poll_key` принимает структуру `lexx::KeyEvent` размером 12 байт. Если событие есть, функция возвращает `true`;
если событий нет или программа не находится в фокусе, возвращает `false`.

```cpp
lexx::KeyEvent event;
if (lexx::poll_key(event)) {
    if (event.character == 'q') {
        lexx::exit(0);
    }
}
```

## Сборка своего приложения

Минимальная последовательность сборки для Homebrew toolchain такая:

```sh
riscv64-elf-g++ \
  -std=c++20 \
  -march=rv32i \
  -mabi=ilp32 \
  -ffreestanding \
  -fno-builtin \
  -fno-exceptions \
  -fno-rtti \
  -fno-use-cxa-atexit \
  -fno-threadsafe-statics \
  -ffunction-sections \
  -fdata-sections \
  -Os \
  -Wall \
  -Wextra \
  -Isdk \
  -nostdlib \
  -static \
  -Wl,-T,examples/lexx/lexx.ld \
  -Wl,--gc-sections \
  -Wl,-Map,app.map \
  -o app.elf app.cpp

riscv64-elf-objcopy -O binary app.elf app.bin
python3 tools/lexx_pack.py app.bin app.lexx --memory 65536 --stack 4096 --cap console,keyboard
```

Для обычной работы удобнее скопировать `examples/lexx/hello/Makefile` и заменить `hello.cpp` на свой файл.

Важно: текущий профиль — чистый `RV32I`. Если использовать деление, остаток от деления, 64-битную арифметику или другие
операции, компилятор может потребовать `libgcc`. На первом этапе лучше писать демо-код без таких операций либо отдельно
продумать подключение минимального runtime.

## Метаданные приложения

Программы теперь могут содержать манифест приложения с версией, автором, лицензией, контактами и временем упаковки UTC. Исходный манифест хранится в JSON, а получить сведения из собранного файла можно командой `lexx-info`, не запуская RV32I код. Подробнее: [манифесты внешних программ](external-programs-manifest.md).

## Формат файла

Файл содержит фиксированный заголовок и тело программы, а при сборке с `--manifest` — необязательный JSON-манифест и 12-байтный footer после тела. Все поля заголовка little-endian. Формат заголовка и ABI остаются версии 1, а структура trailer описана в [документации манифестов](external-programs-manifest.md).

| Offset | Size | Name                 | Description                                   |
|-------:|-----:|----------------------|-----------------------------------------------|
|      0 |    4 | magic                | `LEXX`, little-endian word `0x5858454c`       |
|      4 |    2 | formatVersion        | версия формата, сейчас `1`                    |
|      6 |    2 | headerSize           | размер заголовка, сейчас строго `64`          |
|      8 |    2 | machine              | `1` = RV32I                                   |
|     10 |    2 | abiVersion           | версия ABI, сейчас `1`                        |
|     12 |    4 | flags                | зарезервировано, сейчас `0`                   |
|     16 |    4 | requiredCapabilities | битовая маска требуемых прав                  |
|     20 |    4 | entryOffset          | начальный PC внутри guest-памяти              |
|     24 |    4 | imageOffset          | offset тела программы в файле                 |
|     28 |    4 | imageSize            | размер загружаемого образа                    |
|     32 |    4 | memorySize           | размер guest-памяти                           |
|     36 |    4 | stackSize            | заявленный размер стека                       |
|     40 |    4 | bssSize              | зарезервировано для будущего loader/toolchain |
|     44 |   20 | reserved             | нули, зарезервировано                         |

На текущем этапе образ копируется в guest-память с offset `0`, затем оставшаяся память заполняется нулями. Регистр `sp`
(`x2`) устанавливается в конец guest-памяти, выровненный вниз по 4 байтам. `entryOffset` должен быть выровнен по 4
байтам и указывать внутрь guest-памяти.

## Архитектура guest-кода

Базовая ISA: RV32I.

Поддерживаются базовые инструкции RV32I: `LUI`, `AUIPC`, `JAL`, `JALR`, conditional branches, `LB/LH/LW/LBU/LHU`,
`SB/SH/SW`, `OP-IMM`, `OP`, `FENCE` как no-op, `ECALL`.

Пока не поддерживаются расширения `C`, `M`, атомики, floating point и прямой доступ к MMIO. Любой доступ guest-кода за
пределы выделенной guest-памяти приводит к fault-статусу. Доступ к ресурсам платы должен идти только через системные
вызовы ABI.

## Capabilities

Маска `requiredCapabilities` задаёт ресурсы, которые требует программа.

| Bit | Name          | Status                                        |
|----:|---------------|-----------------------------------------------|
|   0 | console text  | поддерживается                                |
|   1 | keyboard      | поддерживается                                |
|   2 | storage read  | поддерживается для read-only VFS/SD           |
|   3 | storage write | зарезервировано, сейчас отклоняется loader-ом |
|   4 | network       | зарезервировано, сейчас отклоняется loader-ом |
|   5 | devices       | зарезервировано, сейчас отклоняется loader-ом |

Такое поведение сделано намеренно: уже сейчас файл может объявлять будущие требования, но рантайм не даст программе
получить доступ к неподдержанному классу ресурсов. Для файлового чтения capability работает как разрешение: `open_read`
возвращает ошибку, если программа не объявила `storage-read`.

## ABI системных вызовов

Системный вызов выполняется инструкцией `ecall`. Номер вызова передаётся в `a7`. Аргументы — в `a0`, `a1`, `a2`.
Возврат — в `a0`.

| a7 | Name           | Arguments                                         | Return                                                                                  |
|---:|----------------|---------------------------------------------------|-----------------------------------------------------------------------------------------|
|  0 | exit           | `a0 = exitCode`                                   | завершает программу                                                                     |
|  1 | write          | `a0 = fd`, `a1 = ptr`, `a2 = len`                 | bytes written или `0xffffffff`                                                          |
|  2 | read-char      | нет                                               | byte, `0xffffffff` если нет данных/нет фокуса, `0xfffffffe` при Ctrl+C                  |
|  3 | read           | `a0 = fd`, `a1 = ptr`, `a2 = len`                 | bytes read или `0xffffffff`                                                             |
|  4 | poll-key-event | `a0 = ptr`                                        | `1` event written, `0` no event/no focus, `0xffffffff` invalid ptr, `0xfffffffe` Ctrl+C |
|  5 | yield          | нет                                               | `0`                                                                                     |
|  6 | argc           | нет                                               | количество аргументов                                                                   |
|  7 | argv           | нет                                               | guest-адрес таблицы `char const* argv[]`                                                |
|  8 | open-read      | `a0 = path`                                       | fd `>= 3` или `0xffffffff`                                                              |
|  9 | close          | `a0 = fd`                                         | `0` или `0xffffffff`                                                                    |
| 10 | list-directory | `a0 = path`, `a1 = firstIndex`, `a2 = entries[8]` | `0..8` записей либо `0xffffffff`                                                        |
| 11 | stat           | `a0 = path`, `a1 = FileStat*`                     | `0` или `0xffffffff`                                                                    |
| 12 | seek           | `a0 = fd`, `a1 = offsetLow`, `a2 = offsetHigh`    | `0` или `0xffffffff`                                                                    |

`list-directory` требует `storage-read`, принимает относительный либо абсолютный путь, проверяет, что путь указывает на
каталог, и возвращает до 8 записей, начиная с `firstIndex`. `0` означает конец каталога. Для следующей страницы нужно
увеличить `firstIndex` на число полученных записей. В каждой записи есть имя (64 байта с NUL), размер (`uint32_t`
low/high) и тип: 1 — каталог, 2 — обычный файл, 3 — блочное устройство. Структура `DirectoryEntry` занимает 76 байт.
Результат `stat` занимает 12 байт (`uint32_t type`, `size_low`, `size_high`). Все числовые поля little-endian.

Постраничное перечисление не формирует атомарный снимок: если каталог изменился, начните обход заново. ABI не позволяет
записывать файлы и не предоставляет указатели на host-память.

`seek` требует `storage-read` и принимает абсолютную 64-битную позицию в двух регистрах. Разрешены только открытые для
чтения гостевые файловые дескрипторы и позиции в пределах файла. Внутри используется `vfs::FileHandle::seek()`. Файл не
изменяется. Это позволяет внешним просмотрщикам читать только текущую страницу, не загружая файл целиком.

Файловые дескрипторы: `0` = stdin, `1` = stdout, `2` = stderr, `3..6` = read-only guest-файлы, открытые через
`open-read`. Сейчас одна VM может держать до 4 таких файлов одновременно.

### Структура события клавиатуры

`poll-key-event` пишет 12 байт:

| Offset | Size | Field                          |
|-------:|-----:|--------------------------------|
|      0 |    1 | type: `1` key-down, `2` key-up |
|      1 |    1 | modifiers                      |
|      2 |    1 | ASCII character, если есть     |
|      3 |    1 | scanCode                       |
|      4 |    2 | keyCode                        |
|      6 |    2 | reserved                       |
|      8 |    4 | timestampMs                    |

Ctrl+C обрабатывается как сигнал остановки. Если foreground-программа получает Ctrl+C через текстовый ввод или очередь
событий клавиатуры, рантайм запрашивает cancellation token и завершает guest-программу со статусом `cancelled`.

## Ограничения первого этапа

- Это интерпретатор RV32I, не JIT и не нативная загрузка кода.
- Максимальная guest-память по умолчанию — 64 KiB.
- По умолчанию лимит инструкций выключен; `--max-steps <count>` включает конечный лимит, а `--max-steps 0` явно
  оставляет запуск без лимита.
- PSRAM пока используется как предпочтительная квота через существующий `ProgramMemoryArena`, но фактический allocator
  внешней памяти ещё нужно развить отдельно.
- Доступ к VFS/SD есть только на чтение. `seek`, запись/append, Wi-Fi/Ethernet, I2C/SPI/GPIO и прямой доступ к
  устройствам остаются будущими ABI.

## Следующий шаг

Следующий технический шаг — автоматизированная проверка файлового менеджера из `apps/fileman/` на host/sim и аппаратный
smoke на PicoCalc. Затем можно расширять ABI: `seek`, write/append и более строгую политику разрешённых путей.
