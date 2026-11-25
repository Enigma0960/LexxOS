# `link`

Переносимый ярлык программы LexxOS (`.lnk`), не символическая ссылка POSIX.
Хранится как обычный UTF-8 файл на FAT или в `/sys/autostart`.

```text
link create /mnt/sd/games/snake.lnk /mnt/sd/apps/snake.lexx --fg --delay-ms 500 -- --level 2
link show /mnt/sd/games/snake.lnk
link run /mnt/sd/games/snake.lnk
/mnt/sd/games/snake.lnk
link create /sys/autostart/music.lnk /mnt/sd/apps/music.lexx --bg --delay-ms 1500 -- --repeat
rm /sys/autostart/music.lnk
```

Формат манифеста: по одной строке `arg=` на аргумент; пробелы внутри строки
не разбивают его на несколько аргументов.

```text
LEXX-LINK-1
target=/mnt/sd/apps/music.lexx
mode=background
delay_ms=1500
arg=--repeat
```

Фоновый запуск через ProgramSupervisor подавляет stdout/stderr и требует
разрешения программы на работу в фоне. Интерактивный запуск получает фокус
терминала по правилам ProgramSupervisor. Задержки применяются при загрузочном
автозапуске; ручной `link run` их не использует.

Пределы: 8 аргументов по 95 байт, манифест до 2048 байт, задержка до
86400000 мс. Вложенные `.lnk` запрещены. Для перезаписи явно удалите старый ярлык.
