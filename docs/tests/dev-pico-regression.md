# Dev Pico regression checklist

Этот список предназначен для ручной регрессии полной сборки `dev_pico` перед слиянием крупных изменений.
Пока это базовый профиль dev платы; отдельные платы вроде Pico 2W, PicoCalc, STM32F407 и ESP32/M5Stack должны получить
свои
варианты после стабилизации board manifest.

## 1. Сборка и host checks

Выполнить на рабочей машине:

```bash
cmake --build --preset debug-host-full
ctest --test-dir build/host-full --output-on-failure
ctest --test-dir build/host-checks --output-on-failure
cmake --build --preset lexxos-sim-full
PYTHONPATH=emulator python3 -m pytest emulator/tests --lexxos-sim-lib build/sim-full/src/liblexxos_sim.dylib
cmake --build --preset lexxos-pico-full
```

Ожидаемо:

- host contract tests проходят без ошибок;
- emulator pytest проходит, допускается только явно помеченный `skipped`;
- `lexxos-pico-full` генерирует `lexxos.elf` и UF2/образ без ошибок компиляции.

## 2. Первый boot и shell

Прошить dev Pico и открыть serial monitor. Проверить:

```text
version
board
help
pwd
uptime
tasks
mem
```

Ожидаемо:

- shell печатает prompt и не теряет ввод;
- `version` показывает `0.0.5`, platform `pico` и board `dev_pico`;
- `board` показывает корректные GPIO/I2C/SPI/SD/display/input настройки;
- `tasks` показывает shell/system tasks без падений;
- `mem` показывает ненулевой heap snapshot на FreeRTOS сборке.

## 3. Ctrl+C и запуск программ

Проверить отмену долгих команд:

```text
keyboard-live -s 60
```

Нажать `Ctrl+C`.

Ожидаемо:

- программа завершается и shell возвращает prompt;
- повторный ввод команд работает без reset платы;
- после отмены `tasks` не показывает зависшую program task.

## 4. VFS и tmpfs

Проверить базовые файловые операции:

```text
tmp-info
touch /tmp/a.txt
write /tmp/a.txt hello
cat /tmp/a.txt
append /tmp/a.txt world
cat /tmp/a.txt
cp /tmp/a.txt /tmp/b.txt
mv /tmp/b.txt /tmp/c.txt
ls /tmp
stat /tmp/c.txt
rm /tmp/a.txt
rm /tmp/c.txt
tmp-info
```

Ожидаемо:

- `cat` показывает записанный текст;
- `ls /tmp` отражает создание/перемещение файлов;
- `stat` показывает regular file и корректный размер;
- после удаления `tmp-info` возвращается к свободному состоянию.

## 5. Storage и SD card

Вставить тестовую SD карту. Проверить:

```text
storage
block-devs
sd-test
sd-bench
sd-stress -n 8
sd-speed-scan --files 2 --blocks 8
mount sd0 /mnt/sd
ls /mnt/sd
fs-info
fs-smoke /mnt/sd
sync
umount /mnt/sd
```

Ожидаемо:

- `storage` видит `sd0`, карта ready, block size 512;
- `block-devs` показывает `sd0` как free/ready до запуска тестов;
- SD команды не оставляют block device в состоянии busy;
- `fs-smoke /mnt/sd` проходит без ошибок;
- после `umount` повторный `mount sd0 /mnt/sd` работает.

## 6. Display

Проверить дисплей:

```text
display-info
display-fill red
display-fill green
display-fill blue
display-bars
display-text LexxOS
display-test
```

Ожидаемо:

- цвета соответствуют командам и экран полностью обновляется;
- текст читаемый, без инверсии/сдвига;
- после `display-test` shell остается responsive.

## 7. Input, keyboard and gamepad

Проверить основное устройство ввода:

```text
devices
input-test
keyboard-raw -s 10
keyboard-live -s 10
gamepad-test
gamepad-live -s 10
```

Ожидаемо:

- `devices` показывает зарегистрированные input/gamepad devices;
- нажатия клавиш отображаются один раз после debounce;
- направления и кнопки gamepad соответствуют физической раскладке;
- `Ctrl+C` завершает live-команды.

## 8. I2C and shared-device access

Проверить I2C и отсутствие зависаний при повторном доступе:

```text
i2c-buses
i2c-scan
display-test
keyboard-live -s 5
i2c-scan
```

Ожидаемо:

- `i2c-scan` не ломает display/input;
- повторный доступ к устройствам не приводит к assert, hard fault или зависанию;
- после каждой команды shell возвращает prompt.

## 9. Game Boy smoke

Положить тестовые ROM на SD карту и проверить bounded runs:

```text
gb-info /mnt/sd/gb/test-rom-1.gb
gb-rom-check /mnt/sd/gb/test-rom-1.gb
gb-cart /mnt/sd/gb/test-rom-1.gb
gb-test-run /mnt/sd/gb/test-rom-1.gb --frames 2
gbemu /mnt/sd/gb/test-rom-1.gb --frames 2 --display crop
```

Ожидаемо:

- заголовок ROM валиден;
- bounded Game Boy команды завершаются и возвращают prompt;
- после `gbemu` команды `mem`, `tasks`, `storage` продолжают работать;
- повторный запуск `gbemu` не требует reboot.

## 10. Reboot and recovery

Завершить прогон:

```text
sync
reboot
```

Ожидаемо:

- перед reboot нет зависших storage/display/input locks;
- плата перезагружается в shell;
- после reboot `version`, `storage`, `display-info` снова работают.
