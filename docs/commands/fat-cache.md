# `fat-cache`

show FAT disk cache statistics.

## Справка shell

```text
fat-cache - show FAT disk cache statistics

Shows the FatFs diskio read-ahead cache window, hit/miss counters and disk_write chunk statistics. Use write-max to cap FAT disk_write chunks while CMD25 remains experimental.

Usage:
  fat-cache [clear|drop|write-max [0|1|2|4|8|16|32|64]]
```

## Доступность

Доступна в стандартной или full-сборке в зависимости от feature-флагов платы. Документ соответствует LexxOS 0.0.7.
