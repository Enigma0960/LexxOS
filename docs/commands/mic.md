# `mic`

show or test the primary microphone.

## Примеры

```text
lexx:/> mic info
lexx:/> mic sample
lexx:/> mic loopback -s 1 -g 2
```

## Cardputer-Adv

Cardputer-Adv exposes ES8311/I2S capture as `cardputer-adv-mic`. `mic sample` captures one 512-frame PCM chunk and
prints peak/average levels. `mic loopback` captures microphone PCM and writes it to the primary `AudioDevice`, so it is
a short audible wiring test rather than a recorder.

The loopback path is intentionally cancellable and uses a 10 KiB foreground task stack because it keeps a 512-frame
stereo PCM chunk on the task stack while calling both microphone capture and audio output.

## Справка shell

```text
mic - show or test the primary microphone

Shows the registered microphone, captures one PCM chunk, or loops microphone input back to the primary audio output.

Usage:
  mic [info|sample|loopback] [-s seconds] [-g 1..16] [name]

Arguments:
  -s, --seconds <seconds> default=3 - sample or loopback duration
  -g, --gain <1..16> default=4 - digital gain for loopback output
  action - info, sample or loopback
  name - microphone device name

Runtime:
  launch: foreground-task
  stack: 10240 bytes
  priority: 1
  memory: internal-only
  interface: console
```

## Доступность

Доступна в стандартной или full-сборке в зависимости от feature-флагов платы. Документ соответствует LexxOS 0.0.7.
