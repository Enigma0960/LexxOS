#pragma once

#include <string_view>

#include <lexxos/app/console_mirror_types.h>

namespace lexxos::os {

class OperatingSystem;

} // namespace lexxos::os

namespace lexxos::app {

class ConsoleMirrorSink {
public:
	virtual ~ConsoleMirrorSink() = default;

	virtual void initialize(lexxos::os::OperatingSystem& operatingSystem) = 0;
	virtual void write(std::string_view text) = 0;
	virtual void flush() = 0;
	virtual void clear() = 0;
	virtual void refresh() = 0;
	virtual void setMode(ConsoleMirrorMode mode) = 0;
	virtual bool update() = 0;

	[[nodiscard]] virtual ConsoleMirrorMode mode() const = 0;
	[[nodiscard]] virtual bool isReady() const = 0;
};

} // namespace lexxos::app
