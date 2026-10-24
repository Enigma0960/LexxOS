# `info`

## Назначение

Показывает сведения об одном файле или каталоге из VFS. Команда не изменяет содержимое объектов.
Существующая команда `lexx-info` остаётся доступна, включая её режим `--json`.

## Синтаксис

```text
info <path>
```

Путь может быть относительным или абсолютным. Это **один путь**, а не glob-маска: `*` и `?`
не разворачиваются в список файлов (команда `info *.*` не перечисляет все файлы).

## Поля

Для файла: `name`, `path`, `type`, `size`, `created`, `modified`, `permissions`.
Для каталога: `name`, `path`, `type`, `created`, `modified`, `files`, `directories`, `permissions`.
`files` и `directories` — количество непосредственных дочерних элементов, **не рекурсивный** подсчёт.

- Пустая строка `created` или `modified` означает, что VFS не предоставляет дату.
- В FatFs `FILINFO` доступно **время последней модификации**, но нет времени создания. FAT время имеет точность 2
  секунды, часовой пояс в нём не хранится.
- Для FAT `permissions` показывает признак `read-only` либо `read/write`; это **не POSIX-права**. Дополнительное поле
  `fat attributes` отображает флаги read-only, hidden, system, directory и archive. Для других файловых систем поле
  `permissions` остаётся пустым.
- Тип определяется по сигнатуре известных форматов (LEXX, ELF, PNG, JPEG, GIF, BMP, PDF, ZIP), затем по расширению или
  первым байтам. Это информационная классификация, не проверка валидности документа.

Для `.lexx` команда дополнительно проверяет заголовок и CRC32 манифеста и выводит
заголовок LEXX и поля JSON отдельными строками `manifest.schema`, `manifest.id`,
`manifest.name`, `manifest.version`, `manifest.description`, `manifest.author`,
`manifest.license`, `manifest.contacts.*`, `manifest.build.timestamp`.
Устаревшие файлы без манифеста поддерживаются. Некорректный заголовок или CRC32
вызывает ошибку, но не запускает программный код.

## Примеры

```text
lexx:/> info /mnt/sd/apps/fileman.lexx
lexx:/> info /mnt/sd/docs
lexx:/> info /proc/tasks
```

## Исходник

`src/lexxos/apps/fs/info_program.cpp`
