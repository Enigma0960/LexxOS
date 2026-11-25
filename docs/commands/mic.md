# `mic`

Показывает или проверяет основной микрофон.

## Примеры

```text
lexx:/> mic info
lexx:/> mic sample
lexx:/> mic loopback -s 1 -g 2
```

## Cardputer-Adv

Cardputer-Adv регистрирует захват ES8311/I2S как `cardputer-adv-mic`.
`mic sample` получает 512 PCM-кадров и печатает пиковый/средний уровень.
`mic loopback` сразу передаёт PCM основному `AudioDevice` для короткой
звуковой проверки подключения.

Проверку можно отменить. Она использует стек интерактивной задачи 10 КиБ:
512 стереокадров PCM остаются на стеке при вызовах захвата и воспроизведения.

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
