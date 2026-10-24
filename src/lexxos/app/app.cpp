#include <lexxos/app/app.h>

#include <lexxos/app/runtime.h>

int lexxos::app::run() {
	static Runtime runtime;
	return runtime.run();
}
