# PicoCalc southbridge

Документ фиксирует текущее состояние поддержки STM32 southbridge на ClockworkPi PicoCalc в LexxOS 0.0.7.
Дальнейшие работы вынесены в [`docs/todo/roadmap.md`](todo/roadmap.md).

## Назначение

STM32 southbridge на PicoCalc обслуживает клавиатуру, подсветку, батарею и часть power/reset функций. Для LexxOS это
один общий I2C-компонент по адресу `0x1F` на system I2C:

```text
System I2C: I2C1, SDA GP6, SCL GP7, 400 kHz
Address:    0x1F
Owner:      picocalc-keyboard during normal PicoCalc runtime
```

Клавиатурный FIFO является очередью событий. Если сторонняя команда напрямую прочитает FIFO или нарушит порядок чтения
регистров, input-драйвер может потерять `key-up`, после чего клавиша останется логически зажатой. Поэтому обычная
сборка PicoCalc не допускает прямую диагностику southbridge, пока зарегистрирован `picocalc-keyboard`.

Поведение сверено с upstream STM32 firmware ClockworkPi: [
`reg.h`](https://raw.githubusercontent.com/clockworkpi/PicoCalc/master/Code/picocalc_keyboard/reg.h)
описывает keyboard FIFO `0x09`, LCD backlight `0x05`, keyboard backlight `0x0A`, battery `0x0B` и power-off `0x0E`, а
[`fifo.ino`](https://raw.githubusercontent.com/clockworkpi/PicoCalc/master/Code/picocalc_keyboard/fifo.ino) удаляет
событие из очереди при FIFO dequeue. Поэтому LexxOS повторяет модель клиентского runtime-владельца: клавиатура читает
FIFO, а остальные клиенты получают cached state и безопасные команды через service contract.

## Реализованные слои

Основной код находится здесь:

```text
src/lexxos/devices/picocalc/picocalc_southbridge.*
src/lexxos/devices/picocalc/detail/picocalc_southbridge_backlight_mapper.*
src/lexxos/devices/picocalc/detail/picocalc_southbridge_command_tracker.*
src/lexxos/devices/picocalc/detail/picocalc_southbridge_requests.*
src/lexxos/devices/picocalc/detail/picocalc_southbridge_snapshot_cache.*
src/lexxos/devices/picocalc/picocalc_southbridge_resource_policy.*
src/lexxos/devices/picocalc/picocalc_southbridge_service.*
src/lexxos/devices/input/picocalc_keyboard.*
src/lexxos/devices/input/detail/input_event_queue.h
src/lexxos/devices/input/detail/picocalc_keyboard_diagnostics.*
src/lexxos/devices/input/detail/picocalc_keyboard_event_bridge.*
src/lexxos/devices/input/detail/picocalc_keyboard_fifo_poller.*
src/lexxos/devices/input/detail/picocalc_keyboard_initializer.*
src/lexxos/devices/input/detail/picocalc_keyboard_input_state.*
src/lexxos/devices/input/detail/picocalc_keyboard_recovery_policy.*
src/lexxos/devices/input/detail/picocalc_keyboard_runtime_service.*
src/lexxos/devices/input/detail/picocalc_keyboard_transport.*
src/lexxos/devices/input/detail/picocalc_scan_code_state.*
src/lexxos/apps/picocalc/picocalc_info_program.cpp
src/lexxos/apps/picocalc/picocalc_southbridge_program.cpp
```

`PicoCalcSouthbridge` содержит register-level API для:

- version/config/interrupt/debounce/polling-frequency;
- keyboard state и keyboard FIFO;
- battery;
- LCD backlight и keyboard backlight;
- C64 matrix/joystick registers;
- power-off support;
- reset request.

`PicoCalcSouthbridgeSnapshot` умеет хранить все эти поля и считать `validFieldCount()`.

Операции `PicoCalcSouthbridge` удерживают I2C transaction lock на всю пару `write register` + `read response`, чтобы
runtime keyboard polling не мог вклиниться между командой и ответом STM32. Write-команды проверяют ответ STM32 по
register id: если ответ пришёл не от целевого регистра, команда не считается успешной, а service сохраняет состояние
`refused` вместо ложного `ok`.

`PicoCalcSouthbridgeService` используется как cached snapshot service для активного клавиатурного владельца и остаётся
тонким facade над отдельными компонентами:

- `PicoCalcSouthbridgeSnapshotCache` хранит snapshot, refresh interval, timestamps и counters;
- `PicoCalcSouthbridgeBacklightMapper` переводит пользовательский диапазон `0..255` в firmware-specific backlight
  values;
- `PicoCalcSouthbridgeRequests` выполняет safe write requests для backlight/reset/power-off и публикует accepted values;
- `PicoCalcSouthbridgeCommandTracker` хранит последнюю service-backed команду и её status;
- `PicoCalcSouthbridgeResourcePolicy` централизует service name `picocalc-southbridge` и lock policy для runtime
  clients.

В обычном keyboard polling драйвер публикует `keyboard-state`, а расширенный southbridge refresh без чтения FIFO
выполняет только когда он due и клавиатурный поток спокоен. После любого FIFO event, pending input event, активного
modifier или зажатого scan-code full refresh откладывается минимум на `1000 ms`. Это не блокирует ввод и снижает риск
STM32/I2C wedge во время плотного `keyboard-live`/typing сценария. Команды `backlight`, `power` и `poweroff` обращаются
к southbridge через этот service path, а не создают отдельный direct I2C driver в shell.

Внутри `picocalc-keyboard` код разделён по ролям:

- `PicoCalcKeyboardController` владеет southbridge controller, runtime state и orchestrates initialize/update path;
- `PicoCalcKeyboardTransport` читает `keyboard-state`, FIFO и выполняет retry/drain при инициализации;
- `PicoCalcKeyboardInitializer` выполняет boot-time state read, cached snapshot publish и ограниченный FIFO drain;
- `PicoCalcKeyboardFifoPoller` выполняет bounded FIFO drain на update path и возвращает пачку событий без бизнес-логики;
- `PicoCalcKeyboardEventBridge` превращает raw FIFO events в `InputEvent`, управляет очередью, autorepeat и
  stale-release;
- `PicoCalcKeyboardInputState` хранит modifiers, pressed scan-code, autorepeat и stale-release policy;
- `PicoCalcScanCodeState` содержит компактную bitset-модель для 256 scan-code;
- `PicoCalcKeyboardDiagnostics` хранит communication/reset counters и форматирует throttled diagnostic output;
- `PicoCalcKeyboardRecoveryPolicy` хранит initialize/reset backoff и условие auto-reset;
- `PicoCalcKeyboardRuntimeService` реализует `PicoCalcSouthbridgeRuntimeService` как facade над тем же owner state;
- `InputEventQueue<32>` держит pending `InputEvent` с прежней политикой вытеснения самого старого события при
  переполнении.

`PicoCalcKeyboard` остаётся тонким `InputDevice` facade: он делегирует lifecycle/update, event polling, raw diagnostics
и `PicoCalcSouthbridgeRuntimeService` в `PicoCalcKeyboardController`. Controller вызывает transport, публикует cached
keyboard snapshot, отдаёт input events и решает, когда можно безопасно выполнять full southbridge refresh.

`PicoCalcKeyboardRuntimeService` реализует `PicoCalcSouthbridgeRuntimeService`. `DeviceManager` регистрирует этот
contract как `picocalc-southbridge`, поэтому `picocalc-info`, `picocalc-sb`, `backlight`, `display-power`, `power`,
`poweroff`, RTC helpers и статус-бар зависят от runtime service contract, а не от concrete keyboard class.
`DeviceManager` также ведёт service-level resource lock для `picocalc-southbridge`: `devices` показывает этот registered
service как service entry с состоянием `free`/`busy`, priority и owner. Runtime clients получают имя service и lock через
`PicoCalcSouthbridgeResourcePolicy`, поэтому owner/priorities остаются локальной политикой call site, а имя shared
resource не дублируется. При этом keyboard остаётся единственным потребителем FIFO.

## Архитектурный contract

PicoCalc southbridge считается provider-owned composite controller:

- provider/runtime owner — `picocalc-keyboard`;
- public input contract — `InputDevice` с именем `picocalc-keyboard`;
- public service contract — `PicoCalcSouthbridgeRuntimeService` с именем `picocalc-southbridge`;
- destructive stream — keyboard FIFO, доступен только владельцу;
- shared state — battery/backlight/power/config snapshot, доступен клиентам через cached service;
- safe writes — backlight/reset/power-off идут через service-backed методы и проверяют ответ STM32.

Это правило является частью общей архитектуры devices. Когда создание устройств переедет из `DeviceManager` в
`DriverProvider`/composition root, PicoCalc provider должен по-прежнему создавать один southbridge owner и
регистрировать
оба контракта от его имени. Новые runtime clients должны зависеть от `PicoCalcSouthbridgeRuntimeService`, а не от
`PicoCalcSouthbridge` register-level API и не от raw I2C bus.

## Ownership rules

При активном `picocalc-keyboard` действуют правила:

- `picocalc-keyboard` является единственным активным владельцем keyboard FIFO;
- `picocalc-info` показывает service cached snapshot и `direct-i2c: no`;
- `picocalc-info --direct-i2c` отказывается от direct I2C и остаётся в safe cached-snapshot режиме;
- `picocalc-sb` показывает service cached snapshot вместо прямого чтения registers;
- `picocalc-sb --poll` не читает FIFO и выводит предупреждение;
- `picocalc-sb --lcd`, `--kbd` и `--reset` отказываются выполнять direct write;
- `backlight lcd <0..255>` выполняет safe service-backed write;
- `backlight kbd <0..255>` выполняет safe service-backed write и подтверждён на реальном PicoCalc;
- backlight service сохраняет общий пользовательский контракт `0..255`, а firmware-specific диапазон выбирает внутри:
  factory keyboard backlight квантуется в `0`, `32`, ..., `224`; JCS firmware хранит LCD как native step `0..9`, а
  keyboard как native step `0..3`;
- `backlight save` и `backlight restore` используют SD config `/mnt/sd/.lexx/backlight.conf`;
- boot runtime автоматически применяет сохранённые уровни после успешного SD automount;
- `display-power off`/`sleep` гасят LCD/keyboard backlight через service-backed write, а `on` восстанавливает уровни;
- `display-power timeout` хранит idle timeout в `/mnt/sd/.lexx/display-power.conf`;
- `power` обновляет service snapshot и показывает battery/backlight/power fields;
- `poweroff` выполняет VFS sync/unmount и запрашивает power-off через service-backed write; factory path подтверждён, а
  JCS software sleep/off request `0x40` пока может вернуть failure после успешного sync/unmount;
- `picocalc-info`, `picocalc-sb`, `backlight`, `display-power`, `power` и `poweroff` берут `picocalc-southbridge`
  service lock на время safe cached/service-backed операции;
- прямой I2C-режим допустим только в конфигурации, где `picocalc-keyboard` не зарегистрирован, и только через явный
  diagnostic flag `--direct-i2c`.

На PicoCalc обычный reboot RP2040 не всегда равен полному reset STM32 southbridge. Если southbridge уже оказался в
плохом
состоянии, полное отключение питания надёжнее обычного `reboot`.

## Power UX status

P0 power/backlight UX подтверждён на реальном PicoCalc:

```text
battery snapshot        ok
lcd backlight           ok
keyboard backlight      ok
saved backlight restore ok
write-response check    ok
power command output    ok
poweroff factory path   ok
poweroff JCS request    known limitation
keyboard FIFO safety    ok
```

`power` показывает:

- источник данных `source=picocalc-southbridge direct-i2c=no`;
- battery raw byte;
- charging flag;
- процент заряда из младших 7 бит battery register;
- человекочитаемый уровень `empty-or-disconnected`/`critical`/`low`/`medium`/`good`;
- display-only low-battery policy;
- power-off support;
- текущие уровни LCD/keyboard backlight;
- последнюю service-backed write-команду с `state=ok`, `state=failed` или `state=refused`.

Low-battery policy в LexxOS сейчас deliberately display-only: shell session остаётся активной, пока southbridge snapshot
читается. Принудительное shutdown-поведение остаётся на стороне STM32/PMU firmware; если позже потребуется действие со
стороны LexxOS, его нужно добавлять отдельной политикой поверх текущей диагностики.

## Cached snapshot

В активной PicoCalc-сборке cached snapshot содержит:

```text
version/config/interrupt/debounce/polling-frequency
keyboard-state
battery
lcd-backlight
keyboard-backlight
c64-matrix/c64-joystick
power-off-supported
snapshot-ms
snapshot-full-refresh-ms
snapshot-last-refresh-ok
snapshot-fields
snapshot-updates
snapshot-failures
stale-event-releases
stale-scan-code-releases
raw-state
```

После первого расширенного refresh ожидаемое значение `snapshot-fields` больше `1`; в smoke-test на PicoCalc оно должно
показывать battery/backlight/power fields без direct I2C из shell. `snapshot-ms` отражает последнее обновление snapshot,
а `snapshot-full-refresh-ms` — последний полный southbridge refresh.

Во время активного набора `snapshot-ms` может продолжать обновляться от lightweight `keyboard-state`, а
`snapshot-full-refresh-ms` и battery/backlight fields могут временно отставать. После паузы во вводе больше `1000 ms`
полный refresh должен догнать актуальное состояние без роста `snapshot-failures`.

`snapshot-failures` должен оставаться `0` в стабильном сценарии, а `snapshot-last-refresh-ok` — `yes`.
`stale-event-releases` и `stale-scan-code-releases` могут увеличиваться, если драйвер сам сбросил non-repeatable
scan-code после потерянного `key-up`.

## Клавиатура

`PicoCalcKeyboard` читает events из FIFO register `0x09`, а register `keyboard-state` `0x04` использует как подсказку.
На некоторых прошивках southbridge FIFO count может быть равен нулю даже при наличии событий, поэтому драйвер делает
короткий idle probe FIFO.

Реализовано:

- очередь input events на 32 события;
- хранение состояния всех 256 scan-code;
- фильтрация повторного `key-down` для уже нажатого scan-code;
- отслеживание `Alt`, `ShiftLeft`, `ShiftRight`, `Sym`, `Ctrl`;
- `character`, `scanCode` и `modifiers` в `InputEvent`;
- software autorepeat для `Left`, `Right`, `Up`, `Down`, `Backspace`, `Space`;
- stale-release timeout для non-repeatable scan-code;
- защита `Space` от stale-release во время удержания, потому что он repeatable.

Текущая `KeyCode` карта живёт в `picocalc_keyboard_layout.cpp` и покрыта host contract tests для известных PicoCalc
scan-code. Она покрывает навигацию, Enter, Escape, Backspace, Space, цифры, `A..Z`, базовую ASCII punctuation,
`F1..F10`, `Break`, `Home`, `End`, `Delete` и `Insert`. Проверенная scan-code таблица описана в
[`docs/picocalc-keyboard-layout.md`](picocalc-keyboard-layout.md). На проверенной прошивке PicoCalc power button не
появляется в keyboard FIFO, поэтому `KeyCode::Power` не маппится в клавиатурном драйвере.

## Console input integration

PicoCalc input poll выполняется в background runtime task `picocalc-input` с интервалом 16 ms и ограничением на
количество событий за poll. Перед каждым коротким drain task берёт `picocalc-keyboard` input-device lock с owner
`console-input`. Lock не держится постоянно, поэтому diagnostic commands вроде `keyboard-live` и `input-test` могут
эксклюзивно забрать клавиатуру во время собственного запуска, а `picocalc-input` в это время не читает FIFO. Эти
diagnostic commands используют короткий retry/backoff при захвате interactive input lock, чтобы не падать из-за одного
фонового poll окна.

События PicoCalc keyboard преобразуются в общий console input stream перед shell/terminal layer:

- printable `character` попадает в stream как обычный байт;
- `Ctrl` + printable letter преобразуется в control byte, например `Ctrl-C`;
- стрелки, `Home`, `End` и `Delete` отправляются как ANSI cursor/control sequences;
- `Shift`/`Ctrl` + `Up`/`Down` прокручивают LCD scrollback на строку;
- `Sym`/`Alt` + `Up`/`Down` прокручивают LCD scrollback на страницу;
- `Sym`/`Alt` + `Home`/`End` переходят к началу или live-tail scrollback;
- `Escape` закрывает scrollback, если он активен;
- `Escape` вне scrollback в shell mode является shell cancel key и передаётся как `Ctrl-C`;
- `Escape` вне scrollback в program mode передаётся как настоящий ESC, чтобы `terminal::readKey()` видел
  `KeyCode::Escape`;
- `Break` передаётся как `Ctrl-C`.

Shell и full-screen terminal apps читают один и тот же console input stream. UART остаётся fallback: если injected
PicoCalc bytes отсутствуют, чтение переходит к `platform.console.readChar`.

## Диагностика

Безопасная проверка после прошивки:

```text
lexx:/> picocalc-info
lexx:/> picocalc-info --direct-i2c
lexx:/> picocalc-sb
lexx:/> devices
lexx:/> picocalc-sb --poll 2
lexx:/> picocalc-sb --lcd 40
lexx:/> backlight
lexx:/> backlight lcd 40
lexx:/> backlight kbd 10
lexx:/> backlight kbd 64
lexx:/> backlight save
lexx:/> backlight save lcd
lexx:/> backlight restore
lexx:/> backlight load
lexx:/> backlight restore lcd
lexx:/> display-power
lexx:/> display-power off
lexx:/> display-power on
lexx:/> display-power timeout
lexx:/> display-power timeout 60 sleep
press any PicoCalc key to wake display after idle timeout
lexx:/> display-power timeout off
lexx:/> power
lexx:/> keyboard-live -s 10 -i 16 picocalc-keyboard
type continuously during the whole keyboard-live window
lexx:/> picocalc-info
lexx:/> keyboard-live -s 10 -i 16 picocalc-keyboard
```

Ожидаемое поведение:

- `direct-i2c: no` в `picocalc-info` и `picocalc-sb`;
- `snapshot-fields` больше `1` после первого расширенного refresh;
- `snapshot-failures: 0`;
- `snapshot-last-refresh-ok: yes`;
- `devices` показывает `picocalc-southbridge` как `service` entry со status `free` вне активной команды;
- `picocalc-info --direct-i2c` отказывается от direct I2C при активном keyboard service;
- `picocalc-sb --poll 2` не читает FIFO напрямую;
- `picocalc-sb --lcd 40` отказывается от direct write;
- `backlight lcd 40` выполняется через service path;
- `backlight kbd 10` применяет nearest higher supported level: `32` на factory firmware или native step `1` на JCS;
- `backlight kbd 64` меняет keyboard backlight через service path и не повреждает keyboard FIFO;
- backlight/power/display-power команды не ломают keyboard FIFO: следующий `keyboard-live` продолжает получать
  `key-down`/`key-up`, а после отпускания repeatable клавиш `pressed` возвращается в `<none>`;
- `last-command` показывает `state=ok` для подтверждённых write-команд и `state=refused`, если STM32 вернул ответ не от
  целевого register;
- `backlight save` сохраняет уровни на SD card, если `/mnt/sd` смонтирован;
- `backlight save lcd` сохраняет только LCD level;
- `backlight restore` применяет доступные сохранённые уровни и явно печатает failed target при partial restore;
- `backlight restore lcd` применяет только LCD level;
- saved backlight config автоматически применяется после следующего boot SD automount;
- `display-power off`/`sleep` гасят LCD/keyboard backlight через service path;
- `display-power on` восстанавливает последние уровни подсветки;
- `display-power timeout 60 sleep` сохраняет idle timeout config на SD card, после бездействия переводит display в
  `sleep`, гасит LCD/keyboard backlight и пробуждается первым `KeyDown`;
- `power` показывает `source=picocalc-southbridge direct-i2c=no`;
- `poweroff` проверяется отдельно, когда устройство можно выключить; factory path должен выполнить VFS sync, unmount
  активных mount points и power-off через southbridge service, а JCS path сейчас может остановиться на
  `southbridge request failed` после успешного sync/unmount;
- `keyboard-live` эксклюзивно берёт input-device lock с retry/backoff, приостанавливает `picocalc-input` на время
  проверки, завершается без самопроизвольного ввода в shell и может использовать `--full` для raw layout diagnostics.
- во время плотного `keyboard-live` ввод не теряется; одиночный
  `[picocalc-keyboard] communication failure: stage=update-keyboard-fifo-read` допустим как known issue, если
  `ready=yes`, `consecutive=1`, `snapshot-ok=yes`, а после паузы больше `1000 ms` `snapshot-full-refresh-ms`
  обновляется снова без роста серии ошибок.

Этот smoke-test подтверждён на реальном PicoCalc с SD automount, saved backlight restore, плотным `keyboard-live`,
service-backed backlight/power/display-power командами и `snapshot-failures: 0`.

## Инварианты безопасности

- Не читать keyboard FIFO из `picocalc-sb` или другой shell-команды.
- Не возвращать direct `--lcd`, `--kbd`, `--reset` при активном `picocalc-keyboard`.
- Не включать печатный autorepeat для всех символов без lease/timeout-механизма.
- Не считать RP2040 `reboot` полным reset STM32 southbridge.
- Не обходить универсальные `backlight`, `display-power`, `power` и `poweroff` direct I2C командами в обычной
  PicoCalc-сборке.

Direct I2C диагностика для bring-up доступна только без registered `picocalc-southbridge` service:

```text
lexx:/> picocalc-info --direct-i2c
lexx:/> picocalc-sb --direct-i2c
```

В этом режиме вывод содержит `direct-i2c: yes`. Deprecated `picocalc-info --force` остаётся alias для
`picocalc-info --direct-i2c`, но новые smoke-test сценарии должны использовать `--direct-i2c`.

## Ограничения

Пока не реализовано:

- расширенная terminal/app семантика для `F1..F10`, `Insert` и будущих app hotkeys описана в PicoCalc-блоке
  [`docs/todo/roadmap.md`](todo/roadmap.md);
- software `poweroff` request на JCS southbridge пока не подтверждён, используйте физическую кнопку питания;
- одиночный `update-keyboard-fifo-read` warning на JCS считается неблокирующим, если `ready=yes`, `consecutive=1`,
  `snapshot-ok=yes` и ввод продолжает работать;
- отдельный PMU/power-event source для физической power button не планируется без конкретного пользовательского
  сценария; текущий сценарий закрывает автоматическое выключение экрана и подсветки;
- активная low-battery policy со стороны LexxOS поверх текущего display-only статуса.

Следующий технический трек описан в [`docs/todo/roadmap.md`](todo/roadmap.md).
