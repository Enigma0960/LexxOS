#pragma once

namespace lexxos::app {

class ScopedConsoleMirrorSuspension {
public:
	ScopedConsoleMirrorSuspension();
	~ScopedConsoleMirrorSuspension();
	ScopedConsoleMirrorSuspension(ScopedConsoleMirrorSuspension const&) = delete;
	ScopedConsoleMirrorSuspension& operator=(ScopedConsoleMirrorSuspension const&) = delete;
};

} // namespace lexxos::app
