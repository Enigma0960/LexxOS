# `sd-baud`

show or set SD SPI baud rate.

## Справка shell

```text
sd-baud - show or set SD SPI baud rate

Shows the current SD SPI baud rate or sets a new transfer rate/profile after initialization.

Usage:
  sd-baud [safe|default|performance|hz]

Arguments:
  hz - SPI baud rate/profile to set: safe, default, performance or Hz; without argument prints active rate
```

## Доступность

Доступна в стандартной или full-сборке в зависимости от feature-флагов платы. Документ соответствует LexxOS 0.0.7.
