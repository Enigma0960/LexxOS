#pragma once

namespace lexx {

using uint8_t = unsigned char;
using uint16_t = unsigned short;
using int16_t = signed short;
using uint32_t = unsigned int;
using int32_t = signed int;
using size_t = decltype(sizeof(0));

static_assert(sizeof(uint8_t) == 1, "LexxOS ABI requires 8-bit uint8_t");
static_assert(sizeof(uint16_t) == 2, "LexxOS ABI requires 16-bit uint16_t");
static_assert(sizeof(int16_t) == 2, "LexxOS ABI requires 16-bit int16_t");
static_assert(sizeof(uint32_t) == 4, "LexxOS ABI requires 32-bit uint32_t");
static_assert(sizeof(int32_t) == 4, "LexxOS ABI requires 32-bit int32_t");
static_assert(sizeof(size_t) == 4, "LexxOS guest ABI currently expects 32-bit size_t");

inline constexpr uint32_t capability_console_text = 1U << 0U;
inline constexpr uint32_t capability_keyboard = 1U << 1U;
inline constexpr uint32_t capability_storage_read = 1U << 2U;
inline constexpr uint32_t capability_storage_write = 1U << 3U;
inline constexpr uint32_t capability_devices = 1U << 5U;
inline constexpr uint32_t capability_audio = 1U << 6U;
inline constexpr uint32_t capability_graphics = 1U << 7U;
inline constexpr uint32_t capability_clock = 1U << 8U;

inline constexpr uint32_t syscall_exit = 0;
inline constexpr uint32_t syscall_write = 1;
inline constexpr uint32_t syscall_read_char = 2;
inline constexpr uint32_t syscall_read = 3;
inline constexpr uint32_t syscall_poll_key_event = 4;
inline constexpr uint32_t syscall_yield = 5;
inline constexpr uint32_t syscall_argc = 6;
inline constexpr uint32_t syscall_argv = 7;
inline constexpr uint32_t syscall_open_read = 8;
inline constexpr uint32_t syscall_close = 9;
inline constexpr uint32_t syscall_list_directory = 10;
inline constexpr uint32_t syscall_stat = 11;
inline constexpr uint32_t syscall_seek = 12;
inline constexpr uint32_t syscall_open_write_new = 13;
inline constexpr uint32_t syscall_write_file = 14;
inline constexpr uint32_t syscall_remove_file = 15;
inline constexpr uint32_t syscall_rename_path = 16;
inline constexpr uint32_t syscall_mkdir = 17;
inline constexpr uint32_t syscall_rmdir = 18;
inline constexpr uint32_t syscall_milliseconds = 19;
inline constexpr uint32_t syscall_play_tone = 20;
inline constexpr uint32_t syscall_device_info = 21;
inline constexpr uint32_t syscall_device_open = 22;
inline constexpr uint32_t syscall_device_close = 23;
inline constexpr uint32_t syscall_device_control = 24;
inline constexpr uint32_t syscall_graphics = 25;
inline constexpr uint32_t syscall_unix_time = 26;
inline constexpr uint32_t syscall_memory_info = 27;
inline constexpr uint32_t syscall_sleep_ms = 28;
inline constexpr uint32_t syscall_input_mode = 29;

namespace input {

	enum class Mode : uint32_t {
		Terminal = 0,
		Events = 1
	};

} // namespace lexx::input

inline constexpr uint32_t stdin_fd = 0;
inline constexpr uint32_t stdout_fd = 1;
inline constexpr uint32_t stderr_fd = 2;
inline constexpr uint32_t first_file_fd = 3;
inline constexpr uint32_t max_open_files = 4;
inline constexpr size_t directory_page_size = 8;
inline constexpr size_t directory_name_size = 64;

inline constexpr uint32_t no_data = 0xFFFFFFFFU;
inline constexpr uint32_t cancelled = 0xFFFFFFFEU;
inline constexpr int error = -1;

// mkdir syscall error codes (also returned by the host VM).
inline constexpr int mkdir_failed = -1;
inline constexpr int mkdir_permission_denied = -2;
inline constexpr int mkdir_invalid_path = -3;
inline constexpr int mkdir_not_found = -4;
inline constexpr int mkdir_already_exists = -5;
inline constexpr int mkdir_not_directory = -6;
inline constexpr int mkdir_read_only = -7;
inline constexpr int mkdir_no_space = -8;
inline constexpr int mkdir_too_large = -9;
inline constexpr int mkdir_block_device = -10;

enum class KeyEventType : uint8_t {
	None = 0,
	KeyDown = 1,
	KeyUp = 2,
};

struct KeyEvent {
	uint8_t type{0};
	uint8_t modifiers{0};
	uint8_t character{0};
	uint8_t scan_code{0};
	uint16_t key_code{0};
	uint16_t reserved{0};
	uint32_t timestamp_ms{0};
};

static_assert(sizeof(KeyEvent) == 12, "LexxOS ABI key event must be 12 bytes");

inline constexpr uint8_t node_missing = 0;
inline constexpr uint8_t node_directory = 1;
inline constexpr uint8_t node_regular_file = 2;
inline constexpr uint8_t node_block_device = 3;

// Version-1, little-endian, fixed-width directory ABI. Name is always zero-terminated.
struct DirectoryEntry {
	char name[directory_name_size]{};
	uint32_t size_low{0};
	uint32_t size_high{0};
	uint8_t type{0};
	uint8_t reserved[3]{};
};

struct FileStat {
	uint32_t type{0};
	uint32_t size_low{0};
	uint32_t size_high{0};
};

static_assert(sizeof(DirectoryEntry) == 76, "LEXX directory entry ABI size mismatch");
static_assert(sizeof(FileStat) == 12, "LEXX file stat ABI size mismatch");

namespace detail {

	inline uint32_t ecall0(uint32_t number) {
		register uint32_t a0 asm("a0");
		register uint32_t a7 asm("a7") = number;
		asm volatile("ecall" : "=r"(a0) : "r"(a7) : "memory");
		return a0;
	}

	inline uint32_t ecall1(uint32_t number, uint32_t arg0) {
		register uint32_t a0 asm("a0") = arg0;
		register uint32_t a7 asm("a7") = number;
		asm volatile("ecall" : "+r"(a0) : "r"(a7) : "memory");
		return a0;
	}

	inline uint32_t ecall3(uint32_t number, uint32_t arg0, uint32_t arg1, uint32_t arg2) {
		register uint32_t a0 asm("a0") = arg0;
		register uint32_t a1 asm("a1") = arg1;
		register uint32_t a2 asm("a2") = arg2;
		register uint32_t a7 asm("a7") = number;
		asm volatile("ecall" : "+r"(a0) : "r"(a1), "r"(a2), "r"(a7) : "memory");
		return a0;
	}

	inline size_t cstring_length(char const* text) {
		size_t length = 0;
		if (text == nullptr) {
			return 0;
		}

		while (text[length] != '\0') {
			++length;
		}

		return length;
	}

	inline void clear_key_event(KeyEvent& event) {
		event.type = 0;
		event.modifiers = 0;
		event.character = 0;
		event.scan_code = 0;
		event.key_code = 0;
		event.reserved = 0;
		event.timestamp_ms = 0;
	}

	inline uint16_t key_code_from_character(uint32_t value) {
		switch (value) {
			case 'q':
			case 'Q':
				return 17;
			case 'c':
			case 'C':
				return 3;
			case ' ':
				return 8;
			case '\r':
			case '\n':
				return 5;
			case '\b':
			case 0x7FU:
				return 7;
			default:
				return 0;
		}
	}

} // namespace lexx::detail

inline uint32_t write(uint32_t fd, void const* data, size_t size) {
	return detail::ecall3(
	    syscall_write,
	    fd,
	    reinterpret_cast<uint32_t>(data),
	    static_cast<uint32_t>(size));
}

inline uint32_t write(void const* data, size_t size) {
	return write(stdout_fd, data, size);
}

inline uint32_t write(char const* text) {
	return write(stdout_fd, text, detail::cstring_length(text));
}

inline uint32_t write_error(char const* text) {
	return write(stderr_fd, text, detail::cstring_length(text));
}

inline uint32_t write_error(void const* data, size_t size) {
	return write(stderr_fd, data, size);
}

template <size_t Size>
inline uint32_t write(char const (&text)[Size]) {
	return write(stdout_fd, text, Size > 0 ? Size - 1 : 0);
}

inline uint32_t read(uint32_t fd, void* data, size_t size) {
	return detail::ecall3(
	    syscall_read,
	    fd,
	    reinterpret_cast<uint32_t>(data),
	    static_cast<uint32_t>(size));
}

inline uint32_t read_char() {
	return detail::ecall0(syscall_read_char);
}

// Input mode is private to this .lexx process; the OS restores Shell mode
// when the process exits. Switching modes discards pending console bytes.
namespace input {

	inline bool set_mode(Mode mode) {
		return detail::ecall1(syscall_input_mode, static_cast<uint32_t>(mode)) == 0U;
	}

	inline bool get_mode(Mode& mode) {
		auto const result = detail::ecall1(syscall_input_mode, 2U);
		if (result > 1U) {
			return false;
		}

		mode = static_cast<Mode>(result);
		return true;
	}

} // namespace lexx::input

inline bool poll_key(KeyEvent& event) {
	auto const result = detail::ecall1(syscall_poll_key_event, reinterpret_cast<uint32_t>(&event));
	if (result == 1U) {
		return true;
	}

	if (result == cancelled) {
		return false;
	}

	auto const character = read_char();
	if (character == no_data || character == cancelled || character > 0xFFU) {
		return false;
	}

	detail::clear_key_event(event);
	event.type = static_cast<uint8_t>(KeyEventType::KeyDown);
	event.character = static_cast<uint8_t>(character);
	event.key_code = detail::key_code_from_character(character);
	return true;
}

inline int argc() {
	return static_cast<int>(detail::ecall0(syscall_argc));
}

inline char const* const* argv() {
	return reinterpret_cast<char const* const*>(detail::ecall0(syscall_argv));
}

inline char const* argument(size_t index) {
	auto const count = argc();
	if (index >= static_cast<size_t>(count)) {
		return nullptr;
	}

	auto const arguments = argv();
	return arguments != nullptr ? arguments[index] : nullptr;
}

// Paths beginning with ":/" address the application's own embedded resources.
// They are read-only and do not require the storage-read capability.
// list_directory(":/"), list_directory(":/img"), and stat(":/img") work too.
// Example: open_read(":/img/image.png"). Use read(), seek(), stat(), close() normally.
inline int open_read(char const* path) {
	return static_cast<int>(detail::ecall1(syscall_open_read, reinterpret_cast<uint32_t>(path)));
}

inline int close(uint32_t fd) {
	return static_cast<int>(detail::ecall1(syscall_close, fd));
}

// Returns 0..directory_page_size records; -1 means invalid path/capability/buffer.
// The directory is re-enumerated on each page; mutation during enumeration requires restart.
inline int list_directory(char const* path, uint32_t first_index,
    DirectoryEntry (&entries)[directory_page_size]) {
	return static_cast<int>(detail::ecall3(syscall_list_directory,
	    reinterpret_cast<uint32_t>(path), first_index,
	    reinterpret_cast<uint32_t>(entries)));
}

inline int stat(char const* path, FileStat& result) {
	return static_cast<int>(detail::ecall3(syscall_stat,
	    reinterpret_cast<uint32_t>(path), reinterpret_cast<uint32_t>(&result), 0));
}

// Read-only guest seek: absolute byte position as low/high 32-bit words.
// Returns zero on success and -1 on error (invalid fd, capability, or offset).
inline int seek(uint32_t fd, uint32_t offset_low, uint32_t offset_high = 0) {
	return static_cast<int>(detail::ecall3(syscall_seek, fd, offset_low, offset_high));
}

// All mutation operations require capability_storage_write in the executable.
// open_write_new refuses an existing target; it never deliberately truncates it.
inline int open_write_new(char const* path) {
	return static_cast<int>(detail::ecall1(syscall_open_write_new, reinterpret_cast<uint32_t>(path)));
}

// Returns the byte count on success or -1 on error (including short/no-space writes).
inline int write_file(uint32_t fd, void const* data, size_t size) {
	return static_cast<int>(detail::ecall3(syscall_write_file, fd,
	    reinterpret_cast<uint32_t>(data), static_cast<uint32_t>(size)));
}

inline int remove_file(char const* path) {
	return static_cast<int>(detail::ecall1(syscall_remove_file, reinterpret_cast<uint32_t>(path)));
}

// 0 success, -2 cross-filesystem or unsupported cross-directory rename, -1 error.
inline int rename_path(char const* source, char const* destination) {
	return static_cast<int>(detail::ecall3(syscall_rename_path,
	    reinterpret_cast<uint32_t>(source), reinterpret_cast<uint32_t>(destination), 0));
}

// 0 on success, a negative mkdir_* error on failure.
// recursive=true creates missing parents and accepts an existing directory.
// recursive=false creates only the final component and reports existing paths.
// Requires capability_storage_write. The operation is not atomic.
inline int mkdir(char const* path, bool recursive = true) {
	return static_cast<int>(detail::ecall3(syscall_mkdir,
	    reinterpret_cast<uint32_t>(path), recursive ? 1U : 0U, 0U));
}

inline char const* mkdir_error_message(int status) {
	switch (status) {
		case 0                      : return "ok";
		case mkdir_permission_denied: return "storage write capability denied";
		case mkdir_invalid_path     : return "invalid path";
		case mkdir_not_found        : return "parent directory not found";
		case mkdir_already_exists   : return "path already exists";
		case mkdir_not_directory    : return "path component is not a directory";
		case mkdir_read_only        : return "read-only filesystem";
		case mkdir_no_space         : return "no space left on device";
		case mkdir_too_large        : return "path or directory exceeds filesystem limits";
		case mkdir_block_device     : return "path refers to a block device";
		default                     : return "directory creation failed";
	}
}

// Directory creation result. The underlying mkdir syscall currently reports
// only success or -1; CreateFailed therefore cannot distinguish read-only
// mounts, missing storage-write capability, and backend I/O failures.
enum class MkdirStatus : int {
	Ok = 0,
	InvalidPath = -2,
	PathTooLong = -3,
	NotDirectory = -4,
	CreateFailed = -5,
};

inline char const* mkdir_status_message(MkdirStatus status) {
	switch (status) {
		case MkdirStatus::Ok:
			return "success";
		case MkdirStatus::InvalidPath:
			return "invalid absolute directory path";
		case MkdirStatus::PathTooLong:
			return "directory path exceeds SDK limit";
		case MkdirStatus::NotDirectory:
			return "path component exists but is not a directory";
		case MkdirStatus::CreateFailed:
			return "mkdir failed (capability, read-only mount, or filesystem error)";
	}

	return "unknown directory creation error";
}

// mkdir -p semantics: create missing parents and accept existing directories.
// Requires capability_storage_write if any component must be created.
// On failure, failed_offset identifies the beginning of the failing component
// in the original path (if non-null). No heap allocation is needed.
inline MkdirStatus mkdir_recursive(char const* path, size_t* failed_offset = nullptr) {
	if (failed_offset != nullptr) {
		*failed_offset = 0;
	}

	if (path == nullptr || path[0] != '/' || path[1] == '\0') {
		return MkdirStatus::InvalidPath;
	}

	constexpr size_t max_path_length = 512;
	char current[max_path_length]{};
	size_t length = 0;
	while (path[length] != '\0') {
		if (length + 1 >= max_path_length) {
			return MkdirStatus::PathTooLong;
		}

		current[length] = path[length];
		++length;
	}

	current[length] = '\0';
	auto const ensure_directory = [&](size_t end, size_t start) -> MkdirStatus {
		char const saved = current[end];
		current[end] = '\0';
		FileStat info{};
		if (stat(current, info) == 0) {
			current[end] = saved;
			if (info.type == node_directory) {
				return MkdirStatus::Ok;
			}

			if (failed_offset != nullptr) {
				*failed_offset = start;
			}

			return MkdirStatus::NotDirectory;
		}

		if (mkdir(current) == 0) {
			current[end] = saved;
			return MkdirStatus::Ok;
		}

		// Another task may have created the directory concurrently.
		MkdirStatus const status = stat(current, info) == 0
		    ? (info.type == node_directory ? MkdirStatus::Ok : MkdirStatus::NotDirectory)
		    : MkdirStatus::CreateFailed;
		current[end] = saved;
		if (status != MkdirStatus::Ok && failed_offset != nullptr) {
			*failed_offset = start;
		}

		return status;
	};

	for (size_t index = 1, start = 1; index <= length; ++index) {
		if (index != length && current[index] != '/') {
			continue;
		}

		if (index > start) {
			auto const status = ensure_directory(index, start);
			if (status != MkdirStatus::Ok) {
				return status;
			}
		}

		start = index + 1;
	}

	return MkdirStatus::Ok;
}

inline int rmdir(char const* path) {
	return static_cast<int>(detail::ecall1(syscall_rmdir, reinterpret_cast<uint32_t>(path)));
}

// Monotonic milliseconds since boot (wraps at 2^32 ms); no_data if clock is unavailable.
inline uint32_t milliseconds() {
	return detail::ecall0(syscall_milliseconds);
}

// Cooperative pacing. A single syscall sleeps for at most 100 ms so the
// supervisor can process cancellation without long uninterruptible waits.
inline void sleep_ms(uint32_t duration_ms) {
	(void) detail::ecall1(syscall_sleep_ms, duration_ms);
}

// Play one short stereo tone using the host's portable AudioDevice.
// Requires executable audio capability. Returns false if unsupported/unavailable.
inline bool play_tone(uint32_t frequency_hz, uint32_t duration_ms, uint8_t volume = 128) {
	return detail::ecall3(syscall_play_tone, frequency_hz, duration_ms, volume) == 0U;
}

inline void yield() {
	(void) detail::ecall0(syscall_yield);
}

[[noreturn]] inline void exit(int code) {
	(void) detail::ecall1(syscall_exit, static_cast<uint32_t>(code));
	while (true) {
		yield();
	}
}

} // namespace lexx
