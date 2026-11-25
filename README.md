# LexxOS

## От автора

LexxOS — мой проект для свободного времени. Я занимаюсь им ради удовольствия,
без конкретной конечной цели: мне интересно придумывать, пробовать и смотреть, что получится.
Структуру и логику проекта придумываю я, а писать код мне помогает ИИ, выступая
соавтором реализации.

Я рад предложениям по развитию и расширению проекта, но не обещаю реализовать
каждую идею или назвать сроки. Работаю над тем, что мне интересно, когда есть
время и желание.

— Igor Supryadkin

## Сообщество и поддержка

**Сообщество:** [Telegram-канал](https://t.me/lexx_os_channel) · [Telegram-группа](https://t.me/lexx_os) · [GitHub](https://github.com/Enigma0960/LexxOS)

**Поддержать проект:** [Boosty](https://boosty.to/enigma0960) · [Patreon](https://www.patreon.com/cw/enigma0960) · [DonationAlerts](https://www.donationalerts.com/r/enigma0960)

## О системе

LexxOS — экспериментальная встраиваемая операционная система для микроконтроллерных плат.
Проект включает shell, программы и runtime, VFS с tmpfs/FatFs, драйверы устройств,
UART/LCD-консоль и host/simulator для разработки без платы.

Готовится выпуск **0.1.1**. Предыдущая версия доступна
как [публичный prerelease 0.1.0](https://github.com/Enigma0960/LexxOS/releases/tag/v0.1.0).
Задачи, известные недоработки и критерии выпуска собраны в [roadmap.md](roadmap.md).
Наличие профиля платы в исходниках не означает, что текущий код прошёл аппаратную проверку.

## Быстрый запуск на host

Нужны CMake 3.25+, C/C++20 toolchain, Ninja и Python 3. По умолчанию также собираются
[гостевые приложения](apps/README.md): для них нужен RISC-V GCC/objcopy либо Clang/LLD/llvm-objcopy.
Для сборки только ОС добавьте `-DLEXXOS_APPS=OFF` при конфигурации.
Из корня репозитория:

```bash
cmake --preset minimal-host_native
cmake --build --preset minimal-host_native
./build/minimal-host_native/src/lexxos
```

Попробуйте `help`, `version`, `board`, `tasks`, `ls /`, `ls /proc`, `cat /sys/board`, `exit`.
Host предоставляет программные заглушки для оборудования. Для виртуальных SD, display и input
используйте [Python simulator](tools/emulator/README.md).

## Платформы и возможности

В исходниках есть реализации для компьютера, Raspberry Pi Pico (RP2040/RP2350), STM32F407, ESP-IDF
и Rockchip RK3506 (PicoCalc / Luckfox Lyra M).
Профили включают семейство Pico, ClockworkPi PicoCalc, M5Stack и JC-ESP32P4-M3-DEV.
Pinout, доступные устройства и ограничения описаны в [профилях плат](docs/boards.md).

- Shell и program manager: аргументы, перенаправление потоков, кооперативная отмена,
  экспериментальные `run`/`jobs`/`stop`.
- VFS: `/dev`, `/proc`, `/sys`, `/mnt`, `/tmp`, mount points, tmpfs и FatFs.
- SD: SPI и платформенные SDIO backend; RP2040 SDIO остаётся экспериментальной заготовкой.
- Дисплеи, клавиатуры, RTC, сеть, audio и дополнительные приложения включаются по плате и feature flags.
- Game Boy и MIDI имеют отдельные ограничения по ресурсам; MIDI выключен по умолчанию,
  тяжёлые функции ограничены в PicoCalc Pico/Pico W на RP2040.

Processor family, platform backend и board profile — разные уровни конфигурации.
Переносимые сервисы, драйверы и приложения связываются через composition layer.
Подробности: [архитектура](docs/architecture.md), [runtime программ](docs/program-runtime.md).

`dev_pico`, `dev_pico_sdio` и `dev_pico2w` — экспериментальные платы для разработки и проверки драйверов. Их настройки,
состав периферии и назначение IO (GPIO/pinout) могут меняться со временем. Перед подключением оборудования сверяйте
wiring с документацией и board descriptor используемой версии; совместимость распиновки между версиями не гарантируется.

## Сборка прошивок

Список доступных конфигураций:

```bash
cmake --list-presets
cmake --build --list-presets
tools/lexxos-build check all
```

Пример для Pico при установленном Pico SDK и toolchain:

```bash
git submodule update --init --recursive third_party/FreeRTOS
cmake --preset minimal-dev_pico
cmake --build --preset minimal-dev_pico
```

Артефакты Pico находятся в `build/minimal-dev_pico/src/`: `lexxos.elf`, `lexxos.bin`, `lexxos.hex`, `lexxos.uf2`.
Основная матрица плат использует `minimal-<board>` и `full-<board>`;
для Lyra предусмотрен отдельный релизный профиль `picocalc-lyra-release`.
Матрицу можно собрать через `tools/lexxos-build all-minimal all-full --keep-going`.
GUI сам собирает библиотеку и запускает ОС: `python tools/emulator/lexxos_emu/gui.py --profile full`.

Настройка SDK, ESP-IDF environment, feature flags и другие цели описаны в [инструкции сборки](docs/build.md).
Для сборки внешних проектов доступны [пакеты CMake](docs/cmake-packages.md).

## Публичный набор 0.1.0

Minimal/full для Cardputer ADV, PicoCalc с Pico, Pico W, Pico 2, Pico 2 W и
JC-ESP32P4-M3-DEV; отдельный apps ZIP. Все 12 профилей собраны, аппаратная
проверенность и известные ограничения указаны в [описании выпуска](docs/release-0.1.0.md).
PicoCalc-образы используют JCS southbridge firmware.

## Сборка следующего выпуска

Полная сборка из чистого commit включает также SDK LexxCE для ESP32.

```sh
cmake --preset release
cmake --build --preset release
```

В `dist/LexxOS-<версия>/` появятся 15 ZIP, `SHA256SUMS` и `release.json`.
Чтобы собрать только библиотеку CE с заголовками и примером, используйте `python3 tools/release.py --ce-only`.
Подробнее — [упаковка релиза](docs/release-packaging.md).

## Гостевые приложения

В `/apps` находятся `fileman`, `pacman`, `snake` и `tetris`. Они входят в обычную сборку;
отдельно собираются через `cmake --preset apps` и `cmake --build --preset apps`.
Готовые программы: `build/apps/*.lexx`. Требования и запуск описаны в [apps/README.md](apps/README.md).

## Проверки

Быстрые проверки переносимого кода без MCU:

```bash
cmake -S . -B build/host-checks \
  -DLEXXOS_PLATFORM=host \
  -DLEXXOS_BOARD=host_native \
  -DLEXXOS_ENABLE_STATIC_CHECKS=ON
cmake --build build/host-checks
ctest --test-dir build/host-checks --output-on-failure
```

[Проверки разработки](docs/development-checks.md) описывают C++ contracts, статические проверки
и генерацию документации команд. [Аппаратные тест-планы](docs/tests/README.md) и
[simulator regression](tools/emulator/README.md) дополняют host checks.

## Документация

- [Индекс документации](docs/README.md)
- [PicoCalc / Luckfox Lyra M: единое руководство](docs/boards/picocalc_lyra_m.md)
- [План встраиваемого ядра LexxCE](docs/lexxce-plan.md)
- [SDK и графическая библиотека](docs/lexxos-sdk-graphics-roadmap-v2.md)
- [Команды shell](docs/commands/README.md)
- [Матрица команд и условий сборки](docs/command-matrix.md)
- [SD production-профиль](docs/sd-card-production.md) и [FatFs](docs/fatfs-backend.md)
- [PicoCalc](docs/boards/picocalc_pico_w.md) и [LCD-консоль](docs/picocalc-lcd-console.md)
- [JC-ESP32P4-M3-DEV](docs/boards/jc_esp32p4_m3_dev.md)
- [CLion](docs/clion.md) и [OpenOCD](docs/openocd-local.md)
- [Подготовка выпуска 0.1.0: состав, ограничения и checklist](docs/release-0.1.0.md)
- [Выпуск 0.1.1: изменения, проверки и ограничения](docs/release-0.1.1.md)
- [Состояние перехода к новой архитектуре](docs/hardware-migration-first-step.md)
- [План выпуска 0.1.0 и дальнейших работ](roadmap.md)

## Лицензия

Собственный код LexxOS распространяется под [Apache-2.0](LICENSE).
Авторство указано в [NOTICE](NOTICE), сторонние компоненты — в
[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).

Структура исходников, утилит и локальных данных описана в
[структуре проекта](docs/project-layout.md). Временные данные и дампы находятся
в `work/`, результаты сборки — в `build/`, релизные файлы — в `dist/`.
