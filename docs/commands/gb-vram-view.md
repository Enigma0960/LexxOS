# `gb-vram-view`

show Game Boy VRAM tile data.

## Справка shell

```text
gb-vram-view - show Game Boy VRAM tile data

Runs a bounded ROM warmup and renders VRAM directly: decoded tile data, BG tile map debug blocks, synthetic map tiles, forced BG output, or a raw VRAM heatmap.

Usage:
  gb-vram-view [--mode tiles|map|map-tiles|bg|bg-scan|heat] [--target auto|any|tile-data|map|map0|map1] [--min-target count] [--auto-tile] [--force-visible] [--bg-map auto|9800|9c00] [--bg-tiles auto|8000|8800] [--display crop|fit|none] [--tile number] [--max-cycles count] [--trace] <rom>

Arguments:
  -t, --tile <number> default=0 - first tile index to show from VRAM tile data
  -m, --mode <tiles|map|map-tiles|bg|bg-scan|heat> default=tiles - VRAM visualization mode: tile data, BG tile map, synthetic map tiles, forced BG render, BG mode scan, or raw VRAM heatmap
  --target <auto|any|tile-data|map|map0|map1> default=auto - VRAM area that must reach min-target before rendering
  --min-target <count> default=1 - minimum non-zero bytes in the selected VRAM target before rendering
  --auto-tile - for tile mode, start the view near the first non-zero tile-data byte
  --force-visible - for BG mode, force LCD enable, BG enable and DMG palette for diagnostics
  --bg-map <auto|9800|9c00> default=auto - for BG diagnostics, override the background tile-map base
  --bg-tiles <auto|8000|8800> default=auto - for BG diagnostics, override unsigned or signed tile-data addressing
  --max-cycles <count> default=2247168 - maximum warmup cycles before rendering VRAM data
  -r, --trace - print warmup CPU and IO diagnostics
  --display <crop|fit|none> default=crop - display mapping: crop keeps 1:1 pixels, fit downsamples the full 160x144 GB frame, none runs without blitting
  rom required - Game Boy ROM image path
```

## Доступность

Доступна в стандартной или full-сборке в зависимости от feature-флагов платы. Документ соответствует LexxOS 0.0.7.
