#pragma once

namespace lexxos::apps::gameboy_programs {

enum class VramViewMode {
	Tiles,
	Map,
	MapTiles,
	Bg,
	BgScan,
	Heat,
};

enum class VramWarmupTarget {
	Auto,
	Any,
	TileData,
	Map,
	Map0,
	Map1,
};

} // namespace lexxos::apps::gameboy_programs
