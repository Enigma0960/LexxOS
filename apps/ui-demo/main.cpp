#include <lexx/lexx.h>
#include <lexx/ui/transport.h>

#include "shared_tree_demo.h"

extern "C" [[noreturn]] void _start() {
	lexxos::ui_demo::TreeDemo demo{lexx::ui::guestTransport};
	lexx::exit(demo.run());
}
