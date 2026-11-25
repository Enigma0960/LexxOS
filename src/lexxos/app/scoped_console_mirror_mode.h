#pragma once

#include <lexxos/app/console_mirror_types.h>

namespace lexxos::app {

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

} // namespace lexxos::app
