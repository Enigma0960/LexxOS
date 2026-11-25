# Команды LexxOS

Общий справочник команд разных платформ LexxOS. Наличие страницы не означает доступность команды на выбранной плате.
Точный набор и параметры: shell `help` или [генерация справочника конкретной сборки](../command-docs.md).

[Матрица команд](../command-matrix.md) — текущие условия включения, уровни взаимодействия и отключаемые группы.

Дословная справка в блоках «Справка shell» сохранена на языке прошивки;
описания и пояснения справочника написаны по-русски.

`gpio-analyzer` — экспериментальная программа, включаемая только в full на поддерживаемых платах.

- [`append`](append.md) - Дописывает текст в доступный для записи файл VFS.
- [`audio`](audio.md) - Показывает primary `AudioDevice` и проигрывает простой тестовый тон через переносимый audio
  contract. На PicoCalc это пользовательский путь поверх `picocalc-audio`, в отличие от низкоуровневой GPIO/PWM
  диагностики `beep`.
- [`backlight`](backlight.md) - Показывает или меняет уровни LCD/keyboard подсветки через board-level power/backlight
  service. На PicoCalc команда использует `PicoCalcSouthbridgeService`, а на дисплеях с device-level backlight управляет
  подсветкой через `DisplayDevice`.
- [`beep`](beep.md) - Проигрывает короткий диагностический тон через переносимый `AudioDevice`. В sim-сборках команда
  использует `emu_pwm_audio` или `emu_adc_audio`, чтобы быстро проверить аудиовывод эмулятора без PicoCalc GPIO/PWM
  зависимостей.
- [`bench`](bench.md) - Запускает переносимый системный тест производительности LexxOS.
- [`block-devs`](block-devs.md) - Показывает зарегистрированные блочные устройства.
- [`board`](board.md) - Показывает описание активной платы.
- [`bootsel`](bootsel.md) - Перезагружает RP2040 в USB-загрузчик BOOTSEL.
- [`cat`](cat.md) - Выводит содержимое файла VFS или стандартного ввода.
- [`cd`](cd.md) - Меняет текущий каталог VFS.
- [`clear`](clear.md) - Очищает ANSI-терминал.
- [`console-info`](console-info.md) - Показывает состояние LCD-консоли PicoCalc.
- [`console-palette`](console-palette.md) - Печатает ANSI SGR образцы foreground/background цветов. UART-terminal
  получает обычные escape-последовательности, а LCD console mirror разбирает тот же поток и рисует packed 16-color
  RGB565 palette.
- [`cp`](cp.md) - Копирует файл VFS в доступный для записи файл.
- [`devices`](devices.md) - Перечисляет зарегистрированные устройства.
- [`devs`](devs.md) - Показывает зарегистрированные устройства.
- [`display-bars`](display-bars.md) - Рисует цветные полосы на дисплее.
- [`display-fill`](display-fill.md) - Заполняет дисплей одним цветом.
- [`display-info`](display-info.md) - Показывает сведения об устройстве дисплея.
- [`display-power`](display-power.md) - Показывает или меняет режим питания display controller. На PicoCalc поверх
  режима дисплея применяется board policy: `off`/`sleep` гасят LCD и keyboard backlight, а `on` восстанавливает
  последние уровни подсветки. На ESP32-P4 DSI дисплеях команда использует device-level power/backlight API.
- [`display-rect`](display-rect.md) - Рисует заполненный прямоугольник.
- [`display-test`](display-test.md) - Рисует тестовое изображение или измеряет скорость вывода на дисплей.
- [`display-text`](display-text.md) - Рисует простой текст на дисплее.
- [`echo`](echo.md) - Выводит аргументы программы.
- [`edit`](edit.md) - Редактирует текстовый файл VFS.
- [`eth`](eth.md) - Команда Ethernet с параметрами, зависящими от профиля платы.
- [`exit`](exit.md) - Завершает симуляцию на компьютере.
- [`fat-cache`](fat-cache.md) - Показывает статистику дискового кеша FAT.
- [`fs-info`](fs-info.md) - Показывает диагностические сведения VFS.
- [`fs-smoke`](fs-smoke.md) - Проверяет основные операции файловой системы VFS с записью.
- [`gamepad-live`](gamepad-live.md) - Проверяет события геймпада в реальном времени.
- [`gamepad-test`](gamepad-test.md) - Показывает текущее состояние геймпада.
- [`gb-bus`](gb-bus.md) - Проверяет шину памяти Game Boy.
- [`gb-cart`](gb-cart.md) - Проверяет контроллер отображения памяти картриджа Game Boy.
- [`gb-cpu-step`](gb-cpu-step.md) - Пошагово выполняет инструкции CPU Game Boy.
- [`gb-info`](gb-info.md) - Показывает заголовок ROM-картриджа Game Boy.
- [`gb-io`](gb-io.md) - Проверяет регистры ввода-вывода Game Boy.
- [`gb-irq`](gb-irq.md) - Проверяет обработку прерываний CPU Game Boy.
- [`gb-ppu-test`](gb-ppu-test.md) - Рисует тестовый фон PPU Game Boy.
- [`gb-rom-check`](gb-rom-check.md) - Проверяет повторные чтения ROM Game Boy.
- [`gb-test-run`](gb-test-run.md) - Запускает аппаратный тестовый ROM Game Boy с захватом последовательного вывода.
- [`gb-test-suite`](gb-test-suite.md) - Запускает набор отдельных CPU-тестов Blargg для Game Boy.
- [`gb-vram-view`](gb-vram-view.md) - Показывает данные тайлов VRAM Game Boy.
- [`gbemu`](gbemu.md) - Запускает ROM Game Boy или открывает меню выбора ROM.
- [`gbemu-config`](gbemu-config.md) - Показывает или создаёт настройки эмулятора Game Boy.
- [`gpio-analyzer`](gpio-analyzer.md) - Показывает пользовательские GPIO и управляет ими.
- [`help`](help.md) - Показывает список программ.
- [`hexdump`](hexdump.md) - Выводит файл или стандартный ввод в виде шестнадцатеричных байтов.
- [`hosted-info`](hosted-info.md) - Показывает информацию об ESP-Hosted coprocessor, если активная плата использует
  ESP32-C6/ESP-Hosted для Wi-Fi.
- [`hosted-ota`](hosted-ota.md) - Обновляет ESP32-C6 ESP-Hosted slave firmware из binary-файла в VFS.
- [`hosted-ota-rx`](hosted-ota-rx.md) - Обновляет ESP32-C6 ESP-Hosted slave firmware напрямую из UART/YMODEM потока.
- [`i2c-buses`](i2c-buses.md) - Показывает I2C-шины, зарегистрированные в `DeviceManager`. Команда помогает понять,
  какие имена шин доступны для `i2c-scan`, RTC-команд и низкоуровневой диагностики первичного запуска.
- [`i2c-scan`](i2c-scan.md) - Сканирует зарегистрированную I2C-шину и печатает таблицу адресов в стиле `i2cdetect`.
  Команда полезна для первичного запуска плат, проверки DS3231 и осторожной диагностики периферии.
- [`imu`](imu.md) - Считывает измерения основного датчика движения.
- [`info`](info.md) - показывает сведения о файле или каталоге, атрибуты FAT и манифест LEXX.
- [`input-test`](input-test.md) - Выводит ожидающие события ввода.
- [`ir`](ir.md) - Показывает или проверяет основной ИК-передатчик.
- [`jobs`](jobs.md) - Показывает таблицу процессов `ProgramSupervisor`: id, состояние, имя программы, размер стека,
  свободный стек, priority, memory class, внешнюю квоту arena, exit code и launch status.
- [`keyboard-live`](keyboard-live.md) - Проверяет события клавиатуры в реальном времени.
- [`keyboard-raw`](keyboard-raw.md) - Проверяет необработанные события матричной клавиатуры.
- [`led`](led.md) - Задаёт состояние индикаторного светодиода.
- [`lexx-info`](lexx-info.md) - Проверяет исполняемый файл .lexx без запуска.
- [`lexx-run`](lexx-run.md) - Запускает внешний исполняемый файл `.lexx` из VFS/SD через экспериментальный RV32I
  runtime. Команда валидирует заголовок LEXX, выделяет ограниченную guest-память, передает guest-программе `argc/argv` и
  выполняет системные вызовы текстовой консоли, клавиатуры и read-only файлового доступа.
- [`ls`](ls.md) - Показывает содержимое каталога VFS.
- [`mem`](mem.md) - Показывает статистику памяти ядра.
- [`mic`](mic.md) - Показывает или проверяет основной микрофон.
- [`mkdir`](mkdir.md) - Создаёт каталог в доступной для записи VFS.
- [`mount`](mount.md) - Показывает или создаёт точки монтирования VFS.
- [`mplay`](mplay.md) - Проигрывает MIDI.
- [`mplayer`](mplayer.md) - Опциональная команда: в standard-сборке LexxOS 0.0.7 отключена по умолчанию через
  `LEXXOS_FEATURE_MIDI=OFF`.
- [`mv`](mv.md) - Переименовывает или перемещает файл либо каталог VFS.
- [`net-status`](net-status.md) - Показывает состояние сетевых интерфейсов через общий сетевой backend платы.
- [`picocalc-sb`](picocalc-sb.md) - Показывает состояние PicoCalc southbridge через зарегистрированный сервис.
- [`ping`](ping.md) - Проверяет реальное IP-соединение через ICMP echo request/reply на активном сетевом интерфейсе.
- [`power`](power.md) - Показывает состояние питания платы, батареи и поддержки программного выключения. На PicoCalc
  команда читает service-backed snapshot `picocalc-southbridge` без прямого I2C-доступа из shell.
- [`poweroff`](poweroff.md) - Синхронизирует и размонтирует writable файловые системы, затем запрашивает выключение
  платы, если профиль устройства поддерживает board-level power-off. На PicoCalc запрос идёт через
  `picocalc-southbridge` service.
- [`psram-info`](psram-info.md) - Читает JEDEC ID внешней PSRAM.
- [`psram-test`](psram-test.md) - Проверяет чтение и запись внешней PSRAM.
- [`pwd`](pwd.md) - Показывает текущий каталог VFS.
- [`reboot`](reboot.md) - Перезагружает устройство.
- [`rename`](rename.md) - Переименовывает файл или каталог VFS.
- [`rm`](rm.md) - Удаляет доступный для записи файл VFS.
- [`rmdir`](rmdir.md) - Удаляет пустой каталог из доступной для записи VFS.
- [`rtc-config`](rtc-config.md) - Сохраняет настройки RTC для восстановления при загрузке.
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
- [`sd-baud`](sd-baud.md) - Показывает или задаёт частоту SPI карты SD.
- [`sd-bench`](sd-bench.md) - Измеряет скорость секторного чтения SD и операций FAT согласно реализации платы.
- [`sd-debug`](sd-debug.md) - Показывает или настраивает диагностику драйвера SD.
- [`sd-multiblock`](sd-multiblock.md) - Показывает или включает многоблочное чтение SD через CMD18.
- [`sd-multiwrite`](sd-multiwrite.md) - Показывает или включает экспериментальную многоблочную запись SD через CMD25.
- [`sd-profile`](sd-profile.md) - Применяет известный профиль SPI для SD.
- [`sd-read-crc`](sd-read-crc.md) - Показывает или включает проверку CRC при чтении SD.
- [`sd-speed-scan`](sd-speed-scan.md) - Определяет устойчивую частоту SPI карты SD.
- [`sd-spi-mode`](sd-spi-mode.md) - Показывает или задаёт режим тактирования SPI карты SD.
- [`sd-stress`](sd-stress.md) - Повторяет тест создания, чтения и удаления файлов FAT.
- [`sd-test`](sd-test.md) - Проверяет основные операции чтения и записи SD FAT.
- [`shell`](shell.md) - Запускает интерактивную оболочку LexxOS.
- [`shell-config`](shell-config.md) - Показывает и обновляет настройки shell line editor. Сейчас config хранит состояние
  fish-like autosuggestion и историю ввода, которую shell лениво загружает из `/mnt/sd/.lexx/shell.conf` после появления
  SD mount.
- [`stat`](stat.md) - Показывает метаданные узла VFS.
- [`stop`](stop.md) - Запрашивает cooperative cancellation для процесса, созданного `ProgramSupervisor`. Команда не
  убивает task принудительно: процесс должен сам проверить `CancellationToken` и завершиться.
- [`storage`](storage.md) - Показывает, определяет, читает и диагностирует блочные хранилища.
- [`sync`](sync.md) - Сбрасывает отложенные записи файловых систем.
- [`tasks`](tasks.md) - Показывает задачи ядра.
- [`tmp-info`](tmp-info.md) - Показывает сведения о временной файловой системе.
- [`top`](top.md) - Наблюдает за памятью, задачами и процессами супервизора.
- [`touch`](touch.md) - Создаёт пустой файл в доступной для записи VFS.
- [`touch-calibrate`](touch-calibrate.md) - Калибрует сенсорный экран.
- [`touch-draw`](touch-draw.md) - Рисует по событиям сенсорного экрана.
- [`touch-info`](touch-info.md) - Показывает сведения о сенсорном экране.
- [`umount`](umount.md) - Размонтирует файловую систему VFS.
- [`uptime`](uptime.md) - Показывает время работы системы.
- [`version`](version.md) - Показывает версию сборки LexxOS и этап реализации возможностей.
- [`wifi-config`](wifi-config.md) - Сохраняет параметры Wi-Fi сети для автоматического подключения платы после загрузки.
- [`wifi-connect`](wifi-connect.md) - Подключает активную плату к Wi-Fi сети через board-level WiFi backend.
- [`wifi-scan`](wifi-scan.md) - Ищет сети Wi-Fi.
- [`wifi-status`](wifi-status.md) - Показывает состояние активного Wi-Fi STA-интерфейса через board-level WiFi backend.
- [`write`](write.md) - Записывает текст в файл VFS.
- [`yrx`](yrx.md) - Принимает файл по YMODEM в VFS LexxOS.
- [`ysend`](ysend.md) - Передаёт файл VFS LexxOS по YMODEM.

## Дополнительные платформенные команды

- [`esp32-info`](esp32-info.md) — ESP-IDF.
- [`esp32-spi-test`](esp32-spi-test.md) — ESP-IDF.
- [`mouse-info`](mouse-info.md) — семейство JC ESP32-P4.
- [`mouse-reset`](mouse-reset.md) — семейство JC ESP32-P4.
- [`mouse-live`](mouse-live.md) — семейство JC ESP32-P4.
- [`usb-hid-info`](usb-hid-info.md) — семейство JC ESP32-P4.
- [`picocalc-info`](picocalc-info.md) — семейство PicoCalc.
