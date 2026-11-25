# Реализация FatFs

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
#define FF_USE_FASTSEEK   0 /* исходный ffconf.h из third_party */
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

Заголовок `third_party/fatfs/source/ffconf.h` **не изменяется**. Для Lyra
`config/fatfs/lexxos_ffconf_override.h` принудительно подключается компилятором
перед любым `ff.h`: сначала читается исходный `ffconf.h`, затем переопределяется
только `FF_USE_FASTSEEK=1`. FatFs использует `FFCONF_DEF` и не перечитывает
исходный заголовок при последующем включении `ff.h`.

Один и тот же overlay должен действовать при компиляции `ff.c` и всех C/C++
модулей, использующих `FIL` (`FF_USE_FASTSEEK` меняет его размер). Поэтому
параметры задаются через `config/cmake/FatFsConfigOverride.cmake` — `PRIVATE`
для прошивки Lyra, которая компилирует `ff.c` непосредственно, и `PUBLIC` для
общего target `fatfs`. При `-DLEXXOS_FATFS_FASTSEEK=OFF` заголовок overlay
не подключается и применяется обычная конфигурация FatFs.

Текущий адаптер требует `FF_LFN_UNICODE=0`; это дополнительно проверяется на этапе компиляции.

## Ускорение больших .lexx на RK3506/Lyra

`LEXXOS_FATFS_FASTSEEK` автоматически включён для `LEXXOS_PLATFORM=rk3506`
и выключен для остальных платформ. Его можно переопределить через CMake:

```bash
cmake --preset picocalc-lyra-release -DLEXXOS_FATFS_FASTSEEK=ON
```

При чтении файла `.lexx` от 4 МиБ FatVolume однократно пытается построить
CLMT (Cluster Link Map Table, максимум 2048 DWORD = 8 КиБ на активный файл).
Таблица ускоряет повторные произвольные `f_lseek()` во время поиска по индексу
LXR2 и чтения ассетов. Первое построение CLMT может занять время, сопоставимое
с полным проходом цепочки кластеров. При нехватке памяти либо чрезмерной
фрагментации используется стандартный FatFs seek; ошибки чтения не возникают.

Один активный `.lexx` удерживается в выделенном read-only FIL, независимо от
обычного кеша чтения: операции с сохранениями не сбрасывают CLMT.
При перезаписи `.lexx`, rename/remove, размонтировании и reset карта освобождается.
Потребление памяти ограничено 8 КиБ плюс один `FIL` и путь на том.

Fast Seek не меняет формат FAT32, `.lexx` или интерфейс VFS. Опция
`FF_USE_FASTSEEK=1` сама по себе не влияет на `FIL` без `cltbl`.

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
