# SPI в LexxCE

Плата описывает общие линии в `LegacyBoardDescriptor::spiBuses` и отдельные устройства в списке `NamedSpiBus::devices`.
Приложение получает каждый объект через `DeviceManager::spiBus(name)`. Имена устройств уникальны среди всех SPI-шин
платы. Массивы описания и строки имён должны существовать до завершения работы реестра.

```cpp
static constexpr lexxos::platform::SpiDeviceConfig peripherals[]{
    {.name = "display", .cs = {25}, .baudRate = 10000000},
    {.name = "sensor", .cs = {26}, .baudRate = 1000000,
        .mode = lexxos::devices::SpiClockMode::Mode3},
};
static constexpr lexxos::platform::NamedSpiBus buses[]{
    {.name = "peripherals",
        .pins = {.index = 0, .sck = {18}, .mosi = {23}, .miso = {19}},
        .devices = peripherals},
};
// В LegacyBoardDescriptor задайте .spiBuses = buses.
```

Параметр частоты в `SpiDeviceConfig` задаёт частоту устройства. Унаследованное поле `SpiInterfacePins::baudRate` для
этой таблицы игнорируется. На классическом ESP32 индекс 0 соответствует SPI2, индекс 1 — SPI3. Порт принимает частоты от
1 кГц до 40 МГц, фактическая частота определяется делителем контроллера. Передаются байты старшим битом вперёд,
поддерживаются Mode0–Mode3 и активный низкий CS. MOSI или MISO можно отключить значением `-1`.

## Обмен

Для одной операции доступна функция `queuedSpiTransfer`.

```cpp
#include <lexxos/devices/bus/spi_transfer.h>

// system уже успешно прошёл bootstrap.
// command и reply — буферы одинаковой длины, заполненные приложением.
auto* sensor = system.devices().spiBus("sensor");
if (sensor) {
    auto result = lexxos::devices::queuedSpiTransfer(
        *sensor, lexxos::devices::SpiClockMode::Mode3, 1000000,
        command, reply, sizeof(command));
    // result — Ok, Busy или IoError.
}
```

Нулевой указатель `rx` выбирает запись, нулевой `tx` — чтение с заполнением `0xFF`. При обмене нулевой длины данные не
передаются. Для последовательности «команда, затем ответ» используйте одну транзакцию и один интервал CS.

```cpp
lexxos::devices::SpiBusTransaction transaction{*sensor};
if (transaction) {
    sensor->select();
    bool ok = sensor->write(command, sizeof(command));
    if (ok) {
        ok = sensor->read(0xFF, reply, sizeof(reply));
    }
    sensor->deselect();
    // Используйте reply только при ok == true.
}
```

В CE методы `write/read/transfer` требуют успешного `select()` для непустого обмена. Каждому `select()` соответствует
`deselect()` в той же задаче. Транзакцию и выбранный CS нужно освободить до обращения к другому устройству. Изменение
режима или частоты выполняется до `select()`. Если изменение конфигурации не удалось, прежняя конфигурация остаётся
рабочей. Для смены параметров ESP-IDF временно выделяет дополнительный handle устройства, поэтому при исчерпании слотов
операция может вернуть ошибку.

Устройства одного контроллера используют общую очередь и общий mutex. Порт удерживает блокировку на всём интервале CS,
включая несколько вызовов передачи. Длинный обмен выполняется блоками по 64 байта с сохранением активного CS. Между
блоками возможны паузы тактирования. Первая реализация выполняет синхронный обмен, `supportsDma()` возвращает
`false`. [Документация SPI Master Espressif](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/spi_master.html).

## Жизненный цикл и сборка

При bootstrap проверяются повторяющиеся имена, индексы, CS и пересечения SPI с объявленными GPIO, UART и I²C. При отказе
инициализации созданные устройства удаляются до освобождения контроллеров. Сброс реестра выполняйте после завершения
рабочих задач и освобождения транзакций.

В адаптере ESP-IDF требуется зависимость `esp_driver_spi`. Используйте `.a` и весь каталог `include/` из одного SDK.
Публичные интерфейсы SPI экспортируются вместе с остальными заголовками. В поставке есть автономный пример
`examples/spi_devices`.
