from __future__ import annotations

import unicodedata

DIRECTORY_ATTRIBUTE = 0x10
ARCHIVE_ATTRIBUTE = 0x20
LONG_NAME_ATTRIBUTE = 0x0F


def _write_le16(buffer: bytearray, offset: int, value: int) -> None:
    buffer[offset:offset + 2] = int(value).to_bytes(2, "little")


def _write_le32(buffer: bytearray, offset: int, value: int) -> None:
    buffer[offset:offset + 4] = int(value).to_bytes(4, "little")


def _is_valid_short_char(ch: str) -> bool:
    return ch.isascii() and (ch.isalnum() or ch in "$%'-_@~`!(){}^#&")


def _sanitize_short_part(text: str, limit: int) -> str:
    normalized = unicodedata.normalize("NFKD", text)
    result: list[str] = []
    for ch in normalized:
        if ch == ".":
            continue
        upper = ch.upper()
        if _is_valid_short_char(upper):
            result.append(upper)
        elif ch not in " \t\r\n":
            result.append("_")
        if len(result) >= limit:
            break
    return "".join(result) or "FILE"


def _short_name_bytes(alias: str) -> bytes:
    base, _, extension = alias.partition(".")
    if not base or len(base) > 8 or len(extension) > 3:
        raise ValueError(f"invalid FAT short alias: {alias!r}")
    return base.upper().ljust(8).encode("ascii") + extension.upper().ljust(3).encode("ascii")


def _is_plain_83_name(name: str) -> bool:
    if name in (".", ".."):
        return False
    base, dot, extension = name.partition(".")
    if not base or len(base) > 8 or len(extension) > 3 or (dot and "." in extension):
        return False
    if name.strip() != name or name.upper() != name:
        return False
    return all(_is_valid_short_char(ch) for ch in base + extension)


def make_unique_short_alias(name: str, used_aliases: set[str]) -> str:
    base_text, dot, extension_text = name.rpartition(".")
    if not dot:
        base_text = name
        extension_text = ""

    base = _sanitize_short_part(base_text, 8)
    extension = _sanitize_short_part(extension_text, 3) if extension_text else ""

    candidate = base[:8]
    if extension:
        candidate = f"{candidate}.{extension[:3]}"
    if candidate not in used_aliases:
        used_aliases.add(candidate)
        return candidate

    for tail in range(1, 100000):
        suffix = f"~{tail}"
        trimmed_base = base[: max(1, 8 - len(suffix))] + suffix
        candidate = trimmed_base
        if extension:
            candidate = f"{candidate}.{extension[:3]}"
        if candidate not in used_aliases:
            used_aliases.add(candidate)
            return candidate

    raise ValueError(f"cannot generate unique FAT short alias for {name!r}")


def _short_name_checksum(raw_short_name: bytes) -> int:
    checksum = 0
    for value in raw_short_name:
        checksum = (((checksum & 1) << 7) + (checksum >> 1) + value) & 0xFF
    return checksum


def _encode_lfn_chars(name: str) -> list[int]:
    return [value if (value := ord(ch)) <= 0xFFFF else ord("_") for ch in name]


def _write_lfn_entry(entry: bytearray, sequence: int, chars: list[int], checksum: int) -> None:
    entry[0] = sequence
    entry[11] = LONG_NAME_ATTRIBUTE
    entry[12] = 0
    entry[13] = checksum
    _write_le16(entry, 26, 0)

    padded = list(chars)
    if len(padded) < 13:
        padded.append(0x0000)
    while len(padded) < 13:
        padded.append(0xFFFF)

    offsets = [1, 3, 5, 7, 9, 14, 16, 18, 20, 22, 24, 28, 30]
    for offset, value in zip(offsets, padded):
        _write_le16(entry, offset, value)


def create_directory_entry(name: str, alias: str, *, attribute: int, first_cluster: int, size: int) -> bytes:
    raw_short = _short_name_bytes(alias)
    records: list[bytes] = []

    if not _is_plain_83_name(name) or name.upper() != alias:
        chars = _encode_lfn_chars(name)
        chunks = [chars[index:index + 13] for index in range(0, len(chars), 13)] or [[]]
        checksum = _short_name_checksum(raw_short)
        for reverse_index, chunk in enumerate(reversed(chunks)):
            sequence = len(chunks) - reverse_index
            if reverse_index == 0:
                sequence |= 0x40
            entry = bytearray(32)
            _write_lfn_entry(entry, sequence, chunk, checksum)
            records.append(bytes(entry))

    entry = bytearray(32)
    entry[0:11] = raw_short
    entry[11] = attribute
    _write_le16(entry, 26, first_cluster)
    _write_le32(entry, 28, size)
    records.append(bytes(entry))
    return b"".join(records)


def create_dot_entry(name: str, cluster: int) -> bytes:
    entry = bytearray(32)
    entry[0:11] = name.encode("ascii").ljust(11)
    entry[11] = DIRECTORY_ATTRIBUTE
    _write_le16(entry, 26, cluster)
    return bytes(entry)

