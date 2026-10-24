# `sd-profile`

apply known SD SPI profile.

## Справка shell

```text
sd-profile - apply known SD SPI profile

Applies a known SD SPI profile. default uses the stable 50MHz CMD18 read baseline and keeps CMD25 disabled; performance uses 60MHz for explicit tests.

Usage:
  sd-profile [safe|default|performance|fast-read]

Arguments:
  profile - profile to apply: safe=1MHz, default=50MHz CMD18 read, performance=60MHz
```

## Доступность

Доступна в стандартной или full-сборке в зависимости от feature-флагов платы. Документ соответствует LexxOS 0.0.7.
