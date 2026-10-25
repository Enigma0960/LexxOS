#include <lexxos/apps/fs/shortcut_programs.h>

#include <array>
#include <cstring>
#include <string_view>

#include <lexxos/apps/common/core_programs_support.h>
#include <lexxos/launch/shortcuts.h>
#include <lexxos/os/app_context.h>
#include <lexxos/program/program_supervisor.h>
#include <lexxos/vfs/vfs.h>

namespace lexxos::apps::filesystem_programs {
namespace {
using Path = std::array<char, lexxos::vfs::maxPathLength>;

bool equals(char const* text, std::string_view value) {
    return text != nullptr && std::string_view(text) == value;
}
bool copy(std::string_view text, char* destination, std::size_t capacity) {
    if (text.empty() || text.size() >= capacity
        || text.find('\n') != std::string_view::npos || text.find('\r') != std::string_view::npos)
        return false;
    std::memcpy(destination, text.data(), text.size());
    destination[text.size()] = '\0';
    return true;
}
bool resolve(lexxos::os::AppContext& context, char const* text, Path& path) {
    return text && context.vfs().normalizePath(context.workingDirectory(),
        text, path.data(), path.size());
}
bool isLexx(std::string_view target) {
    return target.size() >= 5 && target.substr(target.size() - 5) == ".lexx";
}
void failed(lexxos::os::AppContext& context, std::string_view command,
    lexxos::program::ProgramSupervisorResult const& result) {
    context.writeError(command);
    context.writeError(": ");
    context.writeErrorLine(lexxos::program::toString(result.status));
}
struct LinkListContext {
    lexxos::os::AppContext& context;
};
bool visitLinks(lexxos::vfs::DirectoryEntry const& entry, void* data) {
    if (entry.type != lexxos::vfs::NodeType::RegularFile
        || !lexxos::launch::hasShortcutSuffix(entry.name)) return true;
    auto& ctx = static_cast<LinkListContext*>(data)->context;
    Path path{};
    constexpr std::string_view prefix = "/sys/autostart/";
    if (prefix.size() + entry.name.size() >= path.size()) return true;
    std::memcpy(path.data(), prefix.data(), prefix.size());
    std::memcpy(path.data() + prefix.size(), entry.name.data(), entry.name.size());
    path[prefix.size() + entry.name.size()] = '\0';
    lexxos::launch::Shortcut link{};
    ctx.write(entry.name);
    ctx.write(": ");
    if (lexxos::launch::loadShortcut(ctx.vfs(), path.data(), link)) {
        ctx.write(link.background ? "bg " : "fg ");
        ctx.write(link.target.data());
        ctx.write(" delay=");
        lexxos::apps::writeUnsigned(ctx, link.delayMs);
        ctx.writeLine(" ms");
    } else ctx.writeLine("INVALID");
    return true;
}
} // namespace

lexxos::program::ProgramDescriptor LinkCreateProgram::descriptor() {
    return {
        .name = "ln",
        .help = "create a portable .lnk shortcut with arguments",
        .description = "Creates a VFS-backed shortcut for a builtin or .lexx program.",
        .usage = "ln [--bg|--fg] [--delay-ms N] [--force] <program> <file.lnk> [-- args...]",
        .handler = run,
        .allowExtraArguments = true,
    };
}
int LinkCreateProgram::run(lexxos::os::AppContext& context,
    lexxos::program::ProgramInvocation const& invocation,
    lexxos::program::ParsedArguments const&) {
    lexxos::launch::Shortcut link{};
    char const* target = nullptr;
    char const* destination = nullptr;
    bool force = false, arguments = false;
    for (int i = 1; i < invocation.argc; ++i) {
        auto const* token = invocation.argv[i];
        if (!arguments && equals(token, "--")) {
            arguments = true;
            continue;
        }
        if (arguments) {
            if (link.argCount >= link.args.size()
                || !copy(token, link.args[link.argCount].data(),
                    link.args[link.argCount].size())) {
                context.writeErrorLine("ln: too many or too long arguments");
                return 2;
            }
            ++link.argCount;
            continue;
        }
        if (equals(token, "--bg")) { link.background = true; continue; }
        if (equals(token, "--fg")) { link.background = false; continue; }
        if (equals(token, "--force")) { force = true; continue; }
        if (equals(token, "--delay-ms")) {
            if (++i >= invocation.argc
                || !lexxos::apps::parseUnsigned32(invocation.argv[i], link.delayMs)
                || link.delayMs > lexxos::launch::maxDelayMs) {
                context.writeErrorLine("ln: invalid --delay-ms (0..600000)");
                return 2;
            }
            continue;
        }
        if (token[0] == '-' && target == nullptr) {
            context.writeErrorLine("ln: unknown option");
            return 2;
        }
        if (target == nullptr) target = token;
        else if (destination == nullptr) destination = token;
        else {
            context.writeErrorLine("ln: unexpected operand (use -- before arguments)");
            return 2;
        }
    }
    if (target == nullptr || destination == nullptr || !lexxos::launch::hasShortcutSuffix(destination)) {
        context.writeErrorLine("usage: ln [--bg|--fg] [--delay-ms N] [--force] <program> <file.lnk> [-- args...]");
        return 2;
    }

    Path destinationPath{}, targetPath{};
    if (!resolve(context, destination, destinationPath)) {
        context.writeErrorLine("ln: invalid destination path");
        return 2;
    }
    std::string_view const targetView{target};
    bool validTarget = false;
    if (isLexx(targetView)) {
        validTarget = resolve(context, target, targetPath)
            && copy(targetPath.data(), link.target.data(), link.target.size());
    } else if (targetView.find('/') == std::string_view::npos
        && !lexxos::launch::hasShortcutSuffix(targetView)) {
        validTarget = copy(targetView, link.target.data(), link.target.size());
    }
    if (!validTarget) {
        context.writeErrorLine("ln: target must be a builtin name or a .lexx path");
        return 2;
    }
    lexxos::vfs::Stat existing{};
    if (!force && context.vfs().stat(destinationPath.data(), existing)) {
        context.writeErrorLine("ln: destination exists (use --force)");
        return 1;
    }
    auto const status = lexxos::launch::saveShortcut(context.vfs(), destinationPath.data(), link);
    if (status != lexxos::vfs::WriteStatus::Ok) {
        context.writeError("ln: ");
        context.writeErrorLine(lexxos::vfs::toString(status));
        return 1;
    }
    context.write("ln: created ");
    context.writeLine(destinationPath.data());
    return 0;
}

lexxos::program::ProgramDescriptor LinkRunProgram::descriptor() {
    return {
        .name = "link-run",
        .help = "launch a .lnk shortcut (also invoked automatically by shell)",
        .usage = "link-run <file.lnk> [additional args...]",
        .handler = run,
        .allowExtraArguments = true,
    };
}
int LinkRunProgram::run(lexxos::os::AppContext& context,
    lexxos::program::ProgramInvocation const& invocation,
    lexxos::program::ParsedArguments const&) {
    if (invocation.argc < 2) {
        context.writeErrorLine("usage: link-run <file.lnk> [additional args...]");
        return 2;
    }
    Path path{};
    if (!resolve(context, invocation.argv[1], path)) {
        context.writeErrorLine("link-run: invalid path");
        return 2;
    }
    lexxos::launch::Shortcut shortcut{};
    if (!lexxos::launch::loadShortcut(context.vfs(), path.data(), shortcut)) {
        context.writeErrorLine("link-run: invalid or unreadable shortcut");
        return 1;
    }
    auto const result = lexxos::launch::launchShortcut(context, shortcut,
        invocation.argc - 2, invocation.argv + 2);
    if (!result.started()) {
        failed(context, "link-run", result);
        return 1;
    }
    return 0;
}

lexxos::program::ProgramDescriptor AutostartProgram::descriptor() {
    return {
        .name = "autostart",
        .help = "configure and inspect startup shortcuts",
        .usage = "autostart [status|list|run|delay <milliseconds>]",
        .handler = run,
        .allowExtraArguments = true,
    };
}
int AutostartProgram::run(lexxos::os::AppContext& context,
    lexxos::program::ProgramInvocation const& invocation,
    lexxos::program::ParsedArguments const&) {
    auto const operation = invocation.argc >= 2 ? std::string_view(invocation.argv[1]) : "status";
    if (operation == "status" && invocation.argc <= 2) {
        context.writeLine("autostart directory: /sys/autostart");
        context.writeLine("persistent directory: /mnt/sd/.lexx/autostart");
        context.writeLine("configuration: /mnt/sd/.lexx/autostart.conf");
        context.write("global delay: ");
        lexxos::apps::writeUnsigned(context, lexxos::launch::readAutostartDelay(context.vfs()));
        context.writeLine(" ms");
        return 0;
    }
    if (operation == "list" && invocation.argc == 2) {
        LinkListContext data{context};
        if (!context.vfs().list("/sys/autostart", visitLinks, &data)) {
            context.writeErrorLine("autostart: cannot list shortcuts");
            return 1;
        }
        return 0;
    }
    if (operation == "delay" && invocation.argc == 3) {
        std::uint32_t delay = 0;
        if (!lexxos::apps::parseUnsigned32(invocation.argv[2], delay)
            || !lexxos::launch::writeAutostartDelay(context.vfs(), delay)) {
            context.writeErrorLine("autostart: invalid delay or SD not writable (0..600000)");
            return 1;
        }
        context.writeLine("autostart: delay saved (applies to next startup)");
        return 0;
    }
    if (operation == "run" && invocation.argc == 2) {
        lexxos::launch::requestAutostartRun();
        context.writeLine("autostart: startup queue restarted");
        return 0;
    }
    context.writeErrorLine("usage: autostart [status|list|run|delay <milliseconds>]");
    return 2;
}
} // namespace lexxos::apps::filesystem_programs
