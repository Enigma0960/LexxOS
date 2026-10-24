# `display-test`

draw a test pattern or benchmark display throughput.

## Справка shell

```text
display-test - draw a test pattern or benchmark display throughput

Draws a RGB565 test pattern or measures display throughput across row, stream, double-buffer, cached-copy and PSRAM-staged paths.

Usage:
  display-test [name] [--bench] [--matrix] [--rows-scan] [--transport row|stream|double|cached|psram|all] [--size full|gb2|gb240|gb1|custom] [--baud <hz>] [--frames <count>] [--rows <count>] [--width <px>] [--height <px>]

Arguments:
  name - device name
  --bench - run display throughput benchmark instead of the visual gradient
  --matrix - run standard benchmark matrix: full, gb2, gb240 and gb1 sizes
  --rows-scan - scan double-buffer rows-per-chunk values for the selected benchmark size
  --transport <mode> default=stream - benchmark transport: row, stream, double, cached, psram or all
  --size <name> default=full - benchmark size: full, gb2, gb240, gb1 or custom
  --baud <hz> default=0 - temporarily set display SPI baud rate before benchmark
  --keep-baud - do not restore the previous display bus speed after benchmark
  --frames <count> default=20 - benchmark frame count
  --width <px> default=0 - custom benchmark width
  --height <px> default=0 - custom benchmark height
  --x <px> - benchmark rectangle x offset; centered when omitted
  --y <px> - benchmark rectangle y offset; centered when omitted
  --rows <count> default=16 - rows per chunk for double/psram transports, max 64
  --psram-address <addr> default=2097152 - PSRAM staging address for psram transport

Runtime:
  launch: foreground-task
  stack: 5120 bytes
  priority: 1
  memory: internal-only
  interface: graphical
```

## Доступность

Доступна в стандартной или full-сборке в зависимости от feature-флагов платы. Документ соответствует LexxOS 0.0.7.
