# Регрессионная проверка M5Stack Cardputer-Adv 0.0.7

Имена тестовых файлов и идентификаторы портов в примерах условные; результаты измерений сохранены.

Дата проверки UART: 2026-07-13.
Устройство: M5Stack Cardputer-Adv / ESP32-S3FN8 Stamp-S3A.
Порт: `/dev/tty.usbmodemXXXX`, shell, 115200 бод.
Прошивка: `user-espidf-m5stack_cardputer`, профиль платы `m5stack_cardputer_adv`.

## Сборка и прошивка

```bash
cmake --preset user-espidf-m5stack_cardputer
cmake --build --preset user-espidf-m5stack_cardputer-build
ESPPORT=/dev/tty.usbmodemXXXX ESPBAUD=921600 cmake --build build/configurator/cardputer --target app-flash
```

Итог после изменений размещения ROM во flash, повторного использования слотов
планировщика, освобождения PCM и звука в меню:

- сборка пройдена;
- определения компонента ESP-IDF проверены: `LEXXOS_GAMEBOY_ROM_CACHE_BANKS=3`,
  `LEXXOS_GAMEBOY_ROM_CACHE_PAGE_BYTES=8192`, `LEXXOS_GAMEBOY_DYNAMIC_ROM_CACHE=1`;
- размер `LexxOS.bin` — `0xdcad0` байт, свободно в разделе приложения
  `0x213530` байт (71%);
- прошивка через `/dev/tty.usbmodemXXXX`, `ESPBAUD=921600` успешна;
- время сборки из `version`: `Jul 13 2026 09:58:01`.

## Быстрая проверка через UART

| Блок                     | Команда                                                                                    | Результат                                                                                                                                                                               |
|--------------------------|--------------------------------------------------------------------------------------------|-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| Версия                   | `version`                                                                                  | Пройдено: `platform: espidf`, `board: m5stack_cardputer_adv`, `milestone: fat-minimal-v1`; поле `version:` пусто в этой сборке                                                          |
| Устройства               | `devices`                                                                                  | Пройдено: `sd0`, `m5stack-lcd`, `cardputer-keyboard`, `cardputer-gamepad`, `cardputer-bmi270`, `cardputer-ir`, `cardputer-adv-audio`, `cardputer-adv-mic` — все `ready`                 |
| LCD                      | `display-info`, `display-test`                                                             | Пройдено в предыдущем прогоне того же дня: `240x135`, `rgb565`, полноэкранный вывод работает                                                                                            |
| SD/FAT                   | `ls /mnt/sd/gb`, `cat /mnt/sd/apps/gbemu/settings.ini`, `gb-info /mnt/sd/gb/test-rom-2.gb` | Пройдено: список FAT и `cat` без переполнения стека; ROM `title: ZELDA`, MBC1+RAM+BATTERY, контрольная сумма верна                                                                      |
| Питание                  | `power`                                                                                    | Пройдено: `source=cardputer-adc-gpio10`, `4126mV`, `97%`, `charging=unavailable`                                                                                                        |
| Строка состояния         | `console-info`                                                                             | Пройдено: `status battery: 99% charging=no`, строка обновлена                                                                                                                           |
| Клавиатура без ввода     | `keyboard-live -s 3 -i 50`                                                                 | Пройдено: `events=0`, `changes=0`, ложных событий при подключённом USB Serial/JTAG нет                                                                                                  |
| Геймпад без ввода        | `gamepad-test`, `gamepad-live -s 3 -i 50`                                                  | Пройдено: все кнопки 0, `changes=0`                                                                                                                                                     |
| Раскладка геймпада       | профиль платы                                                                              | Проверено по коду: `e/z/a/d` — вверх/вниз/влево/вправо, `[`/`]` — A/B, `;` — X, обратная кавычка — Y, Enter — Start, Backspace — Select                                                 |
| BMI270                   | `imu -n 3 -i 50`                                                                           | Пройдено: `chip=0x24`, `status=0xD0`, ненулевые ускорение/гироскоп, температура около `34.6C`                                                                                           |
| Сведения ИК              | `ir info`                                                                                  | Пройдено: `cardputer-ir`, `pin: 44`, `carrier-hz: 38000`, выведены коды Samsung power/on/off                                                                                            |
| ИК-передача              | `ir samsung-power -r 1`                                                                    | Пройдено: `ir: sent`; реакция приёмника/ТВ требует ручной проверки при прямой видимости                                                                                                 |
| Сведения аудио           | `audio`                                                                                    | Пройдено в предыдущем прогоне того же дня: `cardputer-adv-audio`, `tone: yes`, `mix: no`, `pcm: yes`                                                                                    |
| PCM                      | `mplay --test`                                                                             | Пройдено в предыдущем прогоне того же дня: `441000` кадров, без опустошения буфера/ожиданий заполнения, около 10–11 с                                                                   |
| Захват микрофона         | `mic sample`                                                                               | Пройдено после `gbemu --audio`: `frames=512`, `sample-rate=44100`, `peak=32768`, `average=14501`                                                                                        |
| Микрофон через динамик   | `mic loopback -s 1 -g 2`                                                                   | Пройдено после `gbemu`: `frames=43520`, `chunks=85`, `peak=32768`                                                                                                                       |
| Game Boy без экрана      | `gbemu --profile --frames 60 --display none /mnt/sd/gb/test-rom-2.gb`                      | Пройдено: старые настройки `rom_storage = file-cache`, но игровой режим выбрал `rom-storage=flash`; `rom-cache=8Kx6=48K`, `cache-fill=0`, `wall-ms=808`, `fps=74.25`, `realtime-x=1.24` |
| Game Boy с экраном       | `gbemu --frames 60 --display crop /mnt/sd/gb/test-rom-2.gb`                                | Пройдено: `display-mode: crop`, `destination: 160x135 @ 40,0`, `cpu-ms=691`, `ppu-ms=74`, `blit-ms=211`; возврат в shell                                                                |
| Game Boy со звуком       | `gbemu --audio --frames 2 --display none /mnt/sd/gb/test-rom-2.gb` (дважды)                | Пройдено дважды: `rom-storage=flash`, `audio=1`, `status=ok`; после освобождения PCM `mem` вернулся примерно к `86456 bytes`                                                            |
| Длительный звук Game Boy | `gbemu --audio --profile --frames 600 --display none /mnt/sd/gb/test-rom-2.gb`             | Пройдено: `audio=1`, `audio frames queued/written=221517/221517`, `wall-ms=9888`, `fps=60.67`, `realtime-x=1.01`, `status=ok`                                                           |
| Звук в меню Game Boy     | `gbemu-config --show-defaults`, проверка кода                                              | Пройдено: по умолчанию `[audio] enabled = true`; старые настройки без секции используют это значение, меню передаёт `--audio`                                                           |
| Слоты планировщика       | 14 интерактивных команд, включая `cat`, `gbemu`, `mic`, `tasks`                            | Пройдено: 14 интерактивных команд, нет `task-create-failed`; остались только `console-mirror`, `console`, `cardputer-input`, `system-events`                                            |
| Куча после Game Boy      | `mem`                                                                                      | Пройдено: после flash ROM, PCM и микрофона `68272 bytes`, минимум `47124 bytes`; лишних задач нет                                                                                       |
| Очистка                  | `clear`, визуальная проверка                                                               | Пройдено в предыдущем прогоне того же дня: очищается вся область LCD под строкой состояния                                                                                              |

## Аудио и микрофон

Для сравнения PCM использовали запись с PicoCalc до исправлений M5.
В прогоне Game Boy 13.07.2026 звук записывался
на macOS через `ffmpeg` с `Wireless GO RX` при
`gbemu --audio --profile --frames 600`. Пик — `23226`, RMS низкий: это
согласуется с тихим/редким линейным сигналом или настройкой усиления,
а не эффектом расстояния до источника. UART подтвердил запись `221517/221517` кадров.

Микрофон `cardputer-adv-mic` использует тот же ES8311/I2S. `mic loopback`
захватывает блоки по 512 кадров и передаёт основному `AudioDevice`.
После `drainPcmOutput()`/`stopPcmOutput()` ES8311 освобождает I2S/DMA;
реестр повторно инициализирует кодек по следующему запросу аудио/микрофона.

## Питание

Cardputer/Cardputer-Adv не предоставляют достоверных аппаратных измерений
тока или состояния зарядки. LexxOS читает напряжение ADC на GPIO10 и оценивает процент:

- `power` выводит `charging=unavailable`;
- строка состояния показывает процент без значка зарядки;
- USB-питание может завышать процент из-за напряжения;
- для зарядки Cardputer-Adv боковой выключатель должен быть в положении `ON`.

## Датчик, ИК и Game Boy

BMI270 отвечает по I2C `0x69` на общей системной/отладочной шине `G8/G9`.
При регистрации загружается конфигурация Bosch; без неё ID читается,
но показания акселерометра/гироскопа нулевые или недействительные.

ИК `cardputer-ir` на GPIO44 передаёт Samsung с несущей 38 кГц:
`ir samsung-power` — `0xE0E040BF`; доступны `samsung-on`, `samsung-off`,
`samsung-code -c <hex>`. UART подтверждает только завершение передачи.

Game Boy требует передачи определений кеша ROM компоненту ESP-IDF;
без них рабочая область слишком велика для внутренней RAM. Используются
динамический кеш `8Kx6=48K` и раздел `storage` по `0x300000`, 1 МиБ,
с отображением ROM во flash. Первый запуск копирует и проверяет ROM;
повторный сообщает `reused mapped flash ROM bytes=524288`, избегая
промахов кеша SD/FAT.
