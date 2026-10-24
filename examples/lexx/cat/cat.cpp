#include <lexx/lexx.h>

namespace {

void write_usage() {
	lexx::write_error("usage: cat.lexx <file>\n");
}

int cat_file(char const* path) {
	if (path == nullptr || path[0] == '\0') {
		write_usage();
		return 2;
	}

	auto const fd = lexx::open_read(path);
	if (fd < 0) {
		lexx::write_error("cat.lexx: cannot open: ");
		lexx::write_error(path);
		lexx::write_error("\n");
		return 1;
	}

	char buffer[128];
	while (true) {
		auto const bytes = lexx::read(static_cast<lexx::uint32_t>(fd), buffer, sizeof(buffer));
		if (bytes == 0) {
			break;
		}
		if (bytes == lexx::no_data) {
			lexx::write_error("cat.lexx: read failed\n");
			(void) lexx::close(static_cast<lexx::uint32_t>(fd));
			return 1;
		}
		lexx::write(buffer, bytes);
		lexx::yield();
	}

	if (lexx::close(static_cast<lexx::uint32_t>(fd)) < 0) {
		lexx::write_error("cat.lexx: close failed\n");
		return 1;
	}
	return 0;
}

int main() {
	if (lexx::argc() < 2) {
		write_usage();
		return 2;
	}
	return cat_file(lexx::argument(1));
}

} // namespace

extern "C" [[noreturn]] void _start() {
	lexx::exit(main());
}
