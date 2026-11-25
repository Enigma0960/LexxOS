#include <lexxos/shortcuts/shortcut.h>

#include <algorithm>
#include <array>
#include <cstring>

#include <lexxos/io/output_stream.h>
#include <lexxos/os/app_context.h>
#include <lexxos/platform/platform_services.h>
#include <lexxos/program/program_supervisor.h>
#include <lexxos/vfs/detail/vfs_autostart.h>

namespace lexxos::shortcuts {

namespace {

	constexpr std::size_t maxManifestBytes = 2048;
	constexpr std::string_view header = "LEXX-LINK-1";

	bool copy(std::string_view value, char* target, std::size_t capacity) {
		if (value.empty() || value.size() >= capacity || value.find('\r') != value.npos || value.find('\n') != value.npos || value.find('\0') != value.npos) {
			return false;
		}

		std::memcpy(target, value.data(), value.size());
		target[value.size()] = '\0';
		return true;
	}

	// Read a bounded text file without allocating an unbounded string on an MCU.
	template <class F>
	Status lines(vfs::Vfs& vfs, std::string_view path, F&& visitor) {
		lexxos::vfs::FileHandle file;
		auto const result = vfs.open(path, lexxos::vfs::OpenMode::Read, file);
		if (result != lexxos::vfs::OpenStatus::Ok) {
			return result == lexxos::vfs::OpenStatus::NotFound ? Status::NotFound : Status::IoError;
		}

		std::array<char, 128> chunk{};
		std::array<char, 256> line{};
		std::size_t used = 0, total = 0;

		for (;;) {
			std::size_t count = 0;
			if (file.read(chunk.data(), chunk.size(), count) != lexxos::vfs::ReadStatus::Ok) {
				return Status::IoError;
			}

			if (!count) {
				break;
			}

			total += count;
			if (total > maxManifestBytes) {
				return Status::TooLarge;
			}

			for (std::size_t i = 0; i < count; ++i) {
				if (chunk[i] == '\n') {
					if (used && line[used - 1] == '\r') {
						--used;
					}

					if (!visitor(std::string_view(line.data(), used))) {
						return Status::Invalid;
					}

					used = 0;
				} else {
					if (used >= line.size() - 1) {
						return Status::TooLarge;
					}

					line[used++] = chunk[i];
				}
			}
		}

		if (used) {
			if (line[used - 1] == '\r') {
				--used;
			}

			if (!visitor(std::string_view(line.data(), used))) {
				return Status::Invalid;
			}
		}

		return Status::Ok;
	}

	bool write(lexxos::vfs::FileHandle& file, std::string_view data) {
		return file.write(data) == lexxos::vfs::WriteStatus::Ok;
	}

	bool writeNumber(lexxos::vfs::FileHandle& file, std::uint32_t number) {
		char digits[10]{};
		std::size_t count = 0;

		do {
			digits[count++] = static_cast<char>('0' + number % 10);
			number /= 10;
		} while (number);

		for (std::size_t i = 0; i < count / 2; ++i) {
			std::swap(digits[i], digits[count - i - 1]);
		}

		return write(file, {digits, count});
	}

	bool endsWith(std::string_view path, std::string_view ext) {
		return path.size() >= ext.size() && path.substr(path.size() - ext.size()) == ext;
	}

	struct Boot {
		std::array<std::array<char, 64>, 24> names{};
		std::size_t count{0}, next{0};
		Shortcut entry{};
		std::uint32_t due{0};
		bool ready{false};
		bool pending{false};
	};

	Boot g_boot{};

	bool collect(lexxos::vfs::DirectoryEntry const& entry, void* data) {
		auto& boot = *static_cast<Boot*>(data);
		if (entry.type != lexxos::vfs::NodeType::RegularFile || !isShortcut(entry.name)) {
			return true;
		}

		if (boot.count >= boot.names.size()) {
			return true;
		}

		if (!copy(entry.name, boot.names[boot.count].data(), boot.names[boot.count].size())) {
			return true;
		}

		++boot.count;
		return true;
	}

	bool timeReached(std::uint32_t now, std::uint32_t due) {
		return static_cast<std::int32_t>(now - due) >= 0;
	}

} // namespace

bool isShortcut(std::string_view path) {
	return endsWith(path, ".lnk");
}

bool parseDelay(std::string_view text, std::uint32_t& result) {
	if (text.empty()) {
		return false;
	}

	std::uint32_t number = 0;
	for (char digit : text) {
		if (digit < '0' || digit > '9') {
			return false;
		}

		auto const next = static_cast<std::uint32_t>(digit - '0');
		if (number > (maxDelayMs - next) / 10) {
			return false;
		}

		number = number * 10 + next;
	}

	result = number;
	return true;
}

Status load(std::string_view path, Shortcut& result) {
	return load(vfs::defaultVfs(), path, result);
}

Status load(vfs::Vfs& vfs, std::string_view path, Shortcut& result) {
	result = {};
	if (!isShortcut(path)) {
		return Status::Invalid;
	}

	bool first = true, gotTarget = false, gotMode = false, gotDelay = false;
	auto const status = lines(vfs, path, [&](std::string_view line) -> bool {
		if (first) {
			first = false;
			return line == header;
		}

		if (line.substr(0, 7) == "target=") {
			if (gotTarget) {
				return false;
			}

			gotTarget = true;
			return copy(line.substr(7), result.target.data(), result.target.size());
		}

		if (line.substr(0, 5) == "mode=") {
			if (gotMode) {
				return false;
			}

			gotMode = true;
			auto mode = line.substr(5);
			if (mode != "foreground" && mode != "background") {
				return false;
			}

			result.background = mode == "background";
			return true;
		}

		if (line.substr(0, 9) == "delay_ms=") {
			if (gotDelay) {
				return false;
			}

			gotDelay = true;
			return parseDelay(line.substr(9), result.delayMs);
		}

		if (line.substr(0, 4) == "arg=") {
			if (result.argc >= result.args.size()) {
				return false;
			}

			// Empty arguments are representable in the manifest.
			auto const argument = line.substr(4);
			if (argument.size() >= result.args[0].size() || argument.find('\0') != argument.npos) {
				return false;
			}

			std::memcpy(result.args[result.argc].data(), argument.data(), argument.size());
			result.args[result.argc][argument.size()] = '\0';
			++result.argc;
			return true;
		}

		return false;
	});

	if (status != Status::Ok) {
		return status;
	}

	if (!gotTarget || !gotMode || !gotDelay || isShortcut(result.target.data())) {
		return Status::Invalid;
	}

	return Status::Ok;
}

Status save(std::string_view path, Shortcut const& data) {
	if (!isShortcut(path) || !data.target[0] || isShortcut(data.target.data()) || data.argc > data.args.size() || data.delayMs > maxDelayMs) {
		return Status::Invalid;
	}

	// Prevent partial replacement of existing shortcuts. Use rm + create to replace.
	lexxos::vfs::Stat existing{};
	if (lexxos::vfs::stat(path, existing)) {
		return Status::Invalid;
	}

	lexxos::vfs::FileHandle file;
	if (lexxos::vfs::open(path, lexxos::vfs::OpenMode::CreateTruncateWrite, file) != lexxos::vfs::OpenStatus::Ok) {
		return Status::IoError;
	}

	bool ok = write(file, "LEXX-LINK-1\ntarget=") && write(file, data.target.data()) && write(file, "\nmode=") && write(file, data.background ? "background" : "foreground") && write(file, "\ndelay_ms=") && writeNumber(file, data.delayMs) && write(file, "\n");
	for (std::size_t i = 0; ok && i < data.argc; ++i) {
		ok = write(file, "arg=") && write(file, data.args[i].data()) && write(file, "\n");
	}

	auto const finishStatus = file.finish();
	ok = ok && finishStatus == lexxos::vfs::WriteStatus::Ok;

	if (!ok) {
		(void) lexxos::vfs::remove(path);
		return Status::IoError;
	}

	return Status::Ok;
}

Status launch(lexxos::os::AppContext& context, Shortcut const& data, int extraArgc,
    char const* const* extraArgv) {
	if (extraArgc < 0 || (extraArgc && !extraArgv)) {
		return Status::Invalid;
	}

	std::string_view const target = data.target.data();
	bool const external = endsWith(target, ".lexx");
	std::array<char const*, lexxos::program::ProgramSupervisor::maxArguments> argv{};
	std::size_t argc = 0;

	if (external) {
		argv[argc++] = "lexx-run";
	}

	argv[argc++] = data.target.data();
	if (argc + data.argc + static_cast<std::size_t>(extraArgc) > argv.size()) {
		return Status::TooLarge;
	}

	for (std::size_t i = 0; i < data.argc; ++i) {
		argv[argc++] = data.args[i].data();
	}

	for (int i = 0; i < extraArgc; ++i) {
		argv[argc++] = extraArgv[i];
	}

	lexxos::program::ProgramStartRequest request{
	    .name = external ? std::string_view{"lexx-run"} : target,
	    .argc = static_cast<int>(argc),
	    .argv = argv.data(),
	};

	if (data.background) {
		request.standardOutput = &lexxos::io::nullOutputStream();
		request.standardError = &lexxos::io::nullOutputStream();
	}

	auto result = data.background ? lexxos::program::supervisor().startBackground(context, request)
	                              : lexxos::program::supervisor().startForeground(context, request);

	if (!result.started()) {
		context.writeError("link: cannot launch ");
		context.writeError(target);
		context.writeError(": ");
		context.writeErrorLine(lexxos::program::toString(result.status));
		return Status::LaunchError;
	}

	return Status::Ok;
}

Config loadConfig() {
	Config result{};
	auto const status = lines(vfs::defaultVfs(), configPath, [&](std::string_view line) {
		if (line.substr(0, 9) == "delay_ms=") {
			return parseDelay(line.substr(9), result.delayMs);
		}

		if (line == "enabled=1") {
			result.enabled = true;
			return true;
		}

		if (line == "enabled=0") {
			result.enabled = false;
			return true;
		}

		return false;
	});

	if (status != Status::Ok && status != Status::NotFound) {
		result.enabled = false; // fail closed on damaged configuration
	}

	return result;
}

bool saveConfig(Config const& config) {
	if (config.delayMs > maxDelayMs) {
		return false;
	}

	(void) lexxos::vfs::createDirectory("/mnt/sd/.lexx");
	lexxos::vfs::FileHandle file;
	if (lexxos::vfs::open(configPath, lexxos::vfs::OpenMode::CreateTruncateWrite, file) != lexxos::vfs::OpenStatus::Ok) {
		return false;
	}

	bool const written = write(file, config.enabled ? "enabled=1\ndelay_ms=" : "enabled=0\ndelay_ms=")
	    && writeNumber(file, config.delayMs)
	    && write(file, "\n");
	return file.finish() == lexxos::vfs::WriteStatus::Ok && written;
}

void pollBoot(lexxos::os::AppContext& context) {
	auto& boot = g_boot;
	auto const uptime = context.platform().clock.uptimeMs != nullptr ? context.platform().clock.uptimeMs() : 0U;
	if (!boot.ready) {
		lexxos::vfs::Stat mounted{};
		if (!context.vfs().stat("/mnt/sd", mounted) || mounted.type != lexxos::vfs::NodeType::Directory) {
			return; // automount may occur after scheduler startup
		}

		boot.ready = true;
		auto const config = loadConfig();
		if (!config.enabled) {
			return;
		}

		lexxos::vfs::detail::ensureAutostartStorage(context.vfs());
		(void) context.vfs().list("/sys/autostart", collect, &boot);
		std::sort(boot.names.begin(), boot.names.begin() + boot.count,
		    [](auto const& a, auto const& b) { return std::strcmp(a.data(), b.data()) < 0; });
		boot.due = uptime + config.delayMs;
	}

	if (boot.next >= boot.count) {
		return;
	}

	if (lexxos::program::supervisor().focus().programFocused()) {
		return;
	}

	if (!boot.pending) {
		std::array<char, lexxos::vfs::maxPathLength> path{};
		std::string_view const prefix = "/sys/autostart/";
		auto const name = std::string_view(boot.names[boot.next].data());
		if (prefix.size() + name.size() >= path.size()) {
			++boot.next;
			return;
		}

		std::memcpy(path.data(), prefix.data(), prefix.size());
		std::memcpy(path.data() + prefix.size(), name.data(), name.size());
		auto status = load(path.data(), boot.entry);
		if (status != Status::Ok) {
			context.writeError("autostart: skip invalid shortcut: ");
			context.writeErrorLine(name);
			++boot.next;
			return;
		}

		boot.pending = true;
		// Global delay precedes the first per-shortcut delay.
		boot.due += boot.entry.delayMs;
	}

	if (!timeReached(uptime, boot.due)) {
		return;
	}

	auto status = launch(context, boot.entry);
	if (status != Status::Ok) {
		context.writeError("autostart: failed: ");
		context.writeErrorLine(boot.names[boot.next].data());
	}

	++boot.next;
	boot.pending = false;
	boot.due = uptime;
}

std::string_view statusText(Status value) {
	switch (value) {
		case Status::Ok         : return "ok";
		case Status::NotFound   : return "not-found";
		case Status::Invalid    : return "invalid-link";
		case Status::TooLarge   : return "too-large";
		case Status::IoError    : return "io-error";
		case Status::LaunchError: return "launch-error";
	}

	return "unknown";
}

} // namespace lexxos::shortcuts
