# `lexx-info`

## Назначение

Читает заголовок и встроенный JSON-манифест внешнего .lexx файла **без запуска** гостевого кода.

## Синтаксис

    lexx-info [--json] <file.lexx>

Без флагов показывает формат/ABI, архитектуру, capabilities, размеры и JSON.
Флаг --json выводит только встроенный JSON. Для старых файлов без манифеста обычный режим
показывает manifest: not present (legacy), а --json возвращает ошибку.

## Примеры

    lexx:/> lexx-info /mnt/sd/apps/hello.lexx
    lexx:/> lexx-info --json /mnt/sd/apps/hello.lexx

Полное описание: [манифесты приложений](../external-programs-manifest.md).

## Исходник

    src/lexxos/apps/lexx/lexx_info_program.cpp
