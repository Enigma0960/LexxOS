# `ir`

Shows or tests the primary IR transmitter.

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

Cardputer-Adv exposes the built-in IR transmitter as `cardputer-ir` on GPIO44 with a 38 kHz carrier. The ESP-IDF
implementation uses RMT with carrier modulation instead of software bit-banging.

Built-in Samsung TV test codes:

```text
samsung-power  0xE0E040BF
samsung-on     0xE0E09966
samsung-off    0xE0E019E6
```

`samsung-power` matches the common Samsung BN59-class power toggle frame used for a quick line-of-sight smoke test.
Serial smoke can only verify that the frame was queued and transmitted; real TV response remains a manual check.
Use `samsung-code -c <hex>` to try another 32-bit Samsung code without rebuilding.

## Доступность

Доступна в Cardputer-сборках LexxOS 0.0.7.
