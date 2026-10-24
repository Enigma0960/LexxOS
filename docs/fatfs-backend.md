# Backend FatFs

LexxOS использует FatFs как единственный backend файловой системы FAT. Предыдущая экспериментальная реализация LexxOS
удалена: все runtime paths, automount и shell-команды работают через FatFs adapter.

## Структура third_party

CMake ожидает официальный архив ChaN FatFs в следующей структуре:

```text
third_party/fatfs/
    source/
        ff.c
        ff.h
        ffunicode.c
        ffsystem.c
        diskio.h
        ffconf.h
        integer.h
```

Установка:

```bash
mkdir -p third_party/fatfs
curl -L -o /tmp/fatfs-r0.16.zip https://elm-chan.org/fsw/ff/arc/ff16.zip
unzip /tmp/fatfs-r0.16.zip -d third_party/fatfs
```

Для другого расположения укажите:

```bash
cmake -S . -B build/host \
  -DLEXXOS_FATFS_SOURCE_DIR=/absolute/path/to/fatfs/source
```

## Переключатели

Backend включается одним feature flag:

```text
LEXXOS_FEATURE_FATFS=ON
```

## Настройки ffconf.h

Для первого аппаратного прогона используются консервативные настройки:

```c
#define FF_FS_READONLY    0
#define FF_FS_MINIMIZE    0
#define FF_USE_FIND       0
#define FF_USE_MKFS       0
#define FF_USE_FASTSEEK   0
#define FF_USE_LFN        2
#define FF_MAX_LFN        64
#define FF_LFN_UNICODE    0
#define FF_CODE_PAGE      437
#define FF_FS_RPATH       0
#define FF_MIN_SS         512
#define FF_MAX_SS         512
#define FF_FS_EXFAT       0
#define FF_FS_REENTRANT   0
#define FF_FS_TINY        0
```

Текущий адаптер требует `FF_LFN_UNICODE=0`; это дополнительно проверяется на этапе компиляции.

## Интеграция с runtime

Соответствие физических дисков FatFs экземплярам `BlockDevice` реализовано в
`src/lexxos/fs/fat/fatfs/fatfs_diskio.cpp`. VFS-адаптер находится в
`src/lexxos/fs/fat/fat_volume.cpp`.

Публичный API совместим с прежним слоем монтирования:

```cpp
lexxos::fs::fat::mount(...)
lexxos::fs::fat::unmount(...)
lexxos::fs::fat::forceUnmount(...)
lexxos::fs::fat::isMounted(...)
lexxos::fs::fat::visitMountedVolumes(...)
```

Благодаря этому `sd_automount_service`, команды `mount`/`umount` и общие VFS-команды зависят от публичного FAT/VFS API,
а не от деталей FatFs.

## Быстрая проверка

В full-сборке доступна команда `fs-smoke [path]`. По умолчанию она проверяет `/mnt/sd`: создаёт временный каталог,
проверяет длинные имена, запись, добавление, чтение, копирование, переименование, `stat`, удаление и `sync`.

Рекомендуемый запуск:

```text
fs-info
fs-smoke /mnt/sd
ls /mnt/sd
sync
```
