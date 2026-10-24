# `backlight`

## Исходник

`src/lexxos/apps/utility/backlight_program.cpp`

## Подробное описание

Показывает или меняет уровни LCD/keyboard подсветки через board-level power/backlight service. На PicoCalc команда
использует `PicoCalcSouthbridgeService`, а на дисплеях с device-level backlight управляет подсветкой через
`DisplayDevice`.

## Параметры

- Без параметров — показать текущие уровни подсветки.
- `lcd <0..255>` / `kbd <0..255>` — установить уровень LCD или клавиатурной подсветки.
- `save [lcd|kbd|all]` — сохранить текущие уровни на SD.
- `restore [lcd|kbd|all]` / `load [lcd|kbd|all]` — восстановить сохранённые уровни.

## Примеры

```text
lexx:/> backlight
lexx:/> backlight lcd 40
lexx:/> backlight kbd 10
lexx:/> backlight save all
```

## Комментарий автора

Это основной пользовательский путь для подсветки. `save` пишет `/mnt/sd/.lexx/backlight.conf`, а boot runtime
автоматически применяет этот файл после успешного SD automount на PicoCalc. Диагностические записи через
`picocalc-sb` намеренно заблокированы при активной клавиатуре, чтобы не повредить состояние southbridge FIFO.

## Справка shell

```text
backlight - show or set board backlight

Shows or changes display/keyboard backlight through the board power service.

Usage:
  backlight [lcd|kbd <0..255>|save [lcd|kbd|all]|restore [lcd|kbd|all]|load [lcd|kbd|all]]

Arguments:
  target - backlight target or action
  value - brightness or save/restore target
```

## Доступность

Доступна в стандартной или full-сборке в зависимости от feature-флагов платы. Документ соответствует LexxOS 0.0.7.
