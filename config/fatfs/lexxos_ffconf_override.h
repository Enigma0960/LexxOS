#pragma once

// Compiler-forced configuration overlay: use the upstream options unchanged
// except for the optional CLMT support enabled by LexxOS on Lyra.
// Upstream ff.h skips its own "ffconf.h" include when FFCONF_DEF is defined.
#include <ffconf.h>

#ifndef FFCONF_DEF
	#error "FatFs configuration header was not found"
#endif

#if defined(LEXXOS_FATFS_FASTSEEK) && LEXXOS_FATFS_FASTSEEK
	#undef FF_USE_FASTSEEK
	#define FF_USE_FASTSEEK 1
#endif
