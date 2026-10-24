# Команды LexxOS

Общий справочник команд разных платформ LexxOS. Наличие страницы не означает доступность команды на выбранной плате.
Точный набор и параметры: shell `help` или [генерация справочника конкретной сборки](../command-docs.md).

[Матрица команд](../command-matrix.md) — текущие условия включения, уровни взаимодействия и отключаемые группы.

`gpio-analyzer` — экспериментальная программа, включаемая только в full на поддерживаемых платах.

- [`append`](append.md) - append text to a writable VFS file.
- [`audio`](audio.md) - Показывает primary `AudioDevice` и проигрывает простой тестовый тон через переносимый audio
  contract. На PicoCalc это пользовательский путь поверх `picocalc-audio`, в отличие от низкоуровневой GPIO/PWM
  диагностики `beep`.
- [`backlight`](backlight.md) - Показывает или меняет уровни LCD/keyboard подсветки через board-level power/backlight
  service. На PicoCalc команда использует `PicoCalcSouthbridgeService`, а на дисплеях с device-level backlight управляет
  подсветкой через `DisplayDevice`.
- [`beep`](beep.md) - Проигрывает короткий диагностический тон через переносимый `AudioDevice`. В sim-сборках команда
  использует `emu_pwm_audio` или `emu_adc_audio`, чтобы быстро проверить аудиовывод эмулятора без PicoCalc GPIO/PWM
  зависимостей.
- [`bench`](bench.md) - run portable LexxOS system benchmark.
- [`block-devs`](block-devs.md) - show registered block devices.
- [`board`](board.md) - show active board descriptor.
- [`bootsel`](bootsel.md) - reboot RP2040 into USB BOOTSEL loader.
- [`cat`](cat.md) - print readable VFS file or stdin.
- [`cd`](cd.md) - change current VFS directory.
- [`clear`](clear.md) - clear ANSI terminal.
- [`console-info`](console-info.md) - show PicoCalc LCD console state.
- [`console-palette`](console-palette.md) - Печатает ANSI SGR образцы foreground/background цветов. UART-terminal
  получает обычные escape-последовательности, а LCD console mirror разбирает тот же поток и рисует packed 16-color
  RGB565 palette.
- [`cp`](cp.md) - copy readable VFS file to writable VFS file.
- [`devices`](devices.md) - list registered devices.
- [`devs`](devs.md) - show registered devices.
- [`display-bars`](display-bars.md) - draw display color bars.
- [`display-fill`](display-fill.md) - fill display with one color.
- [`display-info`](display-info.md) - show display device information.
- [`display-power`](display-power.md) - Показывает или меняет режим питания display controller. На PicoCalc поверх
  режима дисплея применяется board policy: `off`/`sleep` гасят LCD и keyboard backlight, а `on` восстанавливает
  последние уровни подсветки. На ESP32-P4 DSI дисплеях команда использует device-level power/backlight API.
- [`display-rect`](display-rect.md) - draw a filled rectangle.
- [`display-test`](display-test.md) - draw a test pattern or benchmark display throughput.
- [`display-text`](display-text.md) - draw simple text on display.
- [`echo`](echo.md) - print program arguments.
- [`edit`](edit.md) - edit a VFS text file.
- [`eth`](eth.md) - profile-specific command.
- [`exit`](exit.md) - exit host simulation.
- [`fat-cache`](fat-cache.md) - show FAT disk cache statistics.
- [`fs-info`](fs-info.md) - show VFS diagnostic information.
- [`fs-smoke`](fs-smoke.md) - run VFS writable filesystem smoke test.
- [`gamepad-live`](gamepad-live.md) - run a live gamepad input test.
- [`gamepad-test`](gamepad-test.md) - print current gamepad state.
- [`gb-bus`](gb-bus.md) - test Game Boy memory bus.
- [`gb-cart`](gb-cart.md) - test Game Boy cartridge mapper.
- [`gb-cpu-step`](gb-cpu-step.md) - step Game Boy CPU core.
- [`gb-info`](gb-info.md) - show Game Boy ROM cartridge header.
- [`gb-io`](gb-io.md) - test Game Boy IO registers.
- [`gb-irq`](gb-irq.md) - test Game Boy CPU interrupt handling.
- [`gb-ppu-test`](gb-ppu-test.md) - render Game Boy PPU background test.
- [`gb-rom-check`](gb-rom-check.md) - verify repeated Game Boy ROM reads.
- [`gb-test-run`](gb-test-run.md) - run Game Boy hardware test ROM with serial capture.
- [`gb-test-suite`](gb-test-suite.md) - run Blargg Game Boy CPU individual test suite.
- [`gb-vram-view`](gb-vram-view.md) - show Game Boy VRAM tile data.
- [`gbemu`](gbemu.md) - run Game Boy ROMs or open the ROM launcher.
- [`gbemu-config`](gbemu-config.md) - show or initialize Game Boy emulator settings.
- [`gpio-analyzer`](gpio-analyzer.md) - show and control user GPIO pins.
- [`help`](help.md) - show program list.
- [`hexdump`](hexdump.md) - print file or stdin as hexadecimal bytes.
- [`hosted-info`](hosted-info.md) - Показывает информацию об ESP-Hosted coprocessor, если активная плата использует
  ESP32-C6/ESP-Hosted для Wi-Fi.
- [`hosted-ota`](hosted-ota.md) - Обновляет ESP32-C6 ESP-Hosted slave firmware из binary-файла в VFS.
- [`hosted-ota-rx`](hosted-ota-rx.md) - Обновляет ESP32-C6 ESP-Hosted slave firmware напрямую из UART/YMODEM потока.
- [`i2c-buses`](i2c-buses.md) - Показывает I2C-шины, зарегистрированные в `DeviceManager`. Команда помогает понять,
  какие имена шин доступны для `i2c-scan`, RTC-команд и низкоуровневой диагностики первичного запуска.
- [`i2c-scan`](i2c-scan.md) - Сканирует зарегистрированную I2C-шину и печатает таблицу адресов в стиле `i2cdetect`.
  Команда полезна для первичного запуска плат, проверки DS3231 и осторожной диагностики периферии.
- [`imu`](imu.md) - Reads samples from the primary motion sensor.
- [`info`](info.md) - показывает сведения о файле или каталоге, атрибуты FAT и манифест LEXX.
- [`input-test`](input-test.md) - print pending input events.
- [`ir`](ir.md) - Shows or tests the primary IR transmitter.
- [`jobs`](jobs.md) - Показывает таблицу процессов `ProgramSupervisor`: id, состояние, имя программы, размер стека,
  свободный стек, priority, memory class, внешнюю квоту arena, exit code и launch status.
- [`keyboard-live`](keyboard-live.md) - run a live keyboard input test.
- [`keyboard-raw`](keyboard-raw.md) - run a live raw matrix keyboard test.
- [`led`](led.md) - set status LED state.
- [`lexx-info`](lexx-info.md) - inspect a .lexx executable without running it.
- [`lexx-run`](lexx-run.md) - Запускает внешний исполняемый файл `.lexx` из VFS/SD через экспериментальный RV32I
  runtime. Команда валидирует заголовок LEXX, выделяет ограниченную guest-память, передает guest-программе `argc/argv` и
  выполняет системные вызовы текстовой консоли, клавиатуры и read-only файлового доступа.
- [`ls`](ls.md) - list VFS directory.
- [`mem`](mem.md) - show kernel memory statistics.
- [`mic`](mic.md) - show or test the primary microphone.
- [`mkdir`](mkdir.md) - create a writable VFS directory.
- [`mount`](mount.md) - show or create VFS mounts.
- [`mplay`](mplay.md) - MIDI player.
- [`mplayer`](mplayer.md) - Опциональная команда: в standard-сборке LexxOS 0.0.7 отключена по умолчанию через
  `LEXXOS_FEATURE_MIDI=OFF`.
- [`mv`](mv.md) - rename a writable VFS file or directory.
- [`net-status`](net-status.md) - Показывает состояние сетевых интерфейсов через общий сетевой backend платы.
- [`picocalc-sb`](picocalc-sb.md) - Показывает состояние PicoCalc southbridge через зарегистрированный сервис.
- [`ping`](ping.md) - Проверяет реальное IP-соединение через ICMP echo request/reply на активном сетевом интерфейсе.
- [`power`](power.md) - Показывает состояние питания платы, батареи и поддержки программного выключения. На PicoCalc
  команда читает service-backed snapshot `picocalc-southbridge` без прямого I2C-доступа из shell.
- [`poweroff`](poweroff.md) - Синхронизирует и размонтирует writable файловые системы, затем запрашивает выключение
  платы, если профиль устройства поддерживает board-level power-off. На PicoCalc запрос идёт через
  `picocalc-southbridge` service.
- [`psram-info`](psram-info.md) - read external PSRAM JEDEC ID.
- [`psram-test`](psram-test.md) - test external PSRAM read/write.
- [`pwd`](pwd.md) - show current VFS directory.
- [`reboot`](reboot.md) - reboot target device.
- [`rename`](rename.md) - rename a writable VFS file or directory.
- [`rm`](rm.md) - remove a writable VFS file.
- [`rmdir`](rmdir.md) - remove an empty writable VFS directory.
- [`rtc-config`](rtc-config.md) - save RTC settings for boot restore.
- [`rtc-get`](rtc-get.md) - Читает дату и время из выбранного RTC backend и печатает источник, значение RTC и weekday.
  Если oscillator-stop, voltage-low или software-valid flag требует внимания, команда предупреждает, что время может
  быть недостоверным.
- [`rtc-set`](rtc-set.md) - Записывает дату и время в выбранный RTC backend. Общий диапазон LexxOS —
  `2000-01-01..2199-12-31`; PicoCalc JCS RTC поддерживает `2000..2099`, software RTC хранит время в RAM до выключения
  питания и использует Unix range.
- [`rtc-status`](rtc-status.md) - Читает статус выбранного RTC backend: источник, raw status byte,
  oscillator-stop/voltage-low flags, software RTC snapshot и дополнительные поля JCS firmware, когда они доступны.
- [`rtc-sync`](rtc-sync.md) - Синхронизирует выбранный RTC backend с NTP сервером через активный сетевой интерфейс.
- [`run`](run.md) - Запускает программу через экспериментальный `ProgramSupervisor` task path. Foreground-запуск ждёт
  завершения дочерней task, а background-запуск возвращает shell сразу и показывает process id для `jobs`/`stop`.
- [`sd-baud`](sd-baud.md) - show or set SD SPI baud rate.
- [`sd-bench`](sd-bench.md) - benchmark SD raw read and FAT read/write.
- [`sd-debug`](sd-debug.md) - show or control SD driver diagnostics.
- [`sd-multiblock`](sd-multiblock.md) - show or set SD CMD18 multi-block read usage.
- [`sd-multiwrite`](sd-multiwrite.md) - show or set experimental SD CMD25 multi-block writes.
- [`sd-profile`](sd-profile.md) - apply known SD SPI profile.
- [`sd-read-crc`](sd-read-crc.md) - show or set SD read CRC validation.
- [`sd-speed-scan`](sd-speed-scan.md) - find stable SD SPI baud rate.
- [`sd-spi-mode`](sd-spi-mode.md) - show or set SD SPI clock mode.
- [`sd-stress`](sd-stress.md) - run repeated FAT create/read/delete test.
- [`sd-test`](sd-test.md) - run SD FAT read/write smoke test.
- [`shell`](shell.md) - start interactive LexxOS shell.
- [`shell-config`](shell-config.md) - Показывает и обновляет настройки shell line editor. Сейчас config хранит состояние
  fish-like autosuggestion и историю ввода, которую shell лениво загружает из `/mnt/sd/.lexx/shell.conf` после появления
  SD mount.
- [`stat`](stat.md) - show VFS node metadata.
- [`stop`](stop.md) - Запрашивает cooperative cancellation для процесса, созданного `ProgramSupervisor`. Команда не
  убивает task принудительно: процесс должен сам проверить `CancellationToken` и завершиться.
- [`storage`](storage.md) - show, probe, read or diagnose block storage.
- [`sync`](sync.md) - synchronize mounted writable filesystems.
- [`tasks`](tasks.md) - show registered kernel tasks.
- [`tmp-info`](tmp-info.md) - show /tmp diagnostic information.
- [`top`](top.md) - watch memory, tasks and supervised jobs.
- [`touch`](touch.md) - create an empty writable VFS file.
- [`touch-calibrate`](touch-calibrate.md) - calibrate touch panel.
- [`touch-draw`](touch-draw.md) - draw with one or more fingers.
- [`touch-info`](touch-info.md) - show touch panel state.
- [`umount`](umount.md) - unmount a VFS mount point.
- [`uptime`](uptime.md) - show system uptime.
- [`version`](version.md) - show LexxOS build and feature milestone.
- [`wifi-config`](wifi-config.md) - Сохраняет параметры Wi-Fi сети для автоматического подключения платы после загрузки.
- [`wifi-connect`](wifi-connect.md) - Подключает активную плату к Wi-Fi сети через board-level WiFi backend.
- [`wifi-scan`](wifi-scan.md) - scan Wi-Fi networks.
- [`wifi-status`](wifi-status.md) - Показывает состояние активного Wi-Fi STA-интерфейса через board-level WiFi backend.
- [`write`](write.md) - write text to a writable VFS file.
- [`yrx`](yrx.md) - receive a file over YMODEM/CRC.
- [`ysend`](ysend.md) - send a VFS file over YMODEM/CRC.

## Дополнительные платформенные команды

- [`esp32-info`](esp32-info.md) — ESP-IDF.
- [`esp32-spi-test`](esp32-spi-test.md) — ESP-IDF.
- [`mouse-info`](mouse-info.md) — семейство JC ESP32-P4.
- [`mouse-reset`](mouse-reset.md) — семейство JC ESP32-P4.
- [`mouse-live`](mouse-live.md) — семейство JC ESP32-P4.
- [`usb-hid-info`](usb-hid-info.md) — семейство JC ESP32-P4.
- [`picocalc-info`](picocalc-info.md) — семейство PicoCalc.
