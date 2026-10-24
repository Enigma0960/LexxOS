# `edit`

edit a VFS text file.

## PicoCalc LCD

На PicoCalc `edit` использует геометрию активной LCD-консоли вместо фиксированного режима 80x24: status line
отрисовывается на последней строке текстовой области, а page navigation использует фактическую высоту экрана.

## Справка shell

```text
edit - edit a VFS text file

Starts a small screen-oriented text editor for ASCII files. It keeps the edited file in a fixed RAM buffer and writes it back after Ctrl+S.

Usage:
  edit <path>

Arguments:
  path required - VFS text file path

Runtime:
  launch: foreground-task
  stack: 4096 bytes
  priority: 1
  memory: external-preferred
  interface: console
  external quota: 131072 bytes
  internal workspace: 6344 bytes
```

## Доступность

Доступна в стандартной или full-сборке в зависимости от feature-флагов платы. Документ соответствует LexxOS 0.0.7.
