# `display-text`

draw simple text on display.

## Справка shell

```text
display-text - draw simple text on display

Draws 5x7 uppercase ASCII text using RGB565 colors. Additional words are appended with spaces.

Usage:
  display-text <x> <y> <scale> <fg> <bg> <text...>

Arguments:
  x required - left coordinate
  y required - top coordinate
  scale required - integer text scale
  foreground required - text color
  background required - background color
  text required - text to draw; additional words are appended with spaces
```

## Доступность

Доступна в стандартной или full-сборке в зависимости от feature-флагов платы. Документ соответствует LexxOS 0.0.7.
