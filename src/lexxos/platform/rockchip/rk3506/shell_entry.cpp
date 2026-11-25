#include <lexxos/app/app.h>

// Called after the existing ARM entry, BSS, C++ constructors, exception
// vectors, GIC and timer are ready. Runtime::run() creates the real OS,
// registers its VFS/programs, then launches the actual shell program as a task.
extern "C" int rk3506_shell_run() {
	return lexxos::app::run();
}
