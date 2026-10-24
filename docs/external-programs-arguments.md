# Аргументы внешних `.lexx` программ

Внешние программы получили первый ABI для параметров запуска.

Поддерживаемые формы запуска:

```text
lexx:/> /mnt/sd/ext/hello.lexx one two three
lexx:/> lexx-run /mnt/sd/ext/hello.lexx one two three
lexx:/> lexx-run --trace /mnt/sd/ext/hello.lexx one two three
lexx:/> lexx-run --max-steps 500000 /mnt/sd/ext/hello.lexx one two three
```

Для guest-программы `argv[0]` — это путь к `.lexx`, который был указан при запуске. Остальные элементы — пользовательские аргументы:

```text
argv[0] = /mnt/sd/ext/hello.lexx
argv[1] = one
argv[2] = two
argv[3] = three
```

## SDK API

В `sdk/lexx/lexx.h` добавлены функции:

```cpp
int lexx::argc();
char const* const* lexx::argv();
char const* lexx::argument(lexx::size_t index);
```

Пример:

```cpp
for (int i = 0; i < lexx::argc(); ++i) {
    lexx::write("argv[");
    // print i
    lexx::write("]=");
    lexx::write(lexx::argument(static_cast<lexx::size_t>(i)));
    lexx::write("\n");
}
```

## ABI

Добавлены системные вызовы:

| a7 | Name | Return |
|---:|---|---|
| 6 | argc | количество аргументов |
| 7 | argv | guest-адрес таблицы `char const* argv[]` |

Рантайм размещает таблицу `argv[]` и строки аргументов в верхней части guest-памяти и опускает начальный `sp` ниже этого блока. Благодаря этому стек не перезаписывает аргументы сразу после старта программы.

Ограничение первого этапа: максимум 32 аргумента. Если таблица и строки не помещаются в guest-память, запуск завершается ошибкой `arguments-too-large`.

## Проверка

Демо `examples/lexx/hello` теперь печатает `argc` и список `argv` при старте.

После пересборки demo:

```text
lexx:/> /mnt/sd/ext/hello.lexx alpha beta
LexxOS external RV32I demo
argc=3
argv[0]=/mnt/sd/ext/hello.lexx
argv[1]=alpha
argv[2]=beta
```
