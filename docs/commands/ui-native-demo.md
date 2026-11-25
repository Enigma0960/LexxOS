# ui-native-demo / ui-native-tree-demo

Нативные демонстрации LexxUI включаются явным флагом `LEXXOS_FEATURE_UI_NATIVE_DEMO`.
Флаг требует включённых DISPLAY и KEYBOARD. Команды используют системный
`NativeTransport` и тот же пример дерева UI, что и гостевой вариант.

```text
ui-native-demo
ui-native-tree-demo
```

Обе команды запускаются как foreground graphical task, без фонового режима.
Команды требуют доступного дисплея. Host-сборка проверяет регистрацию и справку;
наличие команды на host не означает наличие графического backend.

Список исходников, регистрация и metadata справки формируются из
`config/cmake/commands.json`. Поле `opt_in` сохраняет прежнее поведение:
явный feature-флаг включает демонстрации независимо от групп обычных команд.
