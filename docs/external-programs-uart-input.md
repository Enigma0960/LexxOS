# UART-ввод для foreground `.lexx` программ

Если foreground-программа находится в фокусе, `Shell::feed()` больше не обрабатывает обычные UART-символы как ввод shell-строки.

Вместо этого байт UART передаётся сразу в две очереди:

- `program_input` — как `InputEvent`, чтобы работал `lexx::poll_key()`;
- `console_input` — как обычный символ, чтобы работал будущий/существующий `lexx::read_char()`.

Для `Ctrl+C` shell дополнительно вызывает `ProgramSupervisor::requestStop()` для текущей foreground-программы. Это обеспечивает одинаковое поведение остановки из UART-консоли и с аппаратной клавиатуры.

Проверка:

```text
lexx:/> /mnt/sd/ext/hello.lexx
```

Ожидаемо:

- ввод `q` из UART завершает demo через `lexx::poll_key()`;
- `Ctrl+C` из UART останавливает foreground-программу;
- ввод с аппаратной клавиатуры продолжает работать как раньше.
