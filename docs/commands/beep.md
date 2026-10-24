# `beep`

Вариант команды зависит от платформы. Приведённая ниже справка AudioDevice относится к simulator/ESP-IDF.

## PicoCalc

На `picocalc_pico_w` и `picocalc_pico2w` используется GPIO/PWM-реализация
`src/lexxos/apps/picocalc/beep_program.cpp`:

```text
beep [--frequency <hz>] [--duration <ms>]
```

Частота: 20–20000 Гц; длительность: до 5000 мс. Параметра `--volume` нет.
Значения по умолчанию задаются конфигурацией платы.

## Исходник AudioDevice

`src/lexxos/apps/utility/beep_program.h`

## Подробное описание

Проигрывает короткий диагностический тон через переносимый `AudioDevice`. В sim-сборках команда использует
`emu_pwm_audio` или `emu_adc_audio`, чтобы быстро проверить аудиовывод эмулятора без PicoCalc GPIO/PWM зависимостей.

## Параметры

- `--frequency <hz>` / `-f <hz>` — частота тона; по умолчанию берется из устройства.
- `--duration <ms>` / `-d <ms>` — длительность; по умолчанию берется из устройства.
- `--volume <0..255>` / `-v <0..255>` — громкость; по умолчанию `128`.

## Примеры

```text
lexx:/> beep
lexx:/> beep --frequency 1200 --duration 80 --volume 96
```

## Комментарий автора

На PicoCalc `beep` остаётся простой диагностикой распиновки платы. На Cardputer-Adv `beep` уже идёт через тот же
primary `AudioDevice`, что и `audio`.

## Справка shell

```text
beep - play a short audio beep

Plays a short diagnostic tone through the primary AudioDevice.

Usage:
  beep [--frequency <hz>] [--duration <ms>] [--volume <0..255>]

Arguments:
  -f, --frequency <hz> - tone frequency in Hz
  -d, --duration <ms> - beep duration in milliseconds
  -v, --volume <0..255> - tone volume, default 128
```

## Доступность

Регистрация и наличие аудиоустройства зависят от платы. Точные параметры проверяйте через `help beep`
или в [справочнике своей сборки](../command-docs.md).
