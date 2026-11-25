#pragma once

#include <lexxos/platform/processor.h>

namespace lexxos::platform {

// Идентичность платы не зависит от ее периферии и состава сборки.
struct BoardDescriptor {
	char const* boardId{"unknown"};
	char const* boardName{"Unknown board"};
	char const* vendor{"unknown"};
	ProcessorFamily processorFamily{ProcessorFamily::Unknown};
	char const* processorName{"unknown"};
};

} // namespace lexxos::platform
