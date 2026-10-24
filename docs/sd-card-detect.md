# Подключение SD card-detect

LexxOS поддерживает необязательный GPIO card-detect в board descriptor. Текущий профиль `dev_pico` использует GP22 как
active-low вход card-detect. Другие board profiles могут держать этот pin отключённым, пока wiring не будет проверен на
конкретной плате.

Многие дешёвые Arduino microSD modules выводят только `GND`, `VCC`, `MISO`, `MOSI`, `SCK` и `CS`. В этом случае
card-detect может потребовать пайки непосредственно к контактам механического switch внутри microSD socket.

## Рекомендуемая электрическая схема

Используйте механический card-detect switch из microSD socket, а не обычные SD data pins. Самая простая и безопасная
схема для LexxOS — active-low:

```text
Pico GPIO input ---- card-detect switch ---- GND
```

Когда `insertedLevel = GpioActiveLevel::Low`, firmware включает internal pull-up на Pico. Для bring-up внешний резистор
не обязателен. Для финальной платы всё равно рекомендуется внешний pull-up примерно 10 кОм к 3.3 В.

Ожидаемые уровни:

```text
карта не установлена: GPIO = 1
карта установлена:    GPIO = 0
```

Если конкретный socket подключён как active-high, нужно задать:

```cpp
.insertedLevel = GpioActiveLevel::High,
```

В этом случае platform backend должен использовать internal pull-down.

## Pinout development board

Текущий `dev_pico` profile использует следующие pins:

```text
UART0: TX=GP0 RX=GP1
PSRAM: GP9..GP14 reserved
SD:    MISO=GP16 CS=GP17 SCK=GP18 MOSI=GP19
OLED:  SDA=GP20 SCL=GP21
SD CD: GP22 active-low
LED:   GP25
```

Настройка находится в `src/lexxos/platform/pico/board_dev_pico.cpp`:

```cpp
.sdCardDetect = {
    .detect = {22},
    .insertedLevel = GpioActiveLevel::Low,
},
```

## Примечание по Arduino microSD modules

Если на PCB нет pad/header с именем `CD`, `DET`, `CARD_DETECT` или `SW`, card-detect, скорее всего, не выведен наружу.
В этом случае нужно паяться к контактам switch на самом microSD socket.

Нельзя подключать card-detect GPIO к следующим линиям:

```text
DAT1
DAT2
CLK
CMD
VDD
VSS
MISO
MOSI
SCK
CS
```

Card-detect должен быть отдельным switch contact, а не частью SPI/SDIO data bus.

## Текущее runtime-поведение

LexxOS больше не использует отдельную задачу `sd-monitor`. События card-detect проходят через общий системный сервис:

```text
GPIO IRQ -> lifecycle event -> system-events task -> debounce -> SdAutomountService
```

Interrupt handler только публикует компактное `SdCardDetectChanged` событие в `lifecycle::EventBus` и ставит atomic
fallback-флаг. Он не выполняет SPI/FAT/VFS операции, не печатает в UART и не делает debounce. Вся тяжёлая обработка
выполняется позже в обычном task context внутри `system-events`.

Если platform backend не поддерживает GPIO interrupt, `SystemEventService` использует прежний polling fallback. Это
нужно
для host-сборки и будущих платформ, где card-detect может быть не подключён.

При стабильной установке карты ожидаемый вывод:

```text
sd card: inserted
sd automount: probing (inserted)
sd automount: mounted /dev/sd0 on /mnt/sd type fat
```

При извлечении карты service снимает VFS mount point. Если карта физически уже недоступна, `sync` может завершиться
ошибкой, но mount point всё равно принудительно очищается как hot-remove case. Поэтому сообщение использует
`force-unmounted`, чтобы отличать физическое удаление карты от обычной команды `umount`:

```text
sd card: removed
sd automount: force-unmounted /mnt/sd (card removed, sync failed)
```

После удаления `fs-info` должен показывать `filesystem mounts: 0/4`, а после повторной установки `/mnt/sd` должен снова
становиться `ready,rw`.

Ручные команды остаются доступными:

```text
storage
storage diag
mount /dev/sd0 /mnt/sd
umount /mnt/sd
```

## Правила для interrupt handler

В interrupt handler нельзя выполнять:

- SPI операции;
- FAT/VFS mount или unmount;
- выделение памяти;
- длинный debounce;
- вывод в UART/shell;
- операции, которые могут блокироваться.

Корректная модель — короткий ISR, установка флага/события и дальнейшая обработка в обычном task context.
