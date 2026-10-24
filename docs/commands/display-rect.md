# `display-rect`

draw a filled rectangle.

## Справка shell

```text
display-rect - draw a filled rectangle

Draws a filled RGB565 rectangle without using a full framebuffer.

Usage:
  display-rect <x> <y> <width> <height> <color> [name]

Arguments:
  x required - left coordinate
  y required - top coordinate
  width required - rectangle width
  height required - rectangle height
  color required - fill color: rgb565 value or color name
  name - display device name
```

## Доступность

Доступна в стандартной или full-сборке в зависимости от feature-флагов платы. Документ соответствует LexxOS 0.0.7.
