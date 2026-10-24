#pragma once

#include <array>
#include <cstdint>
#include <string_view>

#include <lexxos/io/output_stream.h>
#include <lexxos/vfs/vfs.h>

namespace lexxos::shell::detail {

struct ShellRedirection {
	lexxos::io::InputStream* standardInput{nullptr};
	lexxos::io::OutputStream* standardOutput{nullptr};
	lexxos::io::OutputStream* standardError{nullptr};
	std::array<char, lexxos::vfs::maxPathLength> standardInputPath{};
	std::array<char, lexxos::vfs::maxPathLength> standardOutputPath{};
	std::array<char, lexxos::vfs::maxPathLength> standardErrorPath{};
	lexxos::vfs::WriteMode standardOutputMode{lexxos::vfs::WriteMode::CreateTruncate};
	lexxos::vfs::WriteMode standardErrorMode{lexxos::vfs::WriteMode::CreateTruncate};
	bool standardInputFile{false};
	bool standardOutputFile{false};
	bool standardErrorFile{false};
};

enum class RedirectionParseStatus : std::uint8_t {
	Ok,
	MissingTarget,
	InvalidTarget,
	UnexpectedTarget,
};

struct RedirectionParseResult {
	RedirectionParseStatus status{RedirectionParseStatus::Ok};
	std::string_view token{};
};

} // namespace lexxos::shell::detail
