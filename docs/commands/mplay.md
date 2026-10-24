# `mplay`

MIDI player.

## Поведение

`mplay` загружает Standard MIDI file из VFS, рендерит его в stereo PCM и отправляет буферы в primary `AudioDevice`.
Backend должен поддерживать `AudioDevice::supportsPcmBuffer()`.

`mplay --test` не требует MIDI-файла и проигрывает детерминированный PCM pattern на preferred sample rate primary
`AudioDevice`. На Cardputer-Adv это `44100 Hz`, `441000` PCM frames и примерно 10-11 секунд выполнения, включая
reference beep. Это основной smoke для ES8311/I2S PCM output; ожидаемый `audio` вывод содержит `pcm: yes`.

Во время PCM-секций команда не печатает промежуточный вывод в консоль. Cancellation polling должен оставаться
неблокирующим, иначе маленькие PCM buffers начинают расходиться паузами и на устройстве появляются щелчки.

Документ соответствует LexxOS 0.0.7.

## Справка shell

```text
mplay - MIDI player

Usage:
  mplay [--test] | mplay [--dump] <midi>

Arguments:
  --dump - Print MIDI parser diagnostics and event stream
  --test - Play a deterministic PCM output test pattern
  midi - Standard MIDI file path

Runtime:
  launch: foreground-task
  stack: 8192 bytes
  priority: 1
  memory: internal-only
  interface: console
```

## Доступность

Доступна в стандартной или full-сборке в зависимости от feature-флагов платы. Документ соответствует LexxOS 0.0.7.
