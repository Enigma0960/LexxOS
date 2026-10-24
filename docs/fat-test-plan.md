# Регрессионный тест-план FatFs/VFS

Этот checklist должен проходить после изменений в SD, VFS, FatFs adapter, automount или shell file commands.

Запускайте тест на disposable SD card или на карте с резервной копией. Тест создаёт, перемещает и удаляет файлы и
каталоги.

## 1. Монтирование и базовая диагностика

```text
version
mount
fs-info
fat-cache
sd-debug
ls /mnt/sd
```

Ожидаемые признаки:

```text
/mnt/sd type fat ready,rw
fat volumes:
sd-debug: diagnostics=on
```

## 2. Операции с файлами в short-name формате

```text
write /mnt/sd/A.TXT hello short
cat /mnt/sd/A.TXT
append /mnt/sd/A.TXT second line
cat /mnt/sd/A.TXT
stat /mnt/sd/A.TXT
mv /mnt/sd/A.TXT /mnt/sd/B.TXT
cat /mnt/sd/B.TXT
rm /mnt/sd/B.TXT
sync
fat-cache
```

## 3. Операции с каталогами в short-name формате

```text
mkdir /mnt/sd/DIR
write /mnt/sd/DIR/A.TXT hello dir
cat /mnt/sd/DIR/A.TXT
mv /mnt/sd/DIR/A.TXT /mnt/sd/A.TXT
cat /mnt/sd/A.TXT
mv /mnt/sd/A.TXT /mnt/sd/DIR/B.TXT
cat /mnt/sd/DIR/B.TXT
rm /mnt/sd/DIR/B.TXT
rmdir /mnt/sd/DIR
sync
```

## 4. Перемещение каталога

```text
mkdir /mnt/sd/A
mkdir /mnt/sd/B
write /mnt/sd/A/FILE.TXT hello move
mv /mnt/sd/A /mnt/sd/B/A
cat /mnt/sd/B/A/FILE.TXT
mv /mnt/sd/B/A /mnt/sd/A
cat /mnt/sd/A/FILE.TXT
rm /mnt/sd/A/FILE.TXT
rmdir /mnt/sd/A
rmdir /mnt/sd/B
sync
```

## 5. Операции с LFN-файлами

```text
write "/mnt/sd/Created Long File.txt" hello lfn
cat "/mnt/sd/Created Long File.txt"
append "/mnt/sd/Created Long File.txt" second line
cat "/mnt/sd/Created Long File.txt"
stat "/mnt/sd/Created Long File.txt"
mv "/mnt/sd/Created Long File.txt" "/mnt/sd/Renamed Long File.txt"
cat "/mnt/sd/Renamed Long File.txt"
rm "/mnt/sd/Renamed Long File.txt"
sync
```

## 6. Операции с LFN-каталогами

```text
mkdir "/mnt/sd/Long Directory Name"
write "/mnt/sd/Long Directory Name/File In Long Dir.txt" hello
cat "/mnt/sd/Long Directory Name/File In Long Dir.txt"
stat "/mnt/sd/Long Directory Name/File In Long Dir.txt"
rmdir "/mnt/sd/Long Directory Name"
rm "/mnt/sd/Long Directory Name/File In Long Dir.txt"
rmdir "/mnt/sd/Long Directory Name"
sync
```

Первый `rmdir` должен завершиться ошибкой `directory not empty`.

## 7. LFN move и rename

```text
write "/mnt/sd/Old Long File.txt" hello rename lfn
mv "/mnt/sd/Old Long File.txt" "/mnt/sd/New Long File.txt"
cat "/mnt/sd/New Long File.txt"
mkdir "/mnt/sd/Long Directory Name"
mv "/mnt/sd/New Long File.txt" "/mnt/sd/Long Directory Name/New Long File.txt"
cat "/mnt/sd/Long Directory Name/New Long File.txt"
mv "/mnt/sd/Long Directory Name" "/mnt/sd/Renamed Long Directory"
cat "/mnt/sd/Renamed Long Directory/New Long File.txt"
rm "/mnt/sd/Renamed Long Directory/New Long File.txt"
rmdir "/mnt/sd/Renamed Long Directory"
sync
```

## 8. Финальная проверка и reboot

```text
sync
reboot
ls /mnt/sd
fs-info
fat-cache
sd-debug
```

Карта должна содержать только файлы, существовавшие до теста.

## 9. Проверка извлечения и повторной установки карты

Этот блок нужен после изменений в card-detect, automount или SD driver.

```text
mount
ls /mnt/sd
```

Затем извлеките карту. Ожидаемый вывод:

```text
sd card: removed
sd automount: unmounted /mnt/sd (removed)
```

После повторной установки карты ожидаемый вывод:

```text
sd card: inserted
sd automount: probing (inserted)
sd automount: mounted /dev/sd0 on /mnt/sd type fat
```

Финальная проверка:

```text
mount
ls /mnt/sd
fs-info
```

## 10. SD/FAT performance regression для production-профиля

Этот блок нужен после изменений в SD backend, FAT cache, `FileHandle`, `FatVolume`, `diskio`, CMake-профилях платы или
SD board descriptor.

Перед тестом убедитесь, что используется стабильный SPI production-профиль:

```text
storage
sd-profile
fat-cache
```

Короткий тест:

```text
fat-cache clear
sd-debug clear
sd-bench 128
fat-cache
sd-debug
```

Длинный RW-тест:

```text
fat-cache clear
sd-debug clear
sd-speed-scan max=60000000 step=1000000 blocks=256 files=16 scope=rw batch=2 dma=on fast-write=on
fat-cache
sd-debug
```
