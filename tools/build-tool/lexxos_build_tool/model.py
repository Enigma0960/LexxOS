from __future__ import annotations

import json

from typing import Any
from pathlib import Path
from dataclasses import dataclass, field

DATA_DIR = Path(__file__).resolve().parent / "data"


@dataclass(frozen=True)
class FeatureOption:
    key: str
    label: str
    default: str = "OFF"
    board_controlled: bool = False
    visible: bool = True


@dataclass(frozen=True)
class BoardOption:
    name: str
    cmake_board: str
    platform: str
    notes: str = ""
    manual: bool = False
    preset: str = ""
    build_dir: str = ""
    user_preset_name: str = ""
    cmake_defaults: dict[str, str] = field(default_factory=dict)
    feature_defaults: dict[str, str] = field(default_factory=dict)


@dataclass(frozen=True)
class PresetOption:
    label: str
    configure_preset: str
    build_target: str | None = None
    notes: str = ""


@dataclass(frozen=True)
class PlatformOption:
    key: str
    label: str
    cmake_platform: str
    boards: list[BoardOption]
    presets: list[PresetOption]
    default_board: str
    default_build_dir: str
    default_cmake_options: dict[str, str] = field(default_factory=dict)
    visible_groups: tuple[str, ...] = ()
    instructions: str = ""


def _read_json(path: Path) -> dict[str, Any]:
    try:
        value = json.loads(path.read_text(encoding="utf-8"))
    except json.JSONDecodeError as error:
        raise RuntimeError(f"Invalid build-tool JSON file: {path}: {error}") from error
    if not isinstance(value, dict):
        raise RuntimeError(f"Invalid build-tool JSON file: {path}: root value must be an object")
    return value


def _string_dict(value: object) -> dict[str, str]:
    if not isinstance(value, dict):
        return {}
    result: dict[str, str] = {}
    for key, item in value.items():
        if isinstance(key, str):
            result[key] = str(item)
    return result


def _string_tuple(value: object) -> tuple[str, ...]:
    if not isinstance(value, list):
        return ()
    return tuple(str(item) for item in value if isinstance(item, str))


def _feature_option(value: object) -> FeatureOption:
    if not isinstance(value, dict):
        raise RuntimeError("Invalid feature entry in build-tool catalog")
    return FeatureOption(
        key=str(value["key"]),
        label=str(value.get("label", value["key"])),
        default=str(value.get("default", "OFF")),
        board_controlled=bool(value.get("boardControlled", False)),
        visible=bool(value.get("visible", True)),
    )


def _preset_option(value: object) -> PresetOption:
    if not isinstance(value, dict):
        raise RuntimeError("Invalid preset entry in build-tool catalog")
    build_target = value.get("buildTarget")
    return PresetOption(
        label=str(value.get("label", value.get("configurePreset", "manual"))),
        configure_preset=str(value.get("configurePreset", "")),
        build_target=str(build_target) if build_target is not None else None,
        notes=str(value.get("notes", "")),
    )


def _board_option(value: object) -> BoardOption:
    if not isinstance(value, dict):
        raise RuntimeError("Invalid board entry in build-tool board catalog")
    board_id = str(value.get("id", ""))
    return BoardOption(
        name=str(value.get("name", board_id or "Manual / custom")),
        cmake_board=board_id,
        platform=str(value.get("platform", "")),
        notes=str(value.get("notes", "")),
        manual=bool(value.get("manual", False)) or not board_id,
        preset=str(value.get("preset", "")),
        build_dir=str(value.get("buildDir", "")),
        user_preset_name=str(value.get("userPresetName", "")),
        cmake_defaults=_string_dict(value.get("cmakeDefaults", {})),
        feature_defaults=_string_dict(value.get("featureDefaults", {})),
    )


def _load_boards(directory: Path) -> dict[str, list[BoardOption]]:
    boards: dict[str, list[BoardOption]] = {}
    for path in sorted(directory.glob("*.json")):
        board = _board_option(_read_json(path))
        if not board.platform:
            raise RuntimeError(f"Board file does not define platform: {path}")
        boards.setdefault(board.platform, []).append(board)
    return boards


def _manual_board(platform_key: str) -> BoardOption:
    return BoardOption(
        name="Manual / custom board",
        cmake_board="",
        platform=platform_key,
        notes="Do not pass LEXXOS_BOARD. Add board variables manually in Additional CMake options.",
        manual=True,
    )


def _platform_option(value: object, boards_by_platform: dict[str, list[BoardOption]]) -> PlatformOption:
    if not isinstance(value, dict):
        raise RuntimeError("Invalid platform entry in build-tool catalog")
    key = str(value["key"])
    boards = [_manual_board(key), *boards_by_platform.get(key, [])]
    default_board = str(value.get("defaultBoard", ""))
    if default_board and all(board.cmake_board != default_board for board in boards):
        raise RuntimeError(f"Platform {key} defaultBoard references unknown board: {default_board}")
    return PlatformOption(
        key=key,
        label=str(value.get("label", key)),
        cmake_platform=str(value.get("cmakePlatform", key)),
        boards=boards,
        presets=[_preset_option(item) for item in value.get("presets", [])],
        default_board=default_board,
        default_build_dir=str(value.get("defaultBuildDir", f"build/configurator/{key}")),
        default_cmake_options=_string_dict(value.get("defaultCMakeOptions", {})),
        visible_groups=_string_tuple(value.get("visibleGroups", [])),
        instructions=str(value.get("instructions", "")),
    )


def _load_model() -> tuple[list[FeatureOption], dict[str, PlatformOption]]:
    catalog = _read_json(DATA_DIR / "catalog.json")
    features = [_feature_option(item) for item in catalog.get("features", [])]
    boards_by_platform = _load_boards(DATA_DIR / "boards")
    platforms = {
        platform.key: platform
        for platform in (_platform_option(item, boards_by_platform) for item in catalog.get("platforms", []))
    }
    return features, platforms


FEATURE_OPTIONS, PLATFORMS = _load_model()
FEATURES: list[tuple[str, str, str]] = [
    (feature.key, feature.label, feature.default)
    for feature in FEATURE_OPTIONS
    if feature.visible and not feature.board_controlled
]
BOARD_CONTROLLED_FEATURES: tuple[str, ...] = tuple(
    feature.key for feature in FEATURE_OPTIONS if feature.board_controlled
)


def default_path_text(path: Path) -> str:
    return str(path).replace(str(Path.home()), "~")
