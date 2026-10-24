#!/usr/bin/env python3
from __future__ import annotations

import argparse
import re
import subprocess
import sys
from dataclasses import dataclass, replace
from pathlib import Path


@dataclass(frozen=True)
class CommandDoc:
    name: str
    summary: str
    help_text: str
    availability: str
    source: str | None = None
    description: str = ""
    parameters: str = ""
    examples: str = ""
    author: str = ""


@dataclass(frozen=True)
class SourceDoc:
    name: str
    source: str
    platforms: tuple[str, ...] = ()
    boards: tuple[str, ...] = ()
    description: str = ""
    parameters: str = ""
    examples: str = ""
    author: str = ""


SOURCE_DOC_SECTIONS = {
    "description",
    "parameters",
    "examples",
    "author",
}


def strip_comment_line(line: str) -> str:
    line = line.strip()
    if line.startswith("*"):
        line = line[1:]
        if line.startswith(" "):
            line = line[1:]
    return line.rstrip()


def clean_doc_text(lines: list[str]) -> str:
    while lines and not lines[0].strip():
        lines.pop(0)
    while lines and not lines[-1].strip():
        lines.pop()
    return "\n".join(lines).strip()


def parse_source_doc_block(block: str, path: Path, repo_root: Path) -> SourceDoc | None:
    lines = [strip_comment_line(line) for line in block.splitlines()]
    command_name = ""
    source = str(path.relative_to(repo_root))
    current_section = ""
    selectors: dict[str, tuple[str, ...]] = {}
    sections: dict[str, list[str]] = {name: [] for name in SOURCE_DOC_SECTIONS}

    for line in lines:
        if line.startswith("@lexxos-command-doc"):
            parts = line.split(maxsplit=1)
            if len(parts) == 2:
                command_name = parts[1].strip()
            continue
        if line.startswith(("@platforms ", "@boards ")):
            key, values = line[1:].split(maxsplit=1)
            selectors[key] = tuple(values.split())
            current_section = ""
            continue
        if line.startswith("@source"):
            parts = line.split(maxsplit=1)
            if len(parts) == 2:
                source = parts[1].strip()
            continue
        if line.startswith("@"):
            section = line[1:].strip()
            if section == "end":
                current_section = ""
            elif section in SOURCE_DOC_SECTIONS:
                current_section = section
            continue
        if current_section:
            sections[current_section].append(line)

    if not command_name:
        return None

    return SourceDoc(
        name=command_name,
        source=source,
        platforms=selectors.get("platforms", ()),
        boards=selectors.get("boards", ()),
        description=clean_doc_text(sections["description"]),
        parameters=clean_doc_text(sections["parameters"]),
        examples=clean_doc_text(sections["examples"]),
        author=clean_doc_text(sections["author"]),
    )


def source_docs(repo_root: Path, platform: str, board: str) -> dict[str, SourceDoc]:
    docs: dict[str, SourceDoc] = {}
    for path in sorted((repo_root / "src").rglob("*.h")):
        text = path.read_text(encoding="utf-8")
        for match in re.finditer(r"/\*\*(.*?)\*/", text, flags=re.DOTALL):
            if "@lexxos-command-doc" not in match.group(1):
                continue
            doc = parse_source_doc_block(match.group(1), path, repo_root)
            if doc is None or (doc.platforms and platform not in doc.platforms) or (doc.boards and board not in doc.boards):
                continue
            if doc.name in docs:
                raise ValueError(f"Ambiguous documentation for {doc.name}: {docs[doc.name].source} and {doc.source}; add @platforms/@boards selectors")
            docs[doc.name] = doc
    return docs


def run_host_shell(executable: Path, commands: list[str]) -> str:
    input_text = "\n".join(["shell-config autosuggest off", *commands, "exit", ""])
    result = subprocess.run(
        [str(executable)],
        input=input_text,
        text=True,
        capture_output=True,
        check=True,
        timeout=30,
    )
    return result.stdout.replace("\r\n", "\n").replace("\r", "\n")


def parse_command_list(output: str) -> dict[str, str]:
    commands: dict[str, str] = {}
    for line in output.splitlines():
        if "(disabled)" in line:
            continue
        match = re.fullmatch(r"([a-z0-9][a-z0-9-]*)(?: \([^)]+\))? - (.+)", line.strip())
        if match:
            commands.setdefault(match.group(1), match.group(2))
    return commands


def parse_help_blocks(output: str) -> dict[str, str]:
    markers = list(re.finditer(r"lexx:/> help ([a-z0-9][a-z0-9-]*)\n", output))
    blocks: dict[str, str] = {}
    for index, marker in enumerate(markers):
        end = markers[index + 1].start() if index + 1 < len(markers) else len(output)
        block = output[marker.end():end]
        prompt = block.find("lexx:/> ")
        if prompt >= 0:
            block = block[:prompt]
        blocks[marker.group(1)] = block.strip()
    return blocks


def query_host(executable: Path) -> dict[str, CommandDoc]:
    listing = parse_command_list(run_host_shell(executable, ["help"]))
    if not listing:
        raise RuntimeError("Host returned no command list")
    details = parse_help_blocks(run_host_shell(executable, [f"help {name}" for name in listing]))
    missing = set(listing) - details.keys()
    if missing:
        raise RuntimeError(f"Missing host help blocks: {sorted(missing)}")
    return {
        name: CommandDoc(
            name=name,
            summary=summary,
            help_text=details[name],
            availability="Доступна в стандартной или full-сборке в зависимости от feature-флагов платы.",
        )
        for name, summary in listing.items()
    }


def clean_sim_help(name: str, output: str) -> str:
    normalized = output.replace("\r\n", "\n").replace("\r", "\n").strip()
    prefix = f"help {name}\n"
    if normalized.startswith(prefix):
        normalized = normalized[len(prefix):]
    prompt = normalized.rfind("\nlexx:/>")
    if prompt >= 0:
        normalized = normalized[:prompt]
    return normalized.strip()


def query_simulator(repo_root: Path, library: Path) -> dict[str, CommandDoc]:
    sys.path.insert(0, str(repo_root))
    from emulator.lexxos_emu import LexxOsSim, VirtualBoard

    result: dict[str, CommandDoc] = {}
    with LexxOsSim(library, VirtualBoard.dev_pico()) as sim:
        sim.boot()
        sim.run_until_output_contains("lexx:/> ", timeout=5.0)
        sim.write_line("shell-config autosuggest off")
        sim.run_until_output_contains("lexx:/> ", timeout=5.0)
        sim.write_line("help")
        listing = parse_command_list(sim.run_until_output_contains("lexx:/> ", timeout=5.0))
        if not listing:
            raise RuntimeError("Simulator returned no command list")
        for name, summary in listing.items():
            sim.write_line(f"help {name}")
            help_text = clean_sim_help(name, sim.run_until_output_contains("lexx:/> ", timeout=5.0))
            result[name] = CommandDoc(
                name=name,
                summary=summary,
                help_text=help_text or f"{name} - {summary}",
                availability="Доступна при включении соответствующих feature-флагов; Game Boy-команды требуют LEXXOS_FEATURE_GAMEBOY.",
            )
    return result


def apply_source_docs(commands: dict[str, CommandDoc], docs: dict[str, SourceDoc]) -> None:
    for name, doc in docs.items():
        if name not in commands:
            continue
        command = commands[name]
        commands[name] = CommandDoc(
            name=command.name,
            summary=command.summary,
            help_text=command.help_text,
            availability=command.availability,
            source=doc.source,
            description=doc.description,
            parameters=doc.parameters,
            examples=doc.examples,
            author=doc.author,
        )


def render_optional_section(title: str, text: str) -> str:
    if not text:
        return ""
    return f"## {title}\n\n{text.strip()}\n\n"


def command_summary(command: CommandDoc) -> str:
    if command.description:
        first_paragraph = command.description.split("\n\n", maxsplit=1)[0]
        return " ".join(line.strip() for line in first_paragraph.splitlines()).rstrip(".")
    return command.summary.rstrip(".")


def existing_extra_sections(path: Path, generated_titles: set[str]) -> str:
    if not path.exists():
        return ""

    lines = path.read_text(encoding="utf-8").splitlines()
    sections: list[tuple[str, list[str]]] = []
    current_title = ""
    current_lines: list[str] = []

    for line in lines:
        match = re.fullmatch(r"## (.+)", line)
        if match:
            if current_title:
                sections.append((current_title, current_lines))
            current_title = match.group(1).strip()
            current_lines = [line]
        elif current_title:
            current_lines.append(line)

    if current_title:
        sections.append((current_title, current_lines))

    preserved = [
        "\n".join(section).strip()
        for title, section in sections
        if title not in generated_titles
    ]
    if not preserved:
        return ""
    return "\n\n".join(preserved) + "\n\n"


def render_command(command: CommandDoc, extra_sections: str = "") -> str:
    source = ""
    if command.source:
        source = f"## Исходник\n\n`{command.source}`\n\n"
    summary = "" if command.description else f"{command.summary.rstrip('.')}.\n\n"
    return (
        f"# `{command.name}`\n\n"
        f"{summary}"
        f"{source}"
        f"{render_optional_section('Подробное описание', command.description)}"
        f"{render_optional_section('Параметры', command.parameters)}"
        f"{render_optional_section('Примеры', command.examples)}"
        f"{render_optional_section('Комментарий автора', command.author)}"
        f"{extra_sections}"
        "## Справка shell\n\n"
        "```text\n"
        f"{command.help_text.strip()}\n"
        "```\n\n"
        "## Доступность\n\n"
        f"{command.availability}\n"
    )


def render_index(commands: dict[str, CommandDoc]) -> str:
    lines = [
        "# Команды LexxOS",
        "",
        "Индекс сгенерирован из встроенной справки одной сборки; отсутствующие команды не включены.",
        "",
    ]
    for name in sorted(commands):
        summary = command_summary(commands[name])
        lines.append(f"- [`{name}`]({name}.md) - {summary}.")
    lines.append("")
    return "\n".join(lines)


def read_build_context(build_dir: Path) -> dict[str, str]:
    values = {}
    for line in (build_dir / "CMakeCache.txt").read_text().splitlines():
        match = re.match(r"([^:#]+):[^=]+=(.*)", line)
        if match:
            values[match[1]] = match[2]
    for key in ("LEXXOS_PLATFORM", "LEXXOS_BOARD", "LEXXOS_BUILD_FLAVOR"):
        if not values.get(key):
            raise ValueError(f"Missing {key} in {build_dir}/CMakeCache.txt")
    return values


def write_documents(output: Path, commands: dict[str, CommandDoc]) -> None:
    output.mkdir(parents=True, exist_ok=True)
    for name, command in sorted(commands.items()):
        command_path = output / f"{name}.md"
        # Generated sections must not survive when their metadata disappears.
        generated = {"Исходник", "Подробное описание", "Параметры", "Примеры", "Комментарий автора", "Справка shell", "Доступность"}
        extra_sections = existing_extra_sections(command_path, generated)
        command_path.write_text(render_command(command, extra_sections), encoding="utf-8")
    # Keep manually maintained files on disk, but never list unavailable commands.
    (output / "README.md").write_text(render_index(commands), encoding="utf-8")


def main() -> int:
    parser = argparse.ArgumentParser(description="Generate command documentation for one configured and built LexxOS target")
    parser.add_argument("--build-dir", type=Path, required=True, help="Configured build directory (CMakeCache.txt)")
    inputs = parser.add_mutually_exclusive_group(required=True)
    inputs.add_argument("--host", type=Path, help="Host executable from this build")
    inputs.add_argument("--sim-lib", type=Path, help="Simulator library from this build")
    inputs.add_argument("--help-dump", type=Path, help="Board UART transcript: help, then help <name> for every listed command; autosuggest off")
    parser.add_argument("--output", type=Path, help="Default: <build-dir>/command-docs")
    args = parser.parse_args()

    repo_root = Path(__file__).resolve().parents[1]
    build_dir = args.build_dir.resolve()
    context = read_build_context(build_dir)
    platform, board, flavor = (context[key] for key in ("LEXXOS_PLATFORM", "LEXXOS_BOARD", "LEXXOS_BUILD_FLAVOR"))
    docs = source_docs(repo_root, platform, board)
    artifact = args.host or args.sim_lib
    if artifact and not artifact.resolve().is_relative_to(build_dir):
        parser.error("Runtime artifact must belong to --build-dir")
    if args.host:
        if platform != "host":
            parser.error("--host requires a host build")
        commands = query_host(args.host.resolve())
    elif args.sim_lib:
        if platform != "sim":
            parser.error("--sim-lib requires a sim build")
        commands = query_simulator(repo_root, args.sim_lib.resolve())
    else:
        transcript = args.help_dump.read_text().replace("\r", "")
        listing = parse_command_list(transcript)
        details = parse_help_blocks(transcript)
        if not listing or set(listing) - details.keys():
            raise ValueError("Incomplete help dump: need command list and help blocks for every command")
        commands = {name: CommandDoc(name, summary, details[name], "") for name, summary in listing.items()}
    version = context.get("CMAKE_PROJECT_VERSION", "unknown")
    availability = f"Сборка: `{board}` / `{flavor}`, платформа `{platform}`, LexxOS {version}. Набор команд получен из runtime help."
    commands = {name: replace(command, availability=availability + (" Экспериментальная программа, только full; интерфейс и поведение могут меняться." if name == "gpio-analyzer" else "")) for name, command in commands.items()}
    if flavor != "full" and "gpio-analyzer" in commands:
        raise ValueError("gpio-analyzer found outside full: rebuild firmware before generating documentation")
    apply_source_docs(commands, docs)
    output = args.output.resolve() if args.output else build_dir / "command-docs"
    write_documents(output, commands)
    print(f"generated {len(commands)} command documents for {board}/{flavor} in {output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
