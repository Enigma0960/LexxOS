# `imu`

Reads samples from the primary motion sensor.

## Справка shell

```text
imu - read the primary motion sensor

Reads samples from a registered MotionSensor, such as the Cardputer-Adv BMI270.

Usage:
  imu [-n samples] [-i milliseconds] [name]

Arguments:
  -n, --samples <count> - sample count
  -i, --interval <milliseconds> - delay between samples
  name - sensor device name
```

## Примеры

```text
lexx:/> imu
lexx:/> imu -n 8 -i 100
lexx:/> imu cardputer-bmi270
```

## Cardputer-Adv

На M5Stack Cardputer-Adv команда читает BMI270 на I2C address `0x69`. Инициализация загружает Bosch BMI270 config file,
после чего включает accel/gyro/temperature sampling. Сырые значения печатаются без пересчета в g/dps, чтобы команда
оставалась простой bring-up диагностикой.

## Доступность

Доступна в Cardputer-сборках LexxOS 0.0.7.
