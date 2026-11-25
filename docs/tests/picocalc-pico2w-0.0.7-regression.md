# Регрессионная проверка PicoCalc Pico 2 W 0.0.7

Имена тестовых файлов и идентификаторы портов в примерах условные; результаты измерений сохранены.

Дата проверки UART: 2026-07-08.
Устройство: ClockworkPi PicoCalc с Raspberry Pi Pico 2 W.
Порт: `/dev/tty.usbserial-XXXX`, 115200 бод.
Прошивка: актуальная `0.0.7` перед релизом.

## Автоматическая проверка UART

| Блок                     | Команда                                                      | Результат                                                                                                       |
|--------------------------|--------------------------------------------------------------|-----------------------------------------------------------------------------------------------------------------|
| Версия                   | `version`                                                    | Пройдено: `version: 0.0.7`, `platform: pico`, `board: picocalc_pico2w`                                          |
| Плата                    | `board`                                                      | Пройдено: ClockworkPi PicoCalc с Raspberry Pi Pico 2 W, RP2350                                                  |
| Память                   | `mem`                                                        | Пройдено                                                                                                        |
| Задачи                   | `tasks`                                                      | Пройдено: таблица задач FreeRTOS                                                                                |
| Устройства               | `devices`                                                    | Пройдено                                                                                                        |
| Хранилище                | `storage`                                                    | Пройдено: SD готова                                                                                             |
| Блочные устройства       | `block-devs`                                                 | Пройдено                                                                                                        |
| VFS/FAT                  | `fs-info`                                                    | Пройдено: `/mnt/sd` смонтирован                                                                                 |
| Текущий каталог          | `pwd`                                                        | Пройдено: `/`                                                                                                   |
| Каталог SD               | `ls /mnt/sd`                                                 | Пройдено                                                                                                        |
| LCD-консоль              | `console-info`                                               | Пройдено: `ready: yes`, `text mode: yes`, `53x37`                                                               |
| Подсветка                | `backlight`                                                  | Пройдено: уровни прочитаны через сервис                                                                         |
| Сохранение подсветки     | `backlight save all`, `backlight restore all`                | Пройдено: `/mnt/sd/.lexx/backlight.conf`                                                                        |
| Питание дисплея          | `display-power`, `display-power off`, `display-power on`     | Пройдено: off/on восстанавливает подсветку LCD и клавиатуры                                                     |
| Тайм-аут дисплея         | `display-power timeout 60 sleep`, восстановление `300 sleep` | Пройдено: настройки сохранены и восстановлены                                                                   |
| STM32                    | `picocalc-sb`, `picocalc-sb --poll 1`                        | Пройдено: кешированный снимок, `keyboard-ready: yes`                                                            |
| Состояние RTC            | `rtc-status`                                                 | Пройдено: `source: picocalc-southbridge`, видна прошивка JCS                                                    |
| Чтение RTC               | `rtc-get`, `rtc-get --source picocalc`                       | Пройдено                                                                                                        |
| NTP/RTC                  | `rtc-sync --tz +07:00`                                       | Пройдено: `rtc sync: ok`                                                                                        |
| Сведения PSRAM           | `psram-info`                                                 | Пройдено: JEDEC ID `00 00 00 0D 5D 53 31 76`                                                                    |
| Данные PSRAM             | `psram-test`                                                 | Пройдено: реализация PIO, данные восстановлены                                                                  |
| Звуковой сигнал          | `beep --frequency 880 --duration 80`                         | Пройдено: выведены левый/правый выводы                                                                          |
| Аудио                    | `audio`                                                      | Пройдено: `picocalc-audio`, тон/смешивание готовы                                                               |
| Интерактивный супервизор | `run board`                                                  | Пройдено                                                                                                        |
| Процессы супервизора     | `jobs`, `jobs clear`                                         | Пройдено                                                                                                        |
| Поиск Wi-Fi              | `wifi-scan`                                                  | Пройдено                                                                                                        |
| Настройки Wi-Fi          | `wifi-config show`, `wifi-status`                            | Пройдено: настройки загружены, соединение установлено после задержки автоподключения                            |
| Сеть                     | `ping --count 1 --timeout 10000 8.8.8.8`                     | Пройдено: `1 packets transmitted, 1 received`                                                                   |
| NTP по сети              | `rtc-sync --tz +07:00`                                       | Пройдено через `pool.ntp.org`                                                                                   |
| Гостевое приложение      | `lexx-run /mnt/sd/ext/hello.lexx alpha beta`                 | Пройдено: загрузка, `argc/argv`, затем предусмотренное ожидание клавиатуры                                      |
| Прямой запуск приложения | `/mnt/sd/ext/hello.lexx direct`                              | Пройдено: загрузка и предусмотренное ожидание клавиатуры                                                        |
| Гостевой cat             | `lexx-run /mnt/sd/ext/cat.lexx /mnt/sd/test.txt`             | Пройдено: содержимое файла выведено                                                                             |
| Программный перезапуск   | `reboot`                                                     | Пройдено: автомонтирование SD, восстановление подсветки, возврат приглашения                                    |
| После перезапуска        | `version`, `wifi-status`, `rtc-get`, `console-info`          | Пройдено                                                                                                        |
| Выключение               | `poweroff`                                                   | Пройдено: `sync ok`, `/mnt/sd` размонтирован, питание выключено; после ручного включения загрузка возобновилась |

## Примечания

- `ping --count 1 --timeout 10000 pool.ntp.org` разрешил DNS, но один ICMP-запрос
  завершился по тайм-ауту. `8.8.8.8` ответил, а `rtc-sync --tz +07:00`
  с `pool.ntp.org` прошёл: Wi-Fi, DNS и UDP/NTP подтверждены.
- `hello.lexx` интерактивен: после `demo: waiting for keyboard input...`
  тайм-аут UART-проверки ожидаем; Ctrl+C возвращает shell.
- Прежняя ошибка `southbridge request failed` у `poweroff` не возникла:
  синхронизация/размонтирование завершились, питание выключилось,
  ручное включение вернуло загрузочный вывод.
- После прогона исправлены отложенные сообщения `wifi-autoconnect` /
  `FreeRTOS task start`, перекрывающие ввод, быстрая вставка UART и
  дополнение длинных MIDI-имён. Тесты на компьютере пройдены; поведение
  интерфейса Pico 2 W нужно подтвердить после перепрошивки.
- 11.07.2026 на COM110 проверены Game Boy/SD в двухъядерной SMP и
  контрольной одноядерной сборке. Запись/синхронизация SD/FAT прошли,
  Start+Select завершал `gbemu`. Единичное
  `[picocalc-keyboard] communication failure: stage=update-keyboard-fifo-read`
  встречалось в обоих режимах при `ready=yes`, `consecutive=1`,
  `snapshot-ok=yes`; это неблокирующее предупреждение, не регрессия SMP.
- 11.07.2026 после возврата `wifi-autoconnect` в `perf-picocalc-pico2w-gb`
  подтверждено: 266 МГц нарушали CYW43 в этой SMP/Game Boy сборке.
  `wifi-connect` задерживался в `join`, `wifi-scan` завершался по тайм-ауту.
  На 250 МГц автоподключение читает `/mnt/sd/.lexx/wifi.conf`, запускает
  `wifi-connect`, получает `link: up`; сети видны, а
  `ping --count 1 --timeout 10000 8.8.8.8` получает `1 received`.
