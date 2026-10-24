# `audio`

## Исходник

`src/lexxos/apps/utility/audio_program.h`

## Подробное описание

Показывает primary `AudioDevice` и проигрывает простой тестовый тон через переносимый audio contract. На PicoCalc это
пользовательский путь поверх `picocalc-audio`, в отличие от низкоуровневой GPIO/PWM диагностики `beep`.

## Параметры

- Без параметров или `info` — показать устройство, готовность, поддержку tone/mix и диапазоны.
- `left|right|stereo` — проиграть тон в выбранный канал.
- `--frequency <hz>` / `-f <hz>` — необязательная частота тона; допустимый диапазон сообщает устройство.
- `--duration <ms>` / `-d <ms>` — необязательная длительность; допустимый максимум сообщает устройство.
- `--volume <0..255>` / `-v <0..255>` — необязательная громкость PWM duty; по умолчанию `128`.

## Примеры

```text
lexx:/> audio
lexx:/> audio left
lexx:/> audio stereo --frequency 1200 --duration 200 --volume 96
```

## Комментарий автора

Новые уведомления, MIDI playback и будущие интерактивные приложения должны идти через `AudioDevice`, чтобы не
привязываться к конкретным GPIO/PWM выводам платы.

## Справка shell

```text
audio - show or test the primary audio device

Uses DeviceManager and the portable AudioDevice API to show board audio capabilities or play a simple test tone.

Usage:
  audio [info|left|right|stereo] [--frequency <hz>] [--duration <ms>] [--volume <0..255>]

Arguments:
  target - target action or channel
  -f, --frequency <hz> - tone frequency in Hz
  -d, --duration <ms> - tone duration in milliseconds
  -v, --volume <0..255> - PWM duty volume, default 128
```

## Доступность

Доступна в стандартной или full-сборке в зависимости от feature-флагов платы. Документ соответствует LexxOS 0.0.7.
