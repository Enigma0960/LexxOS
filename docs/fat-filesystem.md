# FAT filesystem в LexxOS

Документ фиксирует текущий статус FAT-подсистемы после перехода на FatFs как единственный backend.

## Статус

Текущий filesystem layer:

```text
FatFs-backed FAT read/write + ASCII LFN
```

Этого достаточно для нормального embedded workflow на SD-карте:

- автоматическое обнаружение SD-карты и FAT mount при boot/card insertion;
- ручные `mount`, `umount`, `sync`;
- file read/write/append/truncate через VFS;
- создание и удаление каталогов;
- rename/move для файлов и каталогов;
- ASCII long file names для файлов и каталогов;
- cache/storage диагностика через `fs-info`, `fat-cache`, `sd-debug`, `sd-test`, `sd-bench` и `sd-stress`.

## Поддерживаемые операции

Минимальный поддерживаемый набор:

```text
ls /mnt/sd
stat /mnt/sd/file.txt
cat /mnt/sd/file.txt
touch /mnt/sd/file.txt
write /mnt/sd/file.txt text
append /mnt/sd/file.txt text
cp /mnt/sd/file.txt /mnt/sd/copy.txt
rm /mnt/sd/file.txt
mkdir /mnt/sd/dir
rmdir /mnt/sd/dir
mv /mnt/sd/source.txt /mnt/sd/target.txt
rename /mnt/sd/source.txt /mnt/sd/target.txt
sync
umount /mnt/sd
```

Те же операции должны работать с ASCII LFN paths:

```text
write "/mnt/sd/Long File Name.txt" hello
mkdir "/mnt/sd/Long Directory Name"
mv "/mnt/sd/Long File Name.txt" "/mnt/sd/Long Directory Name/New Long File.txt"
cat "/mnt/sd/Long Directory Name/New Long File.txt"
```

## Диагностика

Актуальные диагностические команды не зависят от backend-private FAT internals:

```text
fs-info
fat-cache
sd-debug
sd-test
sd-bench
sd-stress
fs-smoke
```

`fs-info` показывает активные VFS mounts и mounted FAT volumes. `fat-cache` показывает counters FatFs disk cache.
`sd-debug` выводит диагностический буфер SD driver. `sd-test`, `sd-bench`, `sd-stress` и `fs-smoke` проверяют FAT path
через публичные VFS операции.

## Автоматическое монтирование

На профиле `dev_pico` card-detect подключён к GP22 active-low. При boot или установке карты LexxOS выполняет probe и
пытается смонтировать FAT volume:

```text
sd automount: probing (boot)
sd automount: mounted /dev/sd0 on /mnt/sd type fat
```

При установке карты после загрузки ожидаемый вывод:

```text
sd card: inserted
sd automount: probing (inserted)
sd automount: mounted /dev/sd0 on /mnt/sd type fat
```

При извлечении карты текущий monitor снимает mount point:

```text
sd card: removed
sd automount: unmounted /mnt/sd (removed)
```

Manual workflow остаётся доступным:

```text
storage probe
mount /dev/sd0 /mnt/sd
umount /mnt/sd
```

## Текущие ограничения

Ограничения этапа намеренные:

- основной board-tested target — FAT32 с 512-байтными секторами;
- ASCII long file names поддерживаются, non-ASCII UTF-16 LFN пока не является целевым сценарием;
- timestamps пока не записываются;
- recursive delete не реализован;
- permissions, owners и executable bits находятся вне текущей VFS-модели;
- repair/fsck mode отсутствует после удаления legacy diagnostic backend; проверка целостности должна выполняться
  внешними host-инструментами на вынутой SD-карте.

## Правила безопасности для дальнейшей разработки

При развитии FAT layer нужно сохранять следующие правила:

1. Каждый шаг должен быть board-testable и обратимым.
2. Write-back caching нельзя добавлять до отдельного тестирования `sync` и failure behavior.
3. Repair operations должны быть отделены от runtime path и не смешиваться с обычными VFS-командами.
4. Card removal и transient SPI/SDIO read errors нужно считать нормальными runtime events.
5. VFS mount/unmount и FAT write operations нельзя выполнять из interrupt handler.
6. После каждого изменения FAT/VFS нужно прогонять `docs/fat-test-plan.md` на disposable SD card или карте с резервной
   копией.
