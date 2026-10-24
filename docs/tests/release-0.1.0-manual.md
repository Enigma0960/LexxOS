# Короткий ручной регресс LexxOS 0.1.0

Имена тестовых файлов и идентификаторы портов в примерах условные; результаты измерений сохранены.

Сценарий проверяет ядро, shell, SD/VFS, загрузчик `.lexx` и доступную сеть.
Это ожидаемые результаты, а не отметки о прохождении. Minimal и full проверяются
отдельно. Для JC предполагаются SD, UART и подключённый Ethernet; на других платах
отсутствующие возможности отмечаются N/A. Внешняя периферия и экспериментальные
системы не требуются для базового регресса.

Длительные/повторные проверки вынесены в [отдельный сценарий](release-0.1.0-jc-manual-duration.md).
Производительность приложений не оценивается: важны формат, загрузка и возврат в shell.

## Подготовка

1. Сохраните данные SD и конфигурацию; используйте firmware и apps одного выпуска.
   Прошейте плату по `INSTALL.txt` соответствующего архива.
2. Для UART откройте свой порт на 115200 8N1 без hardware flow control. Сохраните лог.
3. Запишите commit/source digest из manifest, flavor, модель платы, версии SDK,
   SD и подключённые устройства. Выполните `version`, `board`, `mem`, `tasks`.
4. Используйте свободное имя тестового каталога: сначала
   `stat /mnt/sd/regression-test`. Если он существует, замените имя во всём сценарии.

## Shell и SD

Команды выполняйте по одной, дожидаясь prompt. При panic, незапрошенном reset,
зависании или ошибке данных сохраните лог и остановите сценарий.

```text
help
devices
storage
ls /mnt/sd
echo shell-ok
mkdir /mnt/sd/regression-test
echo sample-data > /mnt/sd/regression-test/original.txt
cat /mnt/sd/regression-test/original.txt
cp /mnt/sd/regression-test/original.txt /mnt/sd/regression-test/copy.txt
rename /mnt/sd/regression-test/copy.txt /mnt/sd/regression-test/moved.txt
cat /mnt/sd/regression-test/moved.txt
sync
umount /mnt/sd
mount /dev/sd0 /mnt/sd
cat /mnt/sd/regression-test/original.txt
shell-config load
```

Ожидается: правильная плата/flavor, доступный shell, готовая SD, точная строка
`sample-data` после copy/rename и повторного mount. Исходные пользовательские
файлы не меняются. Проверьте также Backspace, стрелки и history на `echo shell-ok`.
Наличие устройства в `devices` само по себе не подтверждает работу периферии.

## Доступная сеть

Выполните `net-status`. На JC с Ethernet — `eth status`; для настроенного Wi-Fi —
`wifi-status`, а на JC дополнительно `hosted-info`. Не изменяйте настройки ради
этого сценария. При доступном gateway выполните `ping --count 3 <gateway>`, затем
повторите чтение тестового файла SD.

Ожидается: link/IP соответствуют подключению, команды завершаются, shell и SD
продолжают работать. Потери ping, обусловленные сетью автора, не блокируют PASS
при отсутствии явного сбоя платы. При двух активных интерфейсах обычный ping
не подтверждает конкретный интерфейс. Пароли и содержимое сетевой конфигурации
не включайте в публикуемый лог.

## Приложения из apps

Для каждого файла выполните `lexx-info /mnt/sd/lexx/NAME.lexx`, затем один запуск:

```text
/mnt/sd/lexx/fileman.lexx /mnt/sd/regression-test /mnt/sd
/mnt/sd/lexx/pacman.lexx
/mnt/sd/lexx/snake.lexx
/mnt/sd/lexx/tetris.lexx
```

Дождитесь первого экрана, выйдите клавишей `q`, дождитесь prompt и убедитесь, что
строка ввода пуста. После каждого выхода выполните `echo returned` и `mem`.
Ожидается: заголовок/манифест читается без ошибки, программа запускается, shell
снова доступен. Не выполняйте операции записи в UI fileman. Игровой процесс,
FPS, звук и физическая клавиатура не оцениваются. Ctrl+C — отмена, не штатный выход.
Остаточный UART-ввод фиксируйте отдельно как известное наблюдение.

## Reboot и завершение

```text
sync
reboot
```

После загрузки проверьте `version`, `board`, `storage`, `net-status`, `mem`, `tasks`
и `cat /mnt/sd/regression-test/original.txt`. Ожидается сохранность данных.
Затем удалите только созданные сценарием файлы:

```text
rm /mnt/sd/regression-test/original.txt
rm /mnt/sd/regression-test/moved.txt
rmdir /mnt/sd/regression-test
sync
stat /mnt/sd/regression-test
```

Последняя команда должна сообщить об отсутствии каталога. Если каталог непуст,
посмотрите `ls`; не удаляйте неизвестные данные рекурсивно.

## Форма результата

Укажите дату, плату, commit/source digest, профиль, состав оборудования и
PASS/FAIL/N/A для shell, SD, сети, каждого приложения и reboot. К результату
приложите лог без секретов. Событие reset записывайте с uptime, последней командой,
reset reason и числом повторений; причина не выводится из одного reset reason.

Отдельно отметьте известные наблюдения и непроверенные функции. Длительные тесты,
внешние устройства и экспериментальные подсистемы не отмечаются PASS по этому
сценарию. Фактические результаты подготовки: [итоговый отчёт](release-0.1.0-validation.md).
