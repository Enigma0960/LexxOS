#include <lexxos/vfs/vfs.h>

#include <array>
#include <string_view>

#include <lexxos/vfs/detail/vfs_string.h>
#include <lexxos/vfs/vfs_internal_types.h>

namespace {

bool isSlash(char value) {
	return value == '/';
}

bool normalizeOne(
    std::string_view base,
    std::string_view input,
    lexxos::vfs::detail::FixedString& output) {
	output.clear();

	if (!output.append('/')) {
		return false;
	}

	std::array<std::string_view, 16> components{};
	std::size_t componentCount = 0;

	auto addComponent = [&](std::string_view component) -> bool {
		if (component.empty() || component == ".") {
			return true;
		}
		if (component == "..") {
			if (componentCount > 0) {
				--componentCount;
			}
			return true;
		}
		if (componentCount >= components.size()) {
			return false;
		}
		components[componentCount++] = component;
		return true;
	};

	auto scan = [&](std::string_view text) -> bool {
		std::size_t start = 0;
		while (start < text.size()) {
			while (start < text.size() && isSlash(text[start])) {
				++start;
			}
			std::size_t end = start;
			while (end < text.size() && !isSlash(text[end])) {
				++end;
			}
			if (!addComponent(text.substr(start, end - start))) {
				return false;
			}
			start = end;
		}
		return true;
	};

	if (!input.empty() && input[0] == '/') {
		if (!scan(input)) {
			return false;
		}
	} else {
		if (!scan(base)) {
			return false;
		}
		if (!scan(input)) {
			return false;
		}
	}

	output.clear();
	if (!output.append('/')) {
		return false;
	}

	for (std::size_t index = 0; index < componentCount; ++index) {
		if (index > 0 && !output.append('/')) {
			return false;
		}
		if (!output.append(components[index])) {
			return false;
		}
	}
	return true;
}

} // namespace

bool lexxos::vfs::Vfs::normalizePath(
    std::string_view cwd,
    std::string_view path,
    char* output,
    std::size_t outputCapacity) {
	detail::FixedString normalized;
	if (!normalizeOne(
	        cwd.empty() ? std::string_view{"/"} : cwd,
	        path.empty() ? std::string_view{"."} : path,
	        normalized)) {
		return false;
	}
	return detail::copyText(normalized.view(), output, outputCapacity);
}

bool lexxos::vfs::normalizePath(
    std::string_view cwd,
    std::string_view path,
    char* output,
    std::size_t outputCapacity) {
	return defaultVfs().normalizePath(cwd, path, output, outputCapacity);
}
