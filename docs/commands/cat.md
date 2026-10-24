# `cat`

print readable VFS file or stdin.

## Справка shell

```text
cat - print readable VFS file or stdin

Prints one or more readable VFS files. With no path it reads from standard input, so shell redirection like cat < /tmp/file works.

Usage:
  cat [path ...]

Runtime:
  launch: foreground-task
  stack: 4096 bytes
  priority: 1
  memory: internal-only
  interface: console
  internal workspace: 225 bytes
```

## Доступность

Доступна в стандартной или full-сборке в зависимости от feature-флагов платы. Документ соответствует LexxOS 0.0.7.
