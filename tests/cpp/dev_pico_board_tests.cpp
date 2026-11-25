#include <string_view>

#include <lexxos/hardware/legacy/dev_pico_board.h>

namespace {

constexpr auto board = lexxos::hardware::legacy::devPicoBoard(12000000);
static_assert(std::string_view{board.boardId} == "dev_pico");
static_assert(board.processorFamily == lexxos::platform::ProcessorFamily::Rp2040);
static_assert(board.statusLed.number == 25);
static_assert(board.debugUart.tx.number == 0 && board.debugUart.rx.number == 1);
static_assert(board.debugOledI2c.sda.number == 20 && board.debugOledI2c.scl.number == 21);
static_assert(board.sdSpi.baudRate == 50000000 && board.sdSpi.cs.number == 17);
static_assert(board.sdCardDetect.detect.number == 22);
static_assert(board.psram.spi.cs.number == 9 && board.psram.sio2.number == 13);
static_assert(board.spiTftDisplay.spiBaudRate == 12000000);
static_assert(board.spiTftDisplay.cs.number == 5 && board.spiTftDisplay.width == 128);
static_assert(board.keyboardExpander.address == 0x20 && board.keyboardExpander.interrupt.number == 27);
static_assert(board.keyboard.rowCount == 6 && board.keyboard.columnCount == 4);
static_assert(board.keyboard.rows[0] == 8 && board.keyboard.columns[3] == 3);
static_assert(board.segaGamepad.select.number == 2 && board.segaGamepad.buttonCStart == 15);

} // namespace

int main() {
	return 0;
}
