#include "file_operations.h"
#include <lexx/lexx.h>

namespace fileman {
namespace {

	constexpr int kMaxPath = 128;
	constexpr int kMaxDepth = 8;
	constexpr unsigned kEntryLimit = 8192;
	lexx::DirectoryEntry g_entries[lexx::directory_page_size]{};
	char g_transferBuffer[512]{};

	bool equal(char const* a, char const* b) {
		for (; *a && *b; ++a, ++b) {
			if (*a != *b) {
				return false;
			}
		}
		return *a == *b;
	}

	bool containedIn(char const* child, char const* ancestor) {
		int i = 0;
		while (ancestor[i] && ancestor[i] == child[i]) {
			++i;
		}
		return ancestor[i] == 0 && (child[i] == 0 || child[i] == '/');
	}

	bool join(char* out, char const* directory, char const* name) {
		int i = 0;
		while (directory[i] && i < kMaxPath - 1) {
			out[i] = directory[i];
			++i;
		}
		if (directory[i]) {
			return false;
		}
		if (i != 1 || directory[0] != '/') {
			if (i == kMaxPath - 1) {
				return false;
			}
			out[i++] = '/';
		}
		for (int j = 0; name[j]; ++j) {
			if (i >= kMaxPath - 1) {
				return false;
			}
			out[i++] = name[j];
		}
		out[i] = 0;
		return true;
	}

	bool protectedPath(char const* path) {
		// Protect mount roots under /mnt (such as /mnt/sd).
		bool mountRoot = false;
		if (path[0] == '/' && path[1] == 'm' && path[2] == 'n' && path[3] == 't' && path[4] == '/') {
			mountRoot = path[5] != 0;
			for (int i = 5; path[i]; ++i) {
				if (path[i] == '/') {
					mountRoot = false;
				}
			}
		}
		return mountRoot || equal(path, "/") || equal(path, "/dev") || equal(path, "/mnt") || equal(path, "/proc") || equal(path, "/sys") || equal(path, "/tmp") || containedIn(path, "/dev") || containedIn(path, "/proc") || containedIn(path, "/sys");
	}

	FileOpResult copyItem(char const* source, char const* destination, int depth);
	FileOpResult deleteItem(char const* source, int depth);

	FileOpResult copyFile(char const* source, char const* destination) {
		int input = lexx::open_read(source);
		if (input < 0) {
			return FileOpResult::IoError;
		}
		// open_write_new checks target existence before any truncating VFS operation.
		int output = lexx::open_write_new(destination);
		if (output < 0) {
			(void) lexx::close(static_cast<lexx::uint32_t>(input));
			return FileOpResult::IoError;
		}
		bool success = true;
		for (;;) {
			auto const count = lexx::read(static_cast<lexx::uint32_t>(input), g_transferBuffer, sizeof(g_transferBuffer));
			if (count == lexx::no_data || count == lexx::cancelled || count > sizeof(g_transferBuffer)) {
				success = false;
				break;
			}
			if (count == 0) {
				break;
			}
			if (lexx::write_file(static_cast<lexx::uint32_t>(output), g_transferBuffer, count) != static_cast<int>(count)) {
				success = false;
				break;
			}
			lexx::yield();
		}
		if (lexx::close(static_cast<lexx::uint32_t>(output)) != 0) {
			success = false;
		}
		(void) lexx::close(static_cast<lexx::uint32_t>(input));
		if (!success) {
			// Only the newly-created destination may be removed on failure.
			(void) lexx::remove_file(destination);
			return FileOpResult::IoError;
		}
		return FileOpResult::Ok;
	}

	FileOpResult copyItem(char const* source, char const* destination, int depth) {
		if (depth > kMaxDepth) {
			return FileOpResult::DepthLimit;
		}
		if (equal(source, destination)) {
			return FileOpResult::Invalid;
		}
		lexx::FileStat stat{};
		if (lexx::stat(source, stat) < 0) {
			return FileOpResult::Missing;
		}
		lexx::FileStat target{};
		if (lexx::stat(destination, target) == 0) {
			return FileOpResult::Exists;
		}
		if (stat.type == lexx::node_regular_file) {
			return copyFile(source, destination);
		}
		if (stat.type != lexx::node_directory || protectedPath(source) || containedIn(destination, source)) {
			return FileOpResult::Invalid;
		}
		if (lexx::mkdir(destination) != 0) {
			return FileOpResult::IoError;
		}
		lexx::uint32_t offset = 0;
		while (offset < kEntryLimit) {
			// Re-enumerate on every iteration. Recursive calls share g_entries.
			auto const count = lexx::list_directory(source, offset, g_entries);
			if (count < 0) {
				return FileOpResult::IoError;
			}
			if (count == 0) {
				return FileOpResult::Ok;
			}
			char name[lexx::directory_name_size]{};
			for (unsigned j = 0; j < sizeof(name); ++j) {
				name[j] = g_entries[0].name[j];
			}
			++offset;
			if (equal(name, ".") || equal(name, "..")) {
				continue;
			}
			char from[kMaxPath]{};
			char to[kMaxPath]{};
			if (!join(from, source, name) || !join(to, destination, name)) {
				return FileOpResult::Invalid;
			}
			auto result = copyItem(from, to, depth + 1);
			if (result != FileOpResult::Ok) {
				return result;
			}
			lexx::yield();
		}
		return FileOpResult::DepthLimit;
	}

	FileOpResult deleteItem(char const* source, int depth) {
		if (depth > kMaxDepth || protectedPath(source)) {
			return FileOpResult::Invalid;
		}
		lexx::FileStat stat{};
		if (lexx::stat(source, stat) < 0) {
			return FileOpResult::Missing;
		}
		if (stat.type == lexx::node_regular_file) {
			return lexx::remove_file(source) == 0 ? FileOpResult::Ok : FileOpResult::IoError;
		}
		if (stat.type != lexx::node_directory) {
			return FileOpResult::Invalid;
		}
		// Always fetch entry 0: deleting each child changes directory offsets.
		unsigned countProcessed = 0;
		while (countProcessed < kEntryLimit) {
			// Skip synthetic '.' / '..' if a filesystem exposes them.
			lexx::uint32_t offset = 0;
			char name[lexx::directory_name_size]{};
			for (;;) {
				auto const count = lexx::list_directory(source, offset, g_entries);
				if (count < 0) {
					return FileOpResult::IoError;
				}
				if (count == 0) {
					return lexx::rmdir(source) == 0 ? FileOpResult::Ok : FileOpResult::IoError;
				}
				for (unsigned j = 0; j < sizeof(name); ++j) {
					name[j] = g_entries[0].name[j];
				}
				if (!equal(name, ".") && !equal(name, "..")) {
					break;
				}
				++offset;
				if (offset > 2U) {
					return FileOpResult::Invalid;
				}
			}
			char child[kMaxPath]{};
			if (!join(child, source, name)) {
				return FileOpResult::Invalid;
			}
			auto const result = deleteItem(child, depth + 1);
			if (result != FileOpResult::Ok) {
				return result;
			}
			++countProcessed;
			lexx::yield();
		}
		return FileOpResult::DepthLimit;
	}

} // namespace

FileOpResult copyPath(char const* source, char const* destination) {
	return copyItem(source, destination, 0);
}

FileOpResult renamePath(char const* source, char const* destination) {
	if (protectedPath(source) || equal(source, destination) || containedIn(destination, source)) {
		return FileOpResult::Invalid;
	}
	lexx::FileStat existing{};
	if (lexx::stat(destination, existing) == 0) {
		return FileOpResult::Exists;
	}
	return lexx::rename_path(source, destination) == 0 ? FileOpResult::Ok : FileOpResult::IoError;
}

FileOpResult movePath(char const* source, char const* destination) {
	if (protectedPath(source) || equal(source, destination) || containedIn(destination, source)) {
		return FileOpResult::Invalid;
	}
	lexx::FileStat existing{};
	if (lexx::stat(destination, existing) == 0) {
		return FileOpResult::Exists;
	}
	int const result = lexx::rename_path(source, destination);
	if (result == 0) {
		return FileOpResult::Ok;
	}
	if (result != -2) {
		return FileOpResult::IoError; // fallback ONLY for cross-FS or cross-directory.
	}
	auto copied = copyItem(source, destination, 0);
	if (copied != FileOpResult::Ok) {
		return copied;
	}
	return deleteItem(source, 0);
}

FileOpResult deletePath(char const* source) {
	return deleteItem(source, 0);
}

char const* describe(FileOpResult result) {
	switch (result) {
		case FileOpResult::Ok        : return "Operation completed";
		case FileOpResult::Exists    : return "Destination already exists";
		case FileOpResult::Missing   : return "Source no longer exists";
		case FileOpResult::Invalid   : return "Path / protected node / depth invalid";
		case FileOpResult::DepthLimit: return "Directory exceeds traversal limit";
		case FileOpResult::IoError   : return "I/O or read-only filesystem error";
	}
	return "Operation failed";
}

} // namespace fileman
