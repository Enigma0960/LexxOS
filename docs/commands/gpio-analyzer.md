# `gpio-analyzer`

Экспериментальная программа: только full на поддерживаемых платах. Интерфейс и поведение могут меняться.

show and control user GPIO pins.

## Справка shell

```text
gpio-analyzer - show and control user GPIO pins

Configures board-approved digital GPIO pins as inputs and outputs, then shows selected inputs as a live logic graph on the display.

Usage:
  gpio-analyzer [-i pins] [-o pin=value,...] [-s milliseconds] [-w milliseconds] [-p board|none|up|down] [display]

Arguments:
  -i, --in <pins> - comma separated input pins or labels, for example GP8,GP9 or 8,9
  -o, --out <assignments> - comma separated output assignments, for example GP12=1,GP28=0
  -s, --sample <milliseconds> default=20 - initial sampling period in milliseconds
  -w, --window <milliseconds> default=5000 - initial time window shown on screen in milliseconds
  -p, --pull <board|none|up|down> default=board - pull mode applied to selected inputs; board uses each pin default
  display - display device name

Runtime:
  launch: foreground-task
  stack: 6144 bytes
  priority: 2
  memory: external-preferred
  interface: graphical
  external quota: 49152 bytes
```

## Доступность

Доступна только в full при `LEXXOS_FEATURE_GPIO_ANALYZER=ON` и поддержке платы.
На PicoCalc Pico W отключена по умолчанию из-за ограничений SRAM. В minimal и standard не поставляется.
