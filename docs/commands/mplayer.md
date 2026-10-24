# `mplayer` (историческое имя)

Текущая команда называется [`mplay`](mplay.md). Отдельный alias `mplayer` не зарегистрирован.
Ниже сохранена прежняя справка; для текущего поведения используйте страницу `mplay`.

> Опциональная команда: в standard-сборке LexxOS 0.0.7 отключена по умолчанию через `LEXXOS_FEATURE_MIDI=OFF`.

play a MIDI file from VFS.

## Справка shell

```text
mplayer - play a MIDI file from VFS

Reads a Standard MIDI file from VFS and maps note/program/controller events to the current AudioDevice synth/mixer backend.

Usage:
  mplayer [--voice <last|lead|bass|loud>] [--mono] <midi>

Arguments:
  -v, --voice <mode> - mono voice selector: last, lead, bass or loud
  mono - force single-tone playback even when the audio backend supports mixing
  midi - Standard MIDI file path
```

## Поведение

`mplayer` читает Standard MIDI file format `0` или `1` с PPQ timing, running status и несколькими треками. На PicoCalc
команда использует primary `AudioDevice` (`picocalc-audio`) и, если backend поддерживает микширование, отправляет в него
несколько активных голосов вместо одного выбранного тона.

Поддержанные события первого рабочего этапа:

- tempo meta event `FF 51`;
- note on/off, включая `note on velocity=0` как note off;
- program change для выбора простого тембра/waveform;
- control change `7` volume, `10` pan, `11` expression, `64` sustain, `120`/`123` all notes off, `121` reset
  controllers;
- channel 10 percussion как упрощённые noise/pulse/sine голоса.

Пример для тестовой мелодии:

```text
lexx:/> mplayer /mnt/sd/midi/example.mid
mplayer: format=1 tracks=5 division=480 audio=mix voice=last
...
mplayer: finished events=<n> notes=<n> off=<n> ctrl=<n> prog=<n> perc=<n> dropped=<n> silent=<n>
```

Если нужно сравнить с прежним монофоническим режимом:

```text
lexx:/> mplayer --mono --voice lead /mnt/sd/midi/example.mid
```

Это не полноценная MT-32/Munt-совместимая эмуляция: SysEx, ROM PCM attacks, LA partial structure, pitch bend, envelopes
и
reverb пока не реализованы. Текущая цель — маленький embedded-friendly MIDI playback path, который уже умеет несколько
партий, pan и разные базовые waveforms.

Документ соответствует LexxOS 0.0.7.
