#include <lexxos/launch/shortcuts.h>

#include <algorithm>
#include <array>
#include <cstring>

#include <lexxos/io/output_stream.h>
#include <lexxos/os/app_context.h>

namespace lexxos::launch {
namespace {
constexpr std::string_view magic = "LEXX-LINK-1";
constexpr std::size_t maxBytes = 1536;
constexpr std::size_t maxStartupEntries = 16;

bool copy(std::string_view from, char* to, std::size_t capacity) {
    if (from.empty() || from.size() >= capacity
        || from.find('\n') != std::string_view::npos
        || from.find('\r') != std::string_view::npos) return false;
    std::memcpy(to, from.data(), from.size());
    to[from.size()] = '\0';
    return true;
}
bool parseDelay(std::string_view text, std::uint32_t& value) {
    if (text.empty()) return false;
    std::uint32_t n = 0;
    for (char c : text) {
        if (c < '0' || c > '9') return false;
        auto const d = static_cast<std::uint32_t>(c - '0');
        if (d > maxDelayMs || n > (maxDelayMs - d) / 10U) return false;
        n = 10U * n + d;
    }
    value = n;
    return true;
}
bool append(std::array<char, maxBytes>& buffer, std::size_t& size, std::string_view chunk) {
    if (chunk.size() > buffer.size() - size) return false;
    std::memcpy(buffer.data() + size, chunk.data(), chunk.size());
    size += chunk.size();
    return true;
}
bool appendDelay(std::array<char, maxBytes>& buffer, std::size_t& size, std::uint32_t n) {
    char digits[12]{};
    std::size_t count = 0;
    do {
        digits[count++] = static_cast<char>('0' + n % 10U);
        n /= 10U;
    } while (n);
    while (count) {
        if (!append(buffer, size, {&digits[--count], 1})) return false;
    }
    return true;
}
bool isLexx(std::string_view target) {
    return target.size() >= 5 && target.substr(target.size() - 5) == ".lexx";
}
bool ensureSettings(lexxos::vfs::Vfs& vfs) {
    if (!vfs.isDirectory("/mnt/sd")) return false;
    auto const status = vfs.createDirectory("/mnt/sd/.lexx");
    return status == lexxos::vfs::WriteStatus::Ok
        || status == lexxos::vfs::WriteStatus::IsDirectory;
}

struct Startup {
    std::array<std::array<char, lexxos::vfs::maxNameLength>, maxStartupEntries> names{};
    std::size_t count{0}, next{0};
    std::uint32_t due{0};
    lexxos::program::ProgramProcessId foreground{0};
    bool initialized{false}, delayArmed{false};
};
Startup g_startup{};

bool collect(lexxos::vfs::DirectoryEntry const& entry, void* userData) {
    auto& state = *static_cast<Startup*>(userData);
    if (entry.type != lexxos::vfs::NodeType::RegularFile || !hasShortcutSuffix(entry.name)
        || state.count >= state.names.size() || entry.name.size() >= lexxos::vfs::maxNameLength) return true;
    auto& name = state.names[state.count++];
    std::memcpy(name.data(), entry.name.data(), entry.name.size());
    name[entry.name.size()] = '\0';
    return true;
}
struct RunningQuery {
    lexxos::program::ProgramProcessId id;
    bool running{false};
};
void match(lexxos::program::ProgramProcessSnapshot const& p, void* userData) {
    auto& query = *static_cast<RunningQuery*>(userData);
    if (p.id == query.id && p.state != lexxos::program::ProgramProcessState::Finished
        && p.state != lexxos::program::ProgramProcessState::Empty) query.running = true;
}
}

bool loadShortcut(lexxos::vfs::Vfs& vfs, std::string_view path, Shortcut& shortcut) {
    shortcut = {};
    if (!hasShortcutSuffix(path)) return false;
    lexxos::vfs::FileHandle file;
    if (vfs.open(path, lexxos::vfs::OpenMode::Read, file) != lexxos::vfs::OpenStatus::Ok
        || file.size() == 0 || file.size() > maxBytes) return false;
    std::array<char, maxBytes> bytes{};
    auto const expected = static_cast<std::size_t>(file.size());
    std::size_t total = 0;
    while (total < expected) {
        std::size_t count = 0;
        if (file.read(bytes.data() + total, expected - total, count) != lexxos::vfs::ReadStatus::Ok
            || count == 0) return false;
        total += count;
    }

    bool first = true, target = false, mode = false, delay = false;
    std::string_view content{bytes.data(), total};
    while (!content.empty()) {
        auto const separator = content.find('\n');
        auto line = content.substr(0, separator);
        content = separator == std::string_view::npos ? std::string_view{} : content.substr(separator + 1);
        if (!line.empty() && line.back() == '\r') line.remove_suffix(1);
        if (first) {
            if (line != magic) return false;
            first = false;
            continue;
        }
        if (line.empty()) continue;
        auto const equal = line.find('=');
        if (equal == std::string_view::npos) return false;
        auto const key = line.substr(0, equal), value = line.substr(equal + 1);
        if (key == "target" && !target) {
            if (!copy(value, shortcut.target.data(), shortcut.target.size())) return false;
            target = true;
        } else if (key == "mode" && !mode) {
            if (value != "fg" && value != "bg") return false;
            shortcut.background = value == "bg";
            mode = true;
        } else if (key == "delay_ms" && !delay) {
            if (!parseDelay(value, shortcut.delayMs)) return false;
            delay = true;
        } else if (key == "arg" && shortcut.argCount < shortcut.args.size()) {
            if (!copy(value, shortcut.args[shortcut.argCount].data(),
                    shortcut.args[shortcut.argCount].size())) return false;
            ++shortcut.argCount;
        } else return false;
    }
    if (!target || !mode || !delay) return false;
    std::string_view const executable = shortcut.target.data();
    return !hasShortcutSuffix(executable)
        && (executable.find('/') == std::string_view::npos
            || (executable.front() == '/' && isLexx(executable)));
}

lexxos::vfs::WriteStatus saveShortcut(lexxos::vfs::Vfs& vfs,
    std::string_view path, Shortcut const& shortcut) {
    if (!hasShortcutSuffix(path) || shortcut.target[0] == '\0'
        || shortcut.argCount > shortcut.args.size() || shortcut.delayMs > maxDelayMs)
        return lexxos::vfs::WriteStatus::InvalidPath;
    std::array<char, maxBytes> buffer{};
    std::size_t size = 0;
    if (!append(buffer, size, magic) || !append(buffer, size, "\ntarget=")
        || !append(buffer, size, shortcut.target.data()) || !append(buffer, size, "\nmode=")
        || !append(buffer, size, shortcut.background ? "bg" : "fg")
        || !append(buffer, size, "\ndelay_ms=") || !appendDelay(buffer, size, shortcut.delayMs)
        || !append(buffer, size, "\n")) return lexxos::vfs::WriteStatus::TooLarge;
    for (std::size_t i = 0; i < shortcut.argCount; ++i) {
        if (!append(buffer, size, "arg=") || !append(buffer, size, shortcut.args[i].data())
            || !append(buffer, size, "\n")) return lexxos::vfs::WriteStatus::TooLarge;
    }
    return vfs.writeText(path, {buffer.data(), size});
}

lexxos::program::ProgramSupervisorResult launchShortcut(lexxos::os::AppContext& context,
    Shortcut const& shortcut, int extraArgc, char const* const* extraArgv) {
    std::array<char const*, lexxos::program::ProgramSupervisor::maxArguments> args{};
    int count = 0;
    if (isLexx(shortcut.target.data())) args[count++] = "lexx-run";
    args[count++] = shortcut.target.data();
    if (extraArgc < 0 || (extraArgc != 0 && extraArgv == nullptr)
        || static_cast<std::size_t>(count + extraArgc) + shortcut.argCount > args.size())
        return {.status = lexxos::program::ProgramSupervisorStatus::ArgumentsTooLarge};
    for (std::size_t i = 0; i < shortcut.argCount; ++i) args[count++] = shortcut.args[i].data();
    for (int i = 0; i < extraArgc; ++i) args[count++] = extraArgv[i];
    lexxos::program::ProgramStartRequest request{
        .name = args[0], .argc = count, .argv = args.data(),
    };
    if (shortcut.background) {
        request.standardOutput = &lexxos::io::nullOutputStream();
        request.standardError = &lexxos::io::nullOutputStream();
        return lexxos::program::supervisor().startBackground(context, request);
    }
    return lexxos::program::supervisor().startForeground(context, request);
}

std::uint32_t readAutostartDelay(lexxos::vfs::Vfs& vfs) {
    lexxos::vfs::FileHandle file;
    if (vfs.open(autostartConfigPath, lexxos::vfs::OpenMode::Read, file)
        != lexxos::vfs::OpenStatus::Ok || file.size() > 64) return 0;
    std::array<char, 65> text{};
    std::size_t count = 0;
    if (file.read(text.data(), 64, count) != lexxos::vfs::ReadStatus::Ok) return 0;
    std::string_view value{text.data(), count};
    constexpr std::string_view key = "delay_ms=";
    if (value.substr(0, key.size()) != key) return 0;
    value.remove_prefix(key.size());
    while (!value.empty() && (value.back() == '\n' || value.back() == '\r')) value.remove_suffix(1);
    std::uint32_t parsed = 0;
    return parseDelay(value, parsed) ? parsed : 0;
}

bool writeAutostartDelay(lexxos::vfs::Vfs& vfs, std::uint32_t milliseconds) {
    if (milliseconds > maxDelayMs || !ensureSettings(vfs)) return false;
    std::array<char, maxBytes> text{};
    std::size_t used = 0;
    return append(text, used, "delay_ms=")
        && appendDelay(text, used, milliseconds) && append(text, used, "\n")
        && vfs.writeText(autostartConfigPath, {text.data(), used}) == lexxos::vfs::WriteStatus::Ok;
}

void requestAutostartRun() { g_startup = {}; }

void pollAutostart(lexxos::os::AppContext& context, std::uint32_t nowMs) {
    if (!g_startup.initialized) {
        if (!context.vfs().isDirectory("/mnt/sd")) return;
        g_startup = {};
        g_startup.initialized = true;
        g_startup.due = nowMs + readAutostartDelay(context.vfs());
        (void) context.vfs().list("/sys/autostart", collect, &g_startup);
        std::sort(g_startup.names.begin(), g_startup.names.begin() + g_startup.count,
            [](auto const& a, auto const& b) { return std::strcmp(a.data(), b.data()) < 0; });
    }
    if (g_startup.foreground != 0) {
        RunningQuery query{.id = g_startup.foreground};
        bool any = false;
        lexxos::program::supervisor().visitProcesses(match, &query, any);
        if (query.running) return;
        g_startup.foreground = 0;
    }
    if (g_startup.next >= g_startup.count
        || static_cast<std::int32_t>(nowMs - g_startup.due) < 0) return;

    std::array<char, lexxos::vfs::maxPathLength> path{};
    auto const name = std::string_view(g_startup.names[g_startup.next].data());
    constexpr std::string_view prefix = "/sys/autostart/";
    if (prefix.size() + name.size() >= path.size()) {
        ++g_startup.next;
        return;
    }
    std::memcpy(path.data(), prefix.data(), prefix.size());
    std::memcpy(path.data() + prefix.size(), name.data(), name.size());
    path[prefix.size() + name.size()] = '\0';
    Shortcut shortcut{};
    if (!loadShortcut(context.vfs(), path.data(), shortcut)) {
        context.writeError("autostart: invalid shortcut: ");
        context.writeErrorLine(name);
        ++g_startup.next;
        return;
    }
    if (!g_startup.delayArmed) {
        g_startup.due = nowMs + shortcut.delayMs;
        g_startup.delayArmed = true;
        return;
    }
    auto const started = launchShortcut(context, shortcut);
    if (!started.started()) {
        context.writeError("autostart: failed ");
        context.writeError(name);
        context.writeError(": ");
        context.writeErrorLine(lexxos::program::toString(started.status));
    } else if (!shortcut.background) {
        g_startup.foreground = started.id;
    }
    ++g_startup.next;
    g_startup.delayArmed = false;
    g_startup.due = nowMs;
}
} // namespace lexxos::launch
