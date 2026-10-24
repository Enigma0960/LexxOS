#pragma once

#include <cstdint>
#include <string_view>

namespace lexxos::os {
class OperatingSystem;
}

namespace lexxos::app {

enum class ConsoleMirrorMode : std::uint8_t {
	Text,
	Graphics,
};

class ScopedConsoleMirrorMode {
public:
	explicit ScopedConsoleMirrorMode(ConsoleMirrorMode mode);
	~ScopedConsoleMirrorMode();

	ScopedConsoleMirrorMode(ScopedConsoleMirrorMode const&) = delete;
	ScopedConsoleMirrorMode& operator=(ScopedConsoleMirrorMode const&) = delete;
	ScopedConsoleMirrorMode(ScopedConsoleMirrorMode&&) = delete;
	ScopedConsoleMirrorMode& operator=(ScopedConsoleMirrorMode&&) = delete;

	void restore();

private:
	ConsoleMirrorMode _previousMode{ConsoleMirrorMode::Text};
	bool _active{false};
};

class ConsoleMirrorSink {
public:
	virtual ~ConsoleMirrorSink() = default;

	virtual void initialize(lexxos::os::OperatingSystem& operatingSystem) = 0;
	virtual void write(std::string_view text) = 0;
	virtual void flush() = 0;
	virtual void clear() = 0;
	virtual void refresh() = 0;
	virtual void setMode(ConsoleMirrorMode mode) = 0;
	[[nodiscard]] virtual ConsoleMirrorMode mode() const = 0;
	virtual bool update() = 0;
	[[nodiscard]] virtual bool isReady() const = 0;
};

namespace console_mirror {

	[[nodiscard]] ConsoleMirrorSink& sink();

	void initialize(lexxos::os::OperatingSystem& operatingSystem);
	void write(std::string_view text);
	void flush();
	void clear();
	void setMode(ConsoleMirrorMode mode);
	bool update();
	void refresh();

	[[nodiscard]] ConsoleMirrorMode mode();
	[[nodiscard]] bool isReady();

} // namespace console_mirror
} // namespace lexxos::app
