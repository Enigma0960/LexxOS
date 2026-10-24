#pragma once

#include <cstddef>
#include <cstring>
#include <string_view>

namespace lexxos::text {

inline std::size_t length(char const* text, std::size_t maxLength = static_cast<std::size_t>(-1)) {
	if (text == nullptr) {
		return 0;
	}

	std::size_t size = 0;
	while (size < maxLength && text[size] != '\0') {
		++size;
	}
	return size;
}

inline std::string_view view(char const* text, std::size_t maxLength = static_cast<std::size_t>(-1)) {
	return text != nullptr ? std::string_view{text, length(text, maxLength)} : std::string_view{};
}

inline int compare(std::string_view left, std::string_view right) {
	auto const count = left.size() < right.size() ? left.size() : right.size();
	if (count != 0) {
		auto const result = std::memcmp(left.data(), right.data(), count);
		if (result < 0) {
			return -1;
		}
		if (result > 0) {
			return 1;
		}
	}

	if (left.size() == right.size()) {
		return 0;
	}
	return left.size() < right.size() ? -1 : 1;
}

inline int compare(char const* left, std::string_view right) {
	return compare(view(left), right);
}

inline int compare(std::string_view left, char const* right) {
	return compare(left, view(right));
}

inline int compare(char const* left, char const* right) {
	return compare(view(left), view(right));
}

inline int comparePrefix(std::string_view left, std::string_view right, std::size_t count) {
	auto const leftCount = left.size() < count ? left.size() : count;
	auto const rightCount = right.size() < count ? right.size() : count;
	auto const commonCount = leftCount < rightCount ? leftCount : rightCount;

	if (commonCount != 0) {
		auto const result = std::memcmp(left.data(), right.data(), commonCount);
		if (result < 0) {
			return -1;
		}
		if (result > 0) {
			return 1;
		}
	}

	if (leftCount == rightCount) {
		return 0;
	}
	return leftCount < rightCount ? -1 : 1;
}

inline bool equals(std::string_view left, std::string_view right) {
	return compare(left, right) == 0;
}

inline bool equals(char const* left, std::string_view right) {
	return equals(view(left), right);
}

inline bool equals(std::string_view left, char const* right) {
	return equals(left, view(right));
}

inline bool equals(char const* left, char const* right) {
	return equals(view(left), view(right));
}

inline char toLowerAscii(char value) {
	return value >= 'A' && value <= 'Z' ? static_cast<char>(value - 'A' + 'a') : value;
}

inline bool equalsIgnoreCaseAscii(std::string_view left, std::string_view right) {
	if (left.size() != right.size()) {
		return false;
	}
	for (std::size_t index = 0; index < left.size(); ++index) {
		if (toLowerAscii(left[index]) != toLowerAscii(right[index])) {
			return false;
		}
	}
	return true;
}

inline bool startsWith(std::string_view value, std::string_view prefix) {
	return value.size() >= prefix.size() && comparePrefix(value, prefix, prefix.size()) == 0;
}

inline bool endsWith(std::string_view value, std::string_view suffix) {
	return value.size() >= suffix.size()
	    && compare(value.substr(value.size() - suffix.size()), suffix) == 0;
}

inline bool contains(std::string_view value, std::string_view needle) {
	if (needle.empty()) {
		return true;
	}
	if (needle.size() > value.size()) {
		return false;
	}

	for (std::size_t index = 0; index + needle.size() <= value.size(); ++index) {
		if (comparePrefix(value.substr(index), needle, needle.size()) == 0) {
			return true;
		}
	}
	return false;
}

inline bool isSpace(char value) {
	return value == ' ' || value == '\t' || value == '\n' || value == '\r' || value == '\f' || value == '\v';
}

inline std::string_view trimLeft(std::string_view value) {
	std::size_t begin = 0;
	while (begin < value.size() && isSpace(value[begin])) {
		++begin;
	}
	return value.substr(begin);
}

inline std::string_view trimRight(std::string_view value) {
	std::size_t end = value.size();
	while (end > 0 && isSpace(value[end - 1])) {
		--end;
	}
	return value.substr(0, end);
}

inline std::string_view trim(std::string_view value) {
	return trimRight(trimLeft(value));
}

inline bool copy(char* output, std::size_t outputCapacity, std::string_view source) {
	if (output == nullptr || outputCapacity == 0 || source.size() + 1 > outputCapacity) {
		return false;
	}
	if (!source.empty()) {
		std::memcpy(output, source.data(), source.size());
	}
	output[source.size()] = '\0';
	return true;
}

inline bool copy(char* output, std::size_t outputCapacity, char const* source) {
	return copy(output, outputCapacity, view(source));
}

inline bool copyRaw(char* output, std::size_t outputCapacity, std::string_view source) {
	if (source.size() > outputCapacity || (output == nullptr && !source.empty())) {
		return false;
	}
	if (!source.empty()) {
		std::memcpy(output, source.data(), source.size());
	}
	return true;
}

} // namespace lexxos::text
