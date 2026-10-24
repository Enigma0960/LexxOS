# `gb-test-suite`

run Blargg Game Boy CPU individual test suite.

## Справка shell

```text
gb-test-suite - run Blargg Game Boy CPU individual test suite

Runs the 11 ROM files from gb-test-roms/cpu_instrs/individual and prints a compact pass/fail summary.

Usage:
  gb-test-suite [--cycles count] [--trace] [--stop-on-fail] <dir>

Arguments:
  -c, --cycles <count> default=143818752 - maximum emulated CPU cycles per test ROM
  -t, --trace - print detailed diagnostics for every test ROM
  --stop-on-fail - stop the suite after the first failed, incomplete or stopped ROM
  dir required - directory containing Blargg cpu_instrs/individual ROM files
```

## Доступность

Доступна в стандартной или full-сборке в зависимости от feature-флагов платы. Документ соответствует LexxOS 0.0.7.
