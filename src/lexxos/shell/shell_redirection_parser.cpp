#include <lexxos/shell/shell_redirection_parser.h>

#include <lexxos/io/output_stream.h>
#include <lexxos/vfs/vfs.h>

namespace lexxos::shell::detail {
namespace {

	bool startsWith(std::string_view value, std::string_view prefix) {
		return value.size() >= prefix.size() && value.substr(0, prefix.size()) == prefix;
	}

	bool isNullDevicePath(std::string_view path) {
		return path == "/dev/null" || path == "null";
	}

	bool parseRedirectionToken(
	    std::string_view token,
	    bool& isRedirection,
	    bool& redirectError,
	    bool& redirectStdErr,
	    bool& redirectInput,
	    bool& append,
	    char const*& inlineTarget) {
		isRedirection = false;
		redirectError = false;
		redirectStdErr = false;
		redirectInput = false;
		append = false;
		inlineTarget = nullptr;

		if (token == ">" || token == "1>") {
			isRedirection = true;
			return true;
		}
		if (token == ">>" || token == "1>>") {
			isRedirection = true;
			append = true;
			return true;
		}
		if (token == "2>") {
			isRedirection = true;
			redirectStdErr = true;
			return true;
		}
		if (token == "2>>") {
			isRedirection = true;
			redirectStdErr = true;
			append = true;
			return true;
		}
		if (token == "<" || token == "0<") {
			isRedirection = true;
			redirectInput = true;
			return true;
		}

		if (startsWith(token, ">>") && token.size() > 2) {
			isRedirection = true;
			append = true;
			inlineTarget = token.data() + 2;
			return true;
		}
		if (startsWith(token, ">") && token.size() > 1) {
			isRedirection = true;
			inlineTarget = token.data() + 1;
			return true;
		}
		if (startsWith(token, "1>>") && token.size() > 3) {
			isRedirection = true;
			inlineTarget = token.data() + 3;
			return true;
		}
		if (startsWith(token, "1>") && token.size() > 2) {
			isRedirection = true;
			inlineTarget = token.data() + 2;
			return true;
		}
		if (startsWith(token, "2>>") && token.size() > 3) {
			isRedirection = true;
			redirectStdErr = true;
			append = true;
			inlineTarget = token.data() + 3;
			return true;
		}
		if (startsWith(token, "2>") && token.size() > 2) {
			isRedirection = true;
			redirectStdErr = true;
			inlineTarget = token.data() + 2;
			return true;
		}
		if (startsWith(token, "0<") && token.size() > 2) {
			isRedirection = true;
			redirectInput = true;
			inlineTarget = token.data() + 2;
			return true;
		}
		if (startsWith(token, "<") && token.size() > 1) {
			isRedirection = true;
			redirectInput = true;
			inlineTarget = token.data() + 1;
			return true;
		}

		return true;
	}

} // namespace

RedirectionParseResult applyRedirections(
    std::array<char const*, Shell::maxArguments> const& argv,
    int argc,
    std::array<char const*, Shell::maxArguments>& commandArgv,
    int& commandArgc,
    ShellRedirection& redirection,
    std::string_view workingDirectory) {
	commandArgc = 0;

	for (int index = 0; index < argc; ++index) {
		std::string_view const token{argv[static_cast<std::size_t>(index)]};
		bool isRedirection = false;
		bool redirectError = false;
		bool redirectStdErr = false;
		bool redirectInput = false;
		bool append = false;
		char const* inlineTarget = nullptr;
		if (!parseRedirectionToken(token, isRedirection, redirectError, redirectStdErr, redirectInput, append, inlineTarget)
		    || redirectError) {
			return {.status = RedirectionParseStatus::UnexpectedTarget, .token = token};
		}

		if (!isRedirection) {
			if (commandArgc >= static_cast<int>(commandArgv.size())) {
				return {.status = RedirectionParseStatus::UnexpectedTarget, .token = token};
			}
			commandArgv[static_cast<std::size_t>(commandArgc)] = argv[static_cast<std::size_t>(index)];
			++commandArgc;
			continue;
		}

		char const* target = inlineTarget;
		if (target == nullptr) {
			if (index + 1 >= argc) {
				return {.status = RedirectionParseStatus::MissingTarget, .token = token};
			}
			target = argv[static_cast<std::size_t>(++index)];
		}

		std::string_view const targetPath{target};
		if (isNullDevicePath(targetPath)) {
			if (redirectInput) {
				redirection.standardInput = &lexxos::io::nullInputStream();
			} else if (redirectStdErr) {
				redirection.standardError = &lexxos::io::nullOutputStream();
			} else {
				redirection.standardOutput = &lexxos::io::nullOutputStream();
			}
			continue;
		}

		auto& pathBuffer = redirectInput
		    ? redirection.standardInputPath
		    : (redirectStdErr ? redirection.standardErrorPath : redirection.standardOutputPath);
		if (!lexxos::vfs::normalizePath(workingDirectory, targetPath, pathBuffer.data(), pathBuffer.size())) {
			return {.status = RedirectionParseStatus::InvalidTarget, .token = targetPath};
		}

		if (redirectInput) {
			redirection.standardInputFile = true;
		} else if (redirectStdErr) {
			redirection.standardErrorFile = true;
			redirection.standardErrorMode = append ? lexxos::vfs::WriteMode::Append : lexxos::vfs::WriteMode::CreateTruncate;
		} else {
			redirection.standardOutputFile = true;
			redirection.standardOutputMode = append ? lexxos::vfs::WriteMode::Append : lexxos::vfs::WriteMode::CreateTruncate;
		}
	}

	return {};
}

} // namespace lexxos::shell::detail
