#include "../ui-demo/shared_tree_demo.h"

#include <lexx/lexx.h>

extern "C" [[noreturn]] void _start() {
	lexxos::ui_demo::TreeDemo demo{lexx::ui::guestTransport};
	lexx::exit(demo.run());
}
