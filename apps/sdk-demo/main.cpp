#include <lexx/devices.h>
#include <lexx/graphics.h>
#include <lexx/lexx.h>
#include <lexx/random.h>
#include <lexx/sprite_player.h>
#include <lexx/system.h>

namespace {

void print_uint(lexx::uint32_t value) {
	char buffer[11]{};
	unsigned count = 0;
	do {
		buffer[count++] = static_cast<char>('0' + value % 10U);
		value /= 10U;
	} while (value != 0 && count < sizeof(buffer));
	while (count != 0) {
		--count;
		(void) lexx::write(&buffer[count], 1);
	}
}

void report(char const* label, lexx::uint32_t value) {
	lexx::write(label);
	print_uint(value);
	lexx::write("\r\n");
}

bool append(char* output, lexx::size_t capacity, lexx::size_t& position, char const* text) {
	while (*text != '\0') {
		if (position + 1U >= capacity) {
			return false;
		}

		output[position++] = *text++;
	}

	output[position] = '\0';
	return true;
}

// Functional VFS test, NOT an SD throughput benchmark. User can supply any
// writable VFS directory; default /tmp is intentionally memory-backed.
void test_vfs(char const* directory) {
	char path[160]{};
	lexx::size_t size = 0;
	if (!append(path, sizeof(path), size, directory)
	    || (size != 0 && path[size - 1] != '/' && !append(path, sizeof(path), size, "/"))
	    || !append(path, sizeof(path), size, "lexx-sdk-demo-")) {
		lexx::write("vfs: invalid test directory\r\n");
		return;
	}

	char digits[11]{};
	lexx::uint32_t stamp = lexx::milliseconds();
	unsigned count = 0;
	do {
		digits[count++] = static_cast<char>('0' + stamp % 10U);
		stamp /= 10U;
	} while (stamp != 0 && count < sizeof(digits));
	while (count != 0) {
		char const c = digits[--count];
		if (size + 1U >= sizeof(path)) {
			lexx::write("vfs: path too long\r\n");
			return;
		}

		path[size++] = c;
	}

	path[size] = '\0';
	if (!append(path, sizeof(path), size, ".tmp")) {
		return;
	}

	int fd = lexx::open_write_new(path);
	if (fd < 0) {
		lexx::write("vfs: SKIP (no writable directory or name collision)\r\n");
		return;
	}

	constexpr char payload[] = "LexxOS SDK v2 smoke test\n";
	bool const written = lexx::write_file(static_cast<lexx::uint32_t>(fd),
	                         payload, sizeof(payload) - 1U)
	    == static_cast<int>(sizeof(payload) - 1U);
	(void) lexx::close(static_cast<lexx::uint32_t>(fd));

	bool verified = false;
	fd = lexx::open_read(path);
	if (fd >= 0) {
		char readback[sizeof(payload)]{};
		int const n = static_cast<int>(lexx::read(static_cast<lexx::uint32_t>(fd),
		    readback, sizeof(payload) - 1U));
		verified = written && n == static_cast<int>(sizeof(payload) - 1U);
		for (lexx::size_t i = 0; verified && i < sizeof(payload) - 1U; ++i) {
			verified = readback[i] == payload[i];
		}

		(void) lexx::close(static_cast<lexx::uint32_t>(fd));
	}

	bool const cleaned = lexx::remove_file(path) == 0;
	lexx::write(verified && cleaned ? "vfs: PASS (write/read/verify/remove)\r\n"
	                                : "vfs: FAIL\r\n");
}

lexx::uint32_t memory[512]{};
lexx::uint32_t volatile checksum = 0;

void benchmark_vm() {
	constexpr lexx::uint32_t iterations = 30000;
	lexx::uint32_t const start = lexx::milliseconds();
	lexx::uint32_t accumulator = 0x1234ABCDU;
	for (lexx::uint32_t i = 0; i < iterations; ++i) {
		accumulator ^= accumulator << 7U;
		accumulator += 0x9E3779B9U;
		memory[i & 511U] = accumulator;
		accumulator ^= memory[(i + 1U) & 511U] >> 3U;
	}

	checksum = accumulator;
	lexx::uint32_t const elapsed = lexx::milliseconds() - start;
	report("vm: iterations=", iterations);
	report("vm: elapsed_ms=", elapsed);
	report("vm: checksum=", checksum);
	if (elapsed != 0U) {
		report("vm: approx_iterations_s=", (iterations * 1000U) / elapsed);
	}

	lexx::uint32_t const calls = 1000U;
	lexx::uint32_t const syscallStart = lexx::milliseconds();
	for (lexx::uint32_t i = 0; i < calls; ++i) {
		checksum = checksum ^ static_cast<lexx::uint32_t>(lexx::argc());
	}

	report("ecall: calls=", calls);
	report("ecall: elapsed_ms=", lexx::milliseconds() - syscallStart);
}

void test_graphics() {
	lexx::graphics::DisplayInfo info{};
	int const query = lexx::graphics::get_info(info);
	if (query != 0) {
		lexx::write("graphics: SKIP (unavailable or denied)\r\n");
		return;
	}

	report("graphics: width=", info.width);
	report("graphics: height=", info.height);
	if (info.pixel_format != 1U || info.width < 24U || info.height < 16U) {
		lexx::write("graphics: SKIP (unsupported pixel format or size)\r\n");
		return;
	}

	if (lexx::graphics::set_mode(lexx::graphics::Mode::Graphics) != 0) {
		lexx::write("graphics: SKIP (not foreground or busy)\r\n");
		return;
	}

	int const tile = (info.width - 8U) / 3U < 42U
	    ? static_cast<int>((info.width - 8U) / 3U)
	    : 42;
	int const tileHeight = info.height < tile + 8
	    ? static_cast<int>(info.height - 8U)
	    : tile;
	lexx::graphics::Command commands[] = {
	    {   static_cast<lexx::uint32_t>(lexx::graphics::CommandType::Clear),            0, 0,    0,          0, 0x0000},
	    {static_cast<lexx::uint32_t>(lexx::graphics::CommandType::FillRect),            2, 8, tile, tileHeight, 0xF800},
	    {static_cast<lexx::uint32_t>(lexx::graphics::CommandType::FillRect),     4 + tile, 8, tile, tileHeight, 0x07E0},
	    {static_cast<lexx::uint32_t>(lexx::graphics::CommandType::FillRect), 6 + 2 * tile, 8, tile, tileHeight, 0x001F},
	};
	bool ok = true;
	lexx::uint32_t const start = lexx::milliseconds();
	constexpr lexx::uint32_t frames = 16;
	for (lexx::uint32_t i = 0; i < frames; ++i) {
		ok = lexx::graphics::submit(commands, sizeof(commands) / sizeof(commands[0])) == 0;
		if (!ok) {
			break;
		}

		lexx::yield();
	}

	lexx::uint32_t const duration = lexx::milliseconds() - start;
	(void) lexx::graphics::set_mode(lexx::graphics::Mode::Text);
	lexx::write(ok ? "graphics: PASS\r\n" : "graphics: FAIL\r\n");
	report("graphics: frames=", ok ? frames : 0U);
	report("graphics: elapsed_ms=", duration);
	if (duration != 0U) {
		report("graphics: approx_fps=", (frames * 1000U) / duration);
	}
}

// Ten-second resource-driven screen saver. Both images are loaded only once
// by the host, not reread from removable media for each frame.
void append_decimal(char* out, lexx::uint32_t& position, lexx::uint32_t value) {
	char digits[11]{};
	lexx::uint32_t count = 0;

	do {
		digits[count++] = static_cast<char>('0' + value % 10U);
		value /= 10U;
	} while (value != 0U);

	while (count != 0U) {
		out[position++] = digits[--count];
	}

	out[position] = 0;
}

bool screensaver() {
	lexx::graphics::DisplayInfo info{};
	if (lexx::graphics::get_info(info) != 0 || info.pixel_format != 1U || info.width < 124U || info.height < 70U) {
		lexx::write("screensaver: SKIP (display too small or unavailable)\r\n");
		return false;
	}

	int const logo = lexx::graphics::load_image(":/img/lexxos.lxim");
	int const banner = lexx::graphics::load_image(":/img/banner.lxim");
	if (logo <= 0 || banner <= 0) {
		if (logo > 0) {
			(void) lexx::graphics::unload_image(static_cast<lexx::uint32_t>(logo));
		}

		if (banner > 0) {
			(void) lexx::graphics::unload_image(static_cast<lexx::uint32_t>(banner));
		}

		lexx::write("screensaver: FAIL (cannot decode embedded images)\r\n");
		return false;
	}

	if (lexx::graphics::set_mode(lexx::graphics::Mode::Graphics) != 0) {
		(void) lexx::graphics::unload_image(static_cast<lexx::uint32_t>(logo));
		(void) lexx::graphics::unload_image(static_cast<lexx::uint32_t>(banner));
		lexx::write("screensaver: SKIP (not foreground or busy)\r\n");
		return false;
	}

	constexpr int imageWidth = 124;
	constexpr int imageHeight = 34;
	constexpr int hudHeight = 20;
	constexpr lexx::uint32_t durationMs = 10000U;
	constexpr lexx::uint32_t framePeriodMs = 40U; // 25 FPS ceiling
	constexpr lexx::uint32_t scaleOptions[] = {1U, 2U, 4U, 8U, 16U};
	lexx::uint32_t availableScales = 0;
	for (lexx::uint32_t scale : scaleOptions) {
		if (static_cast<lexx::uint32_t>(imageWidth) * scale > info.width
		    || static_cast<lexx::uint32_t>(imageHeight) * scale > info.height - hudHeight) {
			break;
		}

		++availableScales;
	}

	lexx::graphics::Command setup[6]{};
	setup[0] = {static_cast<lexx::uint32_t>(lexx::graphics::CommandType::Clear), 0, 0, 0, 0, 0x0000};
	lexx::uint32_t setupCount = 1;
	bool ok = true;
	for (int x = 0; x < info.width; x += 64) {
		if (setupCount == 6) {
			if (lexx::graphics::submit(setup, setupCount) != 0) {
				ok = false;
				break;
			}

			setupCount = 0;
		}

		setup[setupCount++] = {static_cast<lexx::uint32_t>(lexx::graphics::CommandType::BlitImage),
		    x, 0, banner, 0, 0};
	}

	if (ok && setupCount != 0U) {
		ok = lexx::graphics::submit(setup, setupCount) == 0;
	}

	lexx::uint32_t const start = lexx::milliseconds();
	if (start == lexx::no_data) {
		ok = false;
	}

	int x = 0;
	int y = hudHeight + 2;
	int dx = static_cast<int>(info.width) == imageWidth ? 0 : 3;
	int dy = 2;
	int oldX = 0;
	int oldY = 0;
	int oldWidth = imageWidth;
	int oldHeight = imageHeight;
	bool haveOld = false;
	lexx::uint32_t scaleIndex = 0;
	lexx::uint32_t lastScaleSecond = 0;
	lexx::random::Generator random{};
	random.seed(start);

	lexx::uint32_t frames = 0;
	lexx::uint32_t elapsed = 0;

	char hud[32]{};
	while (ok && elapsed < durationMs) {
		// Select a different fitting scale once per second. No guest-side resampling.
		lexx::uint32_t const second = elapsed / 1000U;
		if (second != lastScaleSecond) {
			lastScaleSecond = second;
			if (availableScales > 1U) {
				lexx::uint32_t const candidate = random.range(0U, availableScales - 2U);
				scaleIndex = candidate >= scaleIndex ? candidate + 1U : candidate;
			}
		}

		lexx::uint32_t const scale = scaleOptions[scaleIndex];
		int const scaledWidth = imageWidth * static_cast<int>(scale);
		int const scaledHeight = imageHeight * static_cast<int>(scale);
		int const maxX = static_cast<int>(info.width) - scaledWidth;
		int const maxY = static_cast<int>(info.height) - scaledHeight;
		if (x > maxX) {
			x = maxX;
		}

		if (y > maxY) {
			y = maxY;
		}

		// Build the HUD in guest memory; host text command rasterizes its glyphs.
		lexx::uint32_t pos = 0;
		hud[pos++] = 'F';
		hud[pos++] = 'P';
		hud[pos++] = 'S';
		hud[pos++] = ':';
		append_decimal(hud, pos, elapsed == 0U ? 0U : (frames * 1000U) / elapsed);
		hud[pos++] = ' ';
		hud[pos++] = 'T';
		hud[pos++] = ':';
		append_decimal(hud, pos, elapsed / 1000U);
		hud[pos++] = '.';
		append_decimal(hud, pos, (elapsed % 1000U) / 100U);
		hud[pos++] = 'S';
		hud[pos++] = ' ';
		hud[pos++] = 'X';
		append_decimal(hud, pos, scale);
		hud[pos] = 0;

		lexx::graphics::Command commands[4]{};
		lexx::uint32_t n = 0;
		if (haveOld) {
			commands[n++] = {static_cast<lexx::uint32_t>(lexx::graphics::CommandType::FillRect),
			    oldX, oldY, oldWidth, oldHeight, 0x0000};
		}

		commands[n++] = lexx::graphics::sprite_image_scaled(
		    static_cast<lexx::uint32_t>(logo), x, y, scale);
		commands[n++] = {static_cast<lexx::uint32_t>(lexx::graphics::CommandType::Text),
		    3, 4, static_cast<lexx::int32_t>(reinterpret_cast<lexx::uint32_t>(hud)),
		    1, (0x0821U << 16U) | 0xFFFFU};
		ok = lexx::graphics::submit(commands, n) == 0;
		if (!ok) {
			break;
		}

		++frames;
		haveOld = true;
		oldX = x;
		oldY = y;
		oldWidth = scaledWidth;
		oldHeight = scaledHeight;

		x += dx;
		y += dy;
		if (x < 0 || x > maxX) {
			dx = -dx;
			x = x < 0 ? -x : 2 * maxX - x;
			if (x < 0) {
				x = 0;
			}

			if (x > maxX) {
				x = maxX;
			}
		}

		if (y < hudHeight || y > maxY) {
			dy = -dy;
			y = y < hudHeight ? 2 * hudHeight - y : 2 * maxY - y;
		}

		lexx::uint32_t const now = lexx::milliseconds();
		if (now == lexx::no_data) {
			ok = false;
			break;
		}

		elapsed = now - start;
		if (elapsed >= durationMs) {
			break;
		}

		lexx::uint32_t const target = frames * framePeriodMs;
		if (target > elapsed) {
			lexx::sleep_ms(target - elapsed);
		}

		elapsed = lexx::milliseconds() - start;
	}

	(void) lexx::graphics::set_mode(lexx::graphics::Mode::Text);
	(void) lexx::graphics::unload_image(static_cast<lexx::uint32_t>(banner));
	(void) lexx::graphics::unload_image(static_cast<lexx::uint32_t>(logo));

	lexx::write(ok ? "screensaver: PASS\r\n" : "screensaver: FAIL\r\n");
	report("screensaver: frames=", frames);
	report("screensaver: elapsed_ms=", elapsed);

	if (elapsed > 0U) {
		report("screensaver: fps_avg=", (frames * 1000U) / elapsed);
	}

	return ok;
}

// Optional interactive benchmark: compare a full-screen rewrite to sparse
// updates using the *same* sprite, HUD and number of frames. No SD reads in
// the loop. The wait_idle() barrier includes the final pending DMA transfer.
void benchmark_graphics_roi() {
	lexx::graphics::DisplayInfo info{};
	if (lexx::graphics::get_info(info) != 0 || info.pixel_format != 1U || info.width < 180U || info.height < 100U) {
		lexx::write("roi-bench: SKIP (no compatible display)\r\n");
		return;
	}

	int const logo = lexx::graphics::load_image(":/img/lexxos.lxim");
	if (logo <= 0) {
		lexx::write("roi-bench: FAIL (image unavailable)\r\n");
		return;
	}

	if (lexx::graphics::set_mode(lexx::graphics::Mode::Graphics) != 0) {
		(void) lexx::graphics::unload_image(static_cast<lexx::uint32_t>(logo));
		lexx::write("roi-bench: SKIP (display busy)\r\n");
		return;
	}

	constexpr lexx::uint32_t frames = 48;
	int const range = static_cast<int>(info.width) - 125;
	int const y = static_cast<int>(info.height / 2U);
	lexx::uint32_t fullMs = 0;
	lexx::uint32_t sparseMs = 0;

	auto const measure = [&](bool full, lexx::uint32_t& duration) {
		lexx::uint32_t const started = lexx::milliseconds();
		if (started == lexx::no_data) {
			return false;
		}

		int previousX = 0;
		for (lexx::uint32_t frame = 0; frame < frames; ++frame) {
			int const x = 1 + static_cast<int>((frame * 3U) % static_cast<lexx::uint32_t>(range));
			lexx::graphics::Command commands[3]{};
			commands[0] = full || frame == 0U
			    ? lexx::graphics::Command{static_cast<lexx::uint32_t>(lexx::graphics::CommandType::Clear), 0, 0, 0, 0, 0x0000}
			    : lexx::graphics::Command{static_cast<lexx::uint32_t>(lexx::graphics::CommandType::FillRect), previousX, y, 124, 34, 0x0000};
			commands[1] = {static_cast<lexx::uint32_t>(lexx::graphics::CommandType::BlitImage), x, y, logo, 0, 0};
			commands[2] = {static_cast<lexx::uint32_t>(lexx::graphics::CommandType::Text),
			    3, 4, static_cast<lexx::int32_t>(reinterpret_cast<lexx::uint32_t>("FPS:25")),
			    1, (0x0000U << 16U) | 0xFFFFU};
			if (lexx::graphics::submit(commands, 3) != 0) {
				return false;
			}

			previousX = x;
		}

		if (lexx::graphics::wait_idle() != 0) {
			return false;
		}

		duration = lexx::milliseconds() - started;
		return true;
	};

	bool const ok = measure(true, fullMs) && measure(false, sparseMs);
	(void) lexx::graphics::set_mode(lexx::graphics::Mode::Text);
	(void) lexx::graphics::unload_image(static_cast<lexx::uint32_t>(logo));
	lexx::write(ok ? "roi-bench: PASS\r\n" : "roi-bench: FAIL\r\n");

	if (ok) {
		report("roi-bench: full_ms=", fullMs);
		report("roi-bench: sparse_ms=", sparseMs);
		report("roi-bench: frames_per_case=", frames);
	}
}

// Alpha8 sprite animation: four 24x24 frames in a 96x24 LXIM atlas.
// Each frame renders only the old and new sprite rectangle; time-based frame
// selection stays in the portable SDK, while blending stays in the host OS.
void test_sprite_animation() {
	lexx::graphics::DisplayInfo info{};
	if (lexx::graphics::get_info(info) != 0 || info.pixel_format != 1U || info.width < 24U || info.height < 24U) {
		lexx::write("sprite: SKIP (display unavailable)\r\n");
		return;
	}

	int const handle = lexx::graphics::load_image(":/img/orb.lxim");
	if (handle <= 0) {
		lexx::write("sprite: FAIL (atlas unavailable)\r\n");
		return;
	}

	if (lexx::graphics::set_mode(lexx::graphics::Mode::Graphics) != 0) {
		(void) lexx::graphics::unload_image(static_cast<lexx::uint32_t>(handle));
		lexx::write("sprite: SKIP (graphics busy)\r\n");
		return;
	}

	constexpr lexx::uint32_t durationMs = 5000U;
	constexpr lexx::uint32_t periodMs = 40U;

	constexpr int side = 24;
	constexpr lexx::uint32_t handleCount = 7;
	constexpr lexx::uint32_t background = 0x18E3U;

	lexx::graphics::SpriteSheet sheet{
	    static_cast<lexx::uint32_t>(handle), 168U, 24U, 24U, 24U};
	lexx::graphics::AnimationClip clip{
	    0U, handleCount, 90U, lexx::graphics::PlaybackMode::Loop};
	lexx::graphics::SpritePlayer animation{sheet, clip};

	lexx::graphics::Command clear{static_cast<lexx::uint32_t>(lexx::graphics::CommandType::Clear),
	    0, 0, 0, 0, background};
	bool ok = lexx::graphics::submit(&clear, 1) == 0;
	lexx::uint32_t const started = lexx::milliseconds();
	animation.start(started);
	lexx::uint32_t frames = 0U;
	lexx::uint32_t elapsed = 0U;

	int x = 0, y = 20, dx = 3, dy = 2;
	if (info.width <= 26U) {
		dx = info.width == 24U ? 0 : 1;
	}

	if (info.height <= 44U) {
		y = 0;
		dy = info.height == 24U ? 0 : 1;
	}

	while (ok && elapsed < durationMs) {
		lexx::graphics::Command commands[2]{};
		lexx::uint32_t count = 0;

		int nextX = x + dx, nextY = y + dy;
		if (nextX < 0 || nextX > static_cast<int>(info.width) - side) {
			dx = -dx;
			nextX = x + dx;
		}

		if (nextY < 0 || nextY > static_cast<int>(info.height) - side) {
			dy = -dy;
			nextY = y + dy;
		}

		x = nextX;
		y = nextY;
		ok = animation.append(commands, 2U, count, x, y, started + elapsed, background);
		if (ok && count != 0U) {
			ok = lexx::graphics::submit(commands, count) == 0;
			if (ok) {
				animation.committed();
			} else {
				animation.cancelled();
			}
		}

		if (!ok) {
			break;
		}

		++frames;
		lexx::uint32_t const now = lexx::milliseconds();
		if (now == lexx::no_data) {
			ok = false;
			break;
		}

		elapsed = now - started;
		if (elapsed >= durationMs) {
			break;
		}

		lexx::uint32_t const target = frames * periodMs;
		if (target > elapsed) {
			lexx::sleep_ms(target - elapsed);
		}

		elapsed = lexx::milliseconds() - started;
	}

	if (ok) {
		ok = lexx::graphics::wait_idle() == 0;
	}

	(void) lexx::graphics::set_mode(lexx::graphics::Mode::Text);
	(void) lexx::graphics::unload_image(static_cast<lexx::uint32_t>(handle));
	lexx::write(ok ? "sprite: PASS\r\n" : "sprite: FAIL\r\n");
	report("sprite: frames=", frames);
	report("sprite: elapsed_ms=", elapsed);
	if (elapsed != 0U) {
		report("sprite: fps_avg=", (frames * 1000U) / elapsed);
	}
}

bool once = false;
char const* testDirectory = "/tmp";

int run() {
	for (int i = 1; i < lexx::argc(); ++i) {
		char const* arg = lexx::argument(static_cast<lexx::size_t>(i));
		if (arg != nullptr && arg[0] == '-' && arg[1] == '-' && arg[2] == 'o' && arg[3] == 'n' && arg[4] == 'c' && arg[5] == 'e' && arg[6] == '\0') {
			once = true;
		} else if (arg != nullptr && arg[0] == '/') {
			testDirectory = arg;
		}
	}

	lexx::write("LexxOS SDK v2 diagnostic demo\r\n");
	lexx::system::MemoryInfo mem{};
	if (lexx::system::memory_info(mem)) {
		report("memory: guest_bytes=", mem.guest_bytes);
		report("memory: stack_bytes=", mem.stack_bytes);
	}

	lexx::uint32_t utc = 0;
	if (lexx::system::unix_time(utc)) {
		report("rtc: utc_seconds=", utc);
	} else {
		lexx::write("rtc: unavailable or not synchronized\r\n");
	}

	lexx::DeviceInfo device{};
	int const deviceStatus = lexx::device_info(lexx::DeviceKind::Gpio, 0, device);
	lexx::write(deviceStatus == 0 ? "devices: GPIO registered\r\n"
	                              : "devices: GPIO unavailable\r\n");
	lexx::write(lexx::play_tone(880, 75, 96) ? "audio: tone accepted\r\n"
	                                         : "audio: unavailable\r\n");
	test_vfs(testDirectory);
	benchmark_vm();
	test_graphics();
	(void) screensaver();
	if (once) {
		return 0;
	}

	lexx::write("Keyboard: B=bench G=graphics S=screensaver P=ROI A=alpha sprite Q=exit\r\n");
	for (;;) {
		lexx::KeyEvent key{};
		if (lexx::poll_key(key) && key.type == static_cast<lexx::uint8_t>(lexx::KeyEventType::KeyDown)) {
			if (key.character == 'q' || key.character == 'Q') {
				break;
			}

			if (key.character == 'b' || key.character == 'B') {
				benchmark_vm();
			}

			if (key.character == 'g' || key.character == 'G') {
				test_graphics();
			}

			if (key.character == 's' || key.character == 'S') {
				(void) screensaver();
			}

			if (key.character == 'p' || key.character == 'P') {
				benchmark_graphics_roi();
			}

			if (key.character == 'a' || key.character == 'A') {
				test_sprite_animation();
			}
		}

		lexx::yield();
	}

	return 0;
}

} // namespace

extern "C" [[noreturn]] void _start() {
	lexx::exit(run());
}
