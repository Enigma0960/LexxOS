# Автозапуск `.lexx` из shell

Внешнюю программу теперь можно запускать напрямую, без явного `lexx-run`:

```text
lexx:/> /mnt/sd/ext/hello.lexx
lexx:/> hello.lexx
```

Shell распознаёт первый токен команды по суффиксу `.lexx` после разбора redirection и внутренне превращает запуск в:

```text
lexx-run <file.lexx>
```

`lexx-run` остаётся отладочной командой для явного управления рантаймом:

```text
lexx:/> lexx-run --trace /mnt/sd/ext/hello.lexx
lexx:/> lexx-run --max-steps 500000 /mnt/sd/ext/hello.lexx
```

Важная договорённость: при прямом вызове `.lexx` опции `lexx-run` не применяются. Всё, что стоит после имени `.lexx`, передаётся guest-программе через `argc/argv`; `argv[0]` остаётся тем путём к `.lexx`, который ввёл пользователь.

Redirection обрабатывается до автозапуска, поэтому такие формы тоже должны работать через общий механизм shell:

```text
lexx:/> /mnt/sd/ext/hello.lexx > /mnt/sd/out.txt
lexx:/> /mnt/sd/ext/hello.lexx 2> /mnt/sd/err.txt
```
