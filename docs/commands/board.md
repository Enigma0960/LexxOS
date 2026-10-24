# `board`

show active board descriptor.

## Вывод

Команда печатает board identity, UART/I2C/SPI/SD wiring, PSRAM pins и P2 capabilities:

```text
uart: index=<n|-> tx=<pin|-> rx=<pin|-> baud=<baud>
i2c: index=<n|-> sda=<pin|-> scl=<pin|-> baud=<baud>
sd/spi: index=<n|-> sck=<pin|-> mosi=<pin|-> miso=<pin|-> cs=<pin|-> cd=<pin|-> cd-active=<low|high> baud=<baud>
sd/sdio: index=<n|-> clk=<pin|-> cmd=<pin|-> d0=<pin|-> d1=<pin|-> d2=<pin|-> d3=<pin|-> init-hz=<hz> xfer-hz=<hz> pwr-ldo=<n|->
display/spi: enabled=yes/no index=<n|-> sck=<pin|-> mosi=<pin|-> cs=<pin|-> dc=<pin|-> reset=<pin|-> baud=<baud>
display/dsi: enabled=yes/no name=<name> panel=<kind> size=<width>x<height> lanes=<n> lane-mbps=<mbps> dpi-khz=<khz> fb=<rgb565|rgb888> wire=<rgb565|rgb888> order=<rgb|bgr> ldo=<n|->
psram: cs=<pin|-> sck=<pin|-> mosi=<pin|-> miso=<pin|-> sio2=<pin|-> sio3=<pin|-> baud=<baud>
audio/beep: enabled=yes/no left=<pin|-> right=<pin|-> freq=<hz> duration-ms=<ms>
wireless: radio=<name> wifi=yes/no bluetooth=yes/no if=<name> backend=<backend> autoconnect=yes/no scan=yes/no
wifi/hosted-sdio: index=<n|-> clk=<pin|-> cmd=<pin|-> d0=<pin|-> d1=<pin|-> d2=<pin|-> d3=<pin|-> reset=<pin|-> init-hz=<hz> xfer-hz=<hz>
hosted: host-version=<x.y.z> coprocessor-version=<x.y.z|-> version-error=<err> compatible=<yes|no|unknown> app-desc=<yes|no> app-error=<err>
hosted/app: name=<project> version=<version> idf=<idf-version> build=<date> <time>
ethernet: enabled=yes/no phy=<kind> name=<name> ...
rtc/software: enabled=yes/no default=yes/no auto-sync=yes/no server=<host> ...
capabilities: <capability> [<capability> ...]
```

На платах, где board profile использует общие `interfaces.*` fallback-поля, команда печатает effective wiring, а не
только literal legacy-поля descriptor.

`capabilities` показывает runtime snapshot из `OperatingSystem`: например `display.rgb565`, `storage.block`,
`input.keyboard`, `flash.named-regions`, `audio.tone`, `audio.pcm`, `wireless.wifi` и другие contracts, которые реально доступны
текущему OS instance после регистрации устройств.

На ESP-Hosted платах `board` не стартует Wi-Fi backend принудительно. Если transport ещё не поднят, строка `hosted`
покажет host component version и `ESP_ERR_INVALID_STATE` для coprocessor RPC-полей. Для активной диагностики используйте
`hosted-info`, который по умолчанию сам запускает ESP-Hosted backend.

`wifi/hosted-sdio` печатается, когда board descriptor содержит SDIO-разводку Wi-Fi coprocessor-а. Для
`jc_esp32p4_m3_dev` microSD использует SDMMC Slot 0, а ESP32-C6/ESP-Hosted использует Slot 1.

## Справка shell

```text
board - show active board descriptor
```

## Доступность

Доступна в стандартной или full-сборке в зависимости от feature-флагов платы. Документ соответствует LexxOS 0.0.7.
