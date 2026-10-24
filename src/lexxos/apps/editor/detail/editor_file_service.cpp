#include <lexxos/apps/editor/detail/editor_file_service.h>

#include <array>
#include <cstddef>
#include <string_view>

#include <lexxos/os/app_context.h>
#include <lexxos/vfs/vfs.h>

namespace lexxos::apps::text_editor_programs::detail {
namespace {

	constexpr std::size_t readBufferSize = 96;

	bool isEditorLineCharacter(char value) {
		auto const byte = static_cast<unsigned char>(value);
		return value == '\t' || (byte >= 0x20U && byte <= 0x7EU);
	}

	bool finishLoadedLine(lexxos::os::AppContext& context, TextBuffer& buffer, std::string_view path,
	    char* line, std::size_t length) {
		if (buffer.full()) {
			context.write("edit: ");
			context.write(path);
			context.writeLine(": file has too many lines for the editor buffer");
			return false;
		}
		return buffer.appendLine({line, length});
	}

} // namespace

bool EditorFileService::load(lexxos::os::AppContext& context, std::string_view path,
    TextBuffer& buffer, bool& createdNewFile) {
	buffer.clear();
	createdNewFile = false;

	// Open the source or initialize an empty buffer for a new file.
	lexxos::vfs::FileHandle file;
	auto const openStatus = lexxos::vfs::open(path, lexxos::vfs::OpenMode::Read, file);
	if (openStatus == lexxos::vfs::OpenStatus::NotFound) {
		createdNewFile = true;
		return true;
	}
	if (openStatus != lexxos::vfs::OpenStatus::Ok) {
		context.write("edit: ");
		context.write(path);
		context.write(": ");
		context.writeLine(lexxos::vfs::toString(openStatus));
		return false;
	}

	std::array<char, maxEditorLineLength> pendingLine{};
	std::size_t pendingLength = 0;
	bool hasPendingLine = false;

	std::array<char, readBufferSize> readBuffer{};
	// Decode the file incrementally to keep temporary stack usage bounded.
	while (true) {
		std::size_t bytesRead = 0;
		auto const readStatus = file.read(readBuffer.data(), readBuffer.size(), bytesRead);
		if (readStatus != lexxos::vfs::ReadStatus::Ok) {
			context.write("edit: ");
			context.write(path);
			context.write(": ");
			context.writeLine(lexxos::vfs::toString(readStatus));
			return false;
		}
		if (bytesRead == 0) {
			break;
		}

		for (std::size_t index = 0; index < bytesRead; ++index) {
			char const character = readBuffer[index];
			if (character == '\r') {
				continue;
			}
			if (character == '\n') {
				if (!finishLoadedLine(context, buffer, path, pendingLine.data(), pendingLength)) {
					return false;
				}
				pendingLength = 0;
				pendingLine[0] = '\0';
				hasPendingLine = false;
				continue;
			}
			if (!isEditorLineCharacter(character)) {
				context.write("edit: ");
				context.write(path);
				context.writeLine(": file is not plain ASCII text");
				return false;
			}
			if (pendingLength + 1U >= pendingLine.size()) {
				context.write("edit: ");
				context.write(path);
				context.writeLine(": line is too long for the editor buffer");
				return false;
			}
			pendingLine[pendingLength++] = character;
			pendingLine[pendingLength] = '\0';
			hasPendingLine = true;
		}
	}

	// Preserve the final line when the file does not end with a newline.
	if (hasPendingLine && !finishLoadedLine(context, buffer, path, pendingLine.data(), pendingLength)) {
		return false;
	}

	buffer.modified = false;
	return true;
}

bool EditorFileService::save(lexxos::os::AppContext& context, std::string_view path,
    TextBuffer const& buffer) {
	lexxos::vfs::FileHandle file;
	auto const openStatus = lexxos::vfs::open(path, lexxos::vfs::OpenMode::CreateTruncateWrite, file);
	if (openStatus != lexxos::vfs::OpenStatus::Ok) {
		context.write("edit: ");
		context.write(path);
		context.write(": ");
		context.writeLine(lexxos::vfs::toString(openStatus));
		return false;
	}

	for (std::size_t index = 0; index < buffer.lineCount; ++index) {
		auto const line = buffer.line(index);
		auto status = file.write(line);
		if (status == lexxos::vfs::WriteStatus::Ok) {
			status = file.write("\n");
		}
		if (status != lexxos::vfs::WriteStatus::Ok) {
			context.write("edit: ");
			context.write(path);
			context.write(": ");
			context.writeLine(lexxos::vfs::toString(status));
			return false;
		}
	}

	return true;
}

} // namespace lexxos::apps::text_editor_programs::detail
