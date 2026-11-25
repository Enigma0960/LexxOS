# `ir`

Показывает или проверяет основной ИК-передатчик.

## Справка shell

```text
ir - show or test the primary IR transmitter

Shows the registered IR transmitter or sends the Cardputer-Adv Samsung TV power test frame.

Usage:
  ir [info|samsung-power|samsung-on|samsung-off|samsung-code] [-r count] [-c hex] [name]

Arguments:
  -r, --repeat <count> - number of frames to send
  -c, --code <hex> - Samsung 32-bit code for samsung-code, default E0E040BF
  action - info, samsung-power, samsung-on, samsung-off or samsung-code
  name - IR transmitter device name
```

## Примеры

```text
lexx:/> ir info
lexx:/> ir samsung-power
lexx:/> ir samsung-power -r 3
lexx:/> ir samsung-on
lexx:/> ir samsung-off
lexx:/> ir samsung-code -c E0E040BF
```

## Cardputer-Adv

Cardputer-Adv регистрирует встроенный ИК-передатчик как `cardputer-ir`
на GPIO44 с несущей 38 кГц. ESP-IDF использует RMT с модуляцией несущей.

Встроенные тестовые коды телевизора Samsung:

```text
samsung-power  0xE0E040BF
samsung-on     0xE0E09966
samsung-off    0xE0E019E6
```

`samsung-power` соответствует распространённому коду переключения питания
Samsung BN59. Проверка через UART подтверждает отправку кадра; реакция
телевизора проверяется вручную при прямой видимости. Для другого 32-битного
кода без пересборки используйте `samsung-code -c <hex>`.

## Доступность

Доступна в Cardputer-сборках LexxOS 0.0.7.
