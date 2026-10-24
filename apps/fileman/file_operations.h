#pragma once

namespace fileman {

enum class FileOpResult {
	Ok,
	Exists,
	Missing,
	Invalid,
	DepthLimit,
	IoError,
};

FileOpResult copyPath(char const* source, char const* destination);
FileOpResult movePath(char const* source, char const* destination);
FileOpResult renamePath(char const* source, char const* destination);
FileOpResult deletePath(char const* source);
char const* describe(FileOpResult result);

} // namespace fileman
