# Драйверы GPIO в описании платы

В `LegacyBoardDescriptor::userGpio` можно назначить выводу драйвер `DigitalInput` или `DigitalOutput`. Метки и номера
GPIO принадлежат плате. Приложение обращается к устройству по метке, а ядро хранит его состояние. Обычная запись пина
без поля `driver` по-прежнему описывает доступный GPIO для ручной настройки.

Например, эта конфигурация задаёт четыре входа с подтяжкой вверх и четыре выхода с начальным низким уровнем. Номера
выводов здесь иллюстрируют разводку конкретного устройства.

```cpp
using Pin = lexxos::platform::BoardUserGpioPin;
using lexxos::platform::GpioPull;

static constexpr lexxos::platform::LegacyBoardDescriptor board{
    .userGpio = {
        .pins = {
            Pin::digitalInput({32}, "in1", GpioPull::Up),
            Pin::digitalInput({33}, "in2", GpioPull::Up),
            Pin::digitalInput({25}, "in3", GpioPull::Up),
            Pin::digitalInput({26}, "in4", GpioPull::Up),
            Pin::digitalOutput({16}, "out1"),
            Pin::digitalOutput({17}, "out2"),
            Pin::digitalOutput({18}, "out3"),
            Pin::digitalOutput({19}, "out4"),
        },
        .count = 8,
    },
};
```

Последний аргумент `digitalOutput(pin, label, initialHigh)` задаёт физический уровень при загрузке. Вход по умолчанию
активен при низком уровне. Для другого поведения укажите `digitalInput(pin, label, pull, activeLevel, timing)`, где
`timing` — указатель на статическую структуру `GpioInputTiming` с порогами дребезга, кликов и удержания. Можно также
заполнить поля `.driver`, `.defaultPull`, `.activeLevel`, `.inputTiming` или `.initialHigh` в исходном агрегате пина.

## Использование в приложении CE

```cpp
// Вызывается из задачи приложения. system — CoreSystem, настроенный на эту плату.
if (!lexxos::core::bootstrap(system)) {
    return;
}
auto& devices = system.devices();
auto* button = devices.digitalInput("in1");
auto* relay = devices.digitalOutput("out1");
if (!button || !relay) {
    return;
}
button->setHandler(onButton, relay);
while (devices.pollDigitalInputs(system.platform().clock.uptimeMs())) {
    system.platform().clock.sleepMs(5);
}
```

`onButton` — обработчик приложения с сигнатурой `void(DigitalInputEvent, bool high, void* context)`. В этом примере
`context` содержит указатель на `DigitalOutput`. Установить или заменить обработчик можно без повторной настройки входа.
При ошибке чтения общий опрос возвращает `false`, обработав остальные входы.

Повторный поиск по имени возвращает тот же объект. Для отсутствующего имени или другого типа драйвера результат —
`nullptr`. Указатель можно сохранить до сброса реестра, следующего `bootstrap()` или уничтожения ядра. Опрос всех входов
и обработчики выполняются в вызывающей задаче. Вызовы управления одним выходом из нескольких задач приложение
сериализует самостоятельно.

## Инициализация

До обращения к GPIO ядро проверяет размер таблицы, уникальность меток и физических выводов назначенных драйверов,
совместимость направления и параметры входов. В памяти выделяется один блок под фактически назначенные драйверы. Общие
службы GPIO и обработчики приложение передаёт по ссылке или указателю.

Если настройка устройства завершается ошибкой, `bootstrap()` возвращает `false`, и частично созданный набор драйверов
недоступен. Выходы, которые драйвер успел настроить, уже получили указанный платой начальный уровень. Выбирайте его с
учётом подключённой нагрузки.

При обновлении SDK используйте библиотеку и заголовки из одного комплекта и пересоберите приложение: описание платы и
реестр входят в бинарный интерфейс.
