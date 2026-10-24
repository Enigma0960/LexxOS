# `sd-spi-mode`

show or set SD SPI clock mode.

## Справка shell

```text
sd-spi-mode - show or set SD SPI clock mode

Shows or changes the RP2040 SPI clock mode used for the SD card. The stable profile uses mode 0; modes 1..3 are disabled unless the source is rebuilt for SD speed experiments.

Usage:
  sd-spi-mode [0|1|2|3]

Arguments:
  mode - SPI clock mode 0..3; without argument prints current mode
```

## Доступность

Доступна в стандартной или full-сборке в зависимости от feature-флагов платы. Документ соответствует LexxOS 0.0.7.
