# Read-only storage ABI для `.lexx`

Внешние программы получили первый файловый ABI: чтение файлов через capability `storage-read`.

## Capability

Программа, которая хочет читать VFS/SD, должна быть упакована с capability:

```sh
python3 tools/lexx_pack.py app.bin app.lexx --cap console,storage-read
```

Без `storage-read` системный вызов `open_read` вернёт ошибку.

## SDK API

В `sdk/lexx/lexx.h` добавлены функции:

```cpp
int lexx::open_read(char const* path);
lexx::uint32_t lexx::read(lexx::uint32_t fd, void* data, lexx::size_t size);
int lexx::close(lexx::uint32_t fd);
```

Константы SDK: `lexx::first_file_fd == 3`, `lexx::max_open_files == 4`.

`read()` возвращает:

- `0` — конец файла;
- `1..N` — количество прочитанных байт;
- `lexx::no_data` (`0xffffffff`) — ошибка.

Файловые дескрипторы внешней программы начинаются с `3`; `0`, `1`, `2` зарезервированы под stdin/stdout/stderr.

## Ограничения первого этапа

- Только чтение.
- Максимум `lexx::max_open_files` (`4`) одновременно открытых файла на одну VM.
- Нет `seek`.
- Нет `stat`.
- Нет `write`/`append`.
- Путь нормализуется относительно текущего working directory shell/program context.

## Пример: `cat.lexx`

Добавлен пример:

```text
examples/lexx/cat
```

Сборка:

```sh
cd examples/lexx/cat
make clean
make
```

Проверка на устройстве:

```text
lexx:/> /mnt/sd/ext/cat.lexx /mnt/sd/test.txt
lexx:/> lexx-run /mnt/sd/ext/cat.lexx /mnt/sd/test.txt
```

Если файл читается корректно, содержимое будет выведено в консоль.
