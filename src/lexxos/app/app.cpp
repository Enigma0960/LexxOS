#include <lexxos/app/app.h>

#include <lexxos/app/runtime.h>

int lexxos::app::run() {
	return run(nullptr);
}

int lexxos::app::run(ProgramRegistrar registerPrograms) {
	static Runtime runtime;
	return runtime.run(registerPrograms);
}
