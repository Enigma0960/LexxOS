# Цифровой вход CE

Один объект `lexxos::devices::DigitalInput` обслуживает один GPIO. Приложение выбирает вывод, подтяжку и активный
уровень. Оно может читать подтверждённый уровень через `high()`, состояние нажатия через `pressed()` и обрабатывать
события в своей задаче. Для обычной кнопки вызывайте `poll()` примерно раз в 5 мс и передавайте монотонное время в
миллисекундах. Все вызовы одного объекта выполняйте из одной задачи. Обработчик вызывается синхронно. Вызывайте `poll()`
и `initialize()` только вне обработчика. Таблица GPIO-служб должна жить дольше объекта входа.

| Событие                      | Когда приходит при настройках по умолчанию                                                                         |
|------------------------------|--------------------------------------------------------------------------------------------------------------------|
| `Changed`                    | При каждом изменении физического уровня, сохраняющемся 25 мс. Аргумент `high` содержит новый уровень.              |
| `BecameHigh`, `BecameLow`    | Сразу после `Changed` приходит одно из этих событий по направлению перехода.                                       |
| `SingleClick`, `DoubleClick` | Один раз через 300 мс после последнего отпускания, если за это время не началось следующее подтверждённое нажатие. |
| `TripleClick`                | При третьем подтверждённом отпускании. Следующее нажатие начинает новую серию.                                     |
| `Hold`                       | Один раз через 1500 мс после начала непрерывного нажатия.                                                          |
| `LongHold`                   | Ещё один раз на отметке 10 000 мс того же нажатия.                                                                 |

Удержание завершает предшествующую серию коротких кликов. Отпускание после удержания даёт только события уровня. При
инициализации считывается исходный уровень. Если кнопка уже зажата, распознавание жестов начнётся после первого
отпускания. Настройки времени в `DigitalInputConfig` задаются в миллисекундах. При ошибке чтения `poll()` возвращает
`false`; повторная инициализация начинает наблюдение с текущего уровня.

## Пример обработчика для ESP32

В примере внешняя кнопка замыкает GPIO27 на GND, используется внутренняя подтяжка вверх. Вызовите `runButton` из задачи
приложения, передав ей платформенные службы устройства.

```cpp
#include <lexxos/devices/input/digital_input.h>

using lexxos::devices::DigitalInput;
using lexxos::devices::DigitalInputEvent;
using lexxos::platform::PlatformServices;

void onInput(DigitalInputEvent event, bool high, void* context) {
    auto& platform = *static_cast<PlatformServices const*>(context);
    auto write = platform.console.write;
    switch (event) {
        case DigitalInputEvent::Changed:     write(high ? "input=1\r\n" : "input=0\r\n"); break;
        case DigitalInputEvent::BecameHigh:  write("rising\r\n"); break;
        case DigitalInputEvent::BecameLow:   write("falling\r\n"); break;
        case DigitalInputEvent::SingleClick: write("single\r\n"); break;
        case DigitalInputEvent::DoubleClick: write("double\r\n"); break;
        case DigitalInputEvent::TripleClick: write("triple\r\n"); break;
        case DigitalInputEvent::Hold:        write("held\r\n"); break;
        case DigitalInputEvent::LongHold:    write("long hold\r\n"); break;
    }
}

void runButton(PlatformServices platform) {
    DigitalInput input({.pin = {27}, .pull = lexxos::platform::GpioPull::Up}, platform.gpio);
    if (!input.initialize(onInput, &platform)) {
        platform.console.write("GPIO initialization failed\r\n");
        return;
    }
    while (input.poll(platform.clock.uptimeMs())) {
        platform.clock.sleepMs(5);
    }
    platform.console.write("GPIO read failed\r\n");
}
```
