#include <lexxos/devices/espidf/usb_hid_host.h>

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string_view>

#include <esp_err.h>
#include <esp_idf_version.h>
#include <esp_intr_alloc.h>
#include <esp_log.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#include <usb/hid_host.h>
#include <usb/usb_host.h>

#include <lexxos/devices/gamepad_device.h>
#include <lexxos/devices/input_device.h>
#include <lexxos/devices/pointing_device.h>

namespace lexxos::devices::espidf_usb_hid {
namespace {

	constexpr char tag[] = "lexxos-usb-hid";
	constexpr std::size_t keyboardReportKeyCount = 6;
	constexpr std::size_t keyboardEventCapacity = 64;
	constexpr std::size_t hidLocalUsageCapacity = 16;
	constexpr std::size_t hidReportOffsetCapacity = 8;
	constexpr std::size_t gamepadButtonFieldCapacity = 16;
	constexpr std::size_t hidReportBufferBytes = 64;
	constexpr std::uint32_t usbHostStackBytes = 4096;
	constexpr std::uint32_t hidEventStackBytes = 4096;
	constexpr std::uint32_t initializationTimeoutMs = 3000;

	constexpr std::uint8_t hidLeftControl = 1U << 0U;
	constexpr std::uint8_t hidLeftShift = 1U << 1U;
	constexpr std::uint8_t hidLeftAlt = 1U << 2U;
	constexpr std::uint8_t hidRightControl = 1U << 4U;
	constexpr std::uint8_t hidRightShift = 1U << 5U;
	constexpr std::uint8_t hidRightAlt = 1U << 6U;

	constexpr std::uint16_t hidUsagePageGenericDesktop = 0x01;
	constexpr std::uint16_t hidUsagePageButton = 0x09;
	constexpr std::uint16_t hidUsageX = 0x30;
	constexpr std::uint16_t hidUsageY = 0x31;
	constexpr std::uint16_t hidUsageHatSwitch = 0x39;

	std::uint32_t uptimeMs() {
		return static_cast<std::uint32_t>(esp_timer_get_time() / 1000LL);
	}

	KeyCode mapHidKey(std::uint8_t usage) {
		static constexpr std::array<KeyCode, 26> alphaKeys{
		    KeyCode::A,
		    KeyCode::B,
		    KeyCode::C,
		    KeyCode::D,
		    KeyCode::E,
		    KeyCode::F,
		    KeyCode::G,
		    KeyCode::H,
		    KeyCode::I,
		    KeyCode::J,
		    KeyCode::K,
		    KeyCode::L,
		    KeyCode::M,
		    KeyCode::N,
		    KeyCode::O,
		    KeyCode::P,
		    KeyCode::Q,
		    KeyCode::R,
		    KeyCode::S,
		    KeyCode::T,
		    KeyCode::U,
		    KeyCode::V,
		    KeyCode::W,
		    KeyCode::X,
		    KeyCode::Y,
		    KeyCode::Z,
		};
		if (usage >= 0x04U && usage <= 0x1DU) {
			return alphaKeys[usage - 0x04U];
		}
		if (usage >= 0x1EU && usage <= 0x26U) {
			return static_cast<KeyCode>(static_cast<std::uint16_t>(KeyCode::Num1) + (usage - 0x1EU));
		}
		if (usage == 0x27U) {
			return KeyCode::Num0;
		}
		switch (usage) {
			case 0x28: return KeyCode::Enter;
			case 0x29: return KeyCode::Escape;
			case 0x2A: return KeyCode::Backspace;
			case 0x2B: return KeyCode::Tab;
			case 0x2C: return KeyCode::Space;
			case 0x2D: return KeyCode::Minus;
			case 0x2E: return KeyCode::Equals;
			case 0x2F: return KeyCode::LeftBracket;
			case 0x30: return KeyCode::RightBracket;
			case 0x31: return KeyCode::Backslash;
			case 0x33: return KeyCode::Semicolon;
			case 0x34: return KeyCode::Apostrophe;
			case 0x35: return KeyCode::Grave;
			case 0x36: return KeyCode::Comma;
			case 0x37: return KeyCode::Period;
			case 0x38: return KeyCode::Slash;
			case 0x3A: return KeyCode::F1;
			case 0x3B: return KeyCode::F2;
			case 0x3C: return KeyCode::F3;
			case 0x3D: return KeyCode::F4;
			case 0x3E: return KeyCode::F5;
			case 0x3F: return KeyCode::F6;
			case 0x40: return KeyCode::F7;
			case 0x41: return KeyCode::F8;
			case 0x42: return KeyCode::F9;
			case 0x43: return KeyCode::F10;
			case 0x48: return KeyCode::Break;
			case 0x49: return KeyCode::Insert;
			case 0x4A: return KeyCode::Home;
			case 0x4C: return KeyCode::Delete;
			case 0x4D: return KeyCode::End;
			case 0x4F: return KeyCode::Right;
			case 0x50: return KeyCode::Left;
			case 0x51: return KeyCode::Down;
			case 0x52: return KeyCode::Up;
			default  : return KeyCode::Unknown;
		}
	}

	bool containsUsage(std::array<std::uint8_t, keyboardReportKeyCount> const& keys, std::uint8_t usage) {
		for (auto const key : keys) {
			if (key == usage) {
				return true;
			}
		}
		return false;
	}

	std::uint8_t inputModifiers(std::uint8_t hidModifiers) {
		std::uint8_t result = 0;
		if ((hidModifiers & hidLeftShift) != 0) {
			result |= InputModifierShiftLeft;
		}
		if ((hidModifiers & hidRightShift) != 0) {
			result |= InputModifierShiftRight;
		}
		if ((hidModifiers & (hidLeftAlt | hidRightAlt)) != 0) {
			result |= InputModifierAlt;
		}
		if ((hidModifiers & (hidLeftControl | hidRightControl)) != 0) {
			result |= InputModifierCtrl;
		}
		return result;
	}

	std::uint8_t keyCharacter(std::uint8_t usage, std::uint8_t hidModifiers, bool capsLock) {
		bool const shift = (hidModifiers & (hidLeftShift | hidRightShift)) != 0;
		if (usage >= 0x04U && usage <= 0x1DU) {
			char value = static_cast<char>('a' + (usage - 0x04U));
			if (shift != capsLock) {
				value = static_cast<char>(value - 'a' + 'A');
			}
			return static_cast<std::uint8_t>(value);
		}
		static constexpr char unshifted[] = "1234567890";
		static constexpr char shifted[] = "!@#$%^&*()";
		if (usage >= 0x1EU && usage <= 0x27U) {
			auto const index = usage - 0x1EU;
			return static_cast<std::uint8_t>(shift ? shifted[index] : unshifted[index]);
		}
		switch (usage) {
			case 0x28: return '\r';
			case 0x2A: return '\b';
			case 0x2B: return '\t';
			case 0x2C: return ' ';
			case 0x2D: return shift ? '_' : '-';
			case 0x2E: return shift ? '+' : '=';
			case 0x2F: return shift ? '{' : '[';
			case 0x30: return shift ? '}' : ']';
			case 0x31: return shift ? '|' : '\\';
			case 0x33: return shift ? ':' : ';';
			case 0x34: return shift ? '"' : '\'';
			case 0x35: return shift ? '~' : '`';
			case 0x36: return shift ? '<' : ',';
			case 0x37: return shift ? '>' : '.';
			case 0x38: return shift ? '?' : '/';
			default  : return 0;
		}
	}

	class UsbKeyboard final : public InputDevice {
	public:
		[[nodiscard]] std::string_view name() const override { return "usb-keyboard"; }

		[[nodiscard]] bool isReady() const override { return _ready.load(std::memory_order_acquire); }

		void update() override {}

		void setRawEventReporting(bool enabled) override {
			portENTER_CRITICAL(&_mux);
			_rawReporting = enabled;
			portEXIT_CRITICAL(&_mux);
		}

		bool pollEvent(InputEvent& event) override {
			portENTER_CRITICAL(&_mux);
			if (_eventCount == 0) {
				portEXIT_CRITICAL(&_mux);
				return false;
			}
			event = _events[_eventTail];
			_eventTail = (_eventTail + 1U) % keyboardEventCapacity;
			--_eventCount;
			portEXIT_CRITICAL(&_mux);
			return true;
		}

		[[nodiscard]] bool isPressed(KeyCode key) const override {
			portENTER_CRITICAL(&_mux);
			bool pressed = false;
			for (auto const usage : _keys) {
				if (mapHidKey(usage) == key) {
					pressed = true;
					break;
				}
			}
			portEXIT_CRITICAL(&_mux);
			return pressed;
		}

		[[nodiscard]] std::uint32_t rawState() const override {
			portENTER_CRITICAL(&_mux);
			auto value = static_cast<std::uint32_t>(_modifiers)
			    | (_connectedCount != 0 ? (1U << 8U) : 0U)
			    | (_capsLock ? (1U << 9U) : 0U);
			portEXIT_CRITICAL(&_mux);
			return value;
		}

		[[nodiscard]] std::uint32_t rawKeyCount() const override { return keyboardReportKeyCount; }

		void setReady(bool ready) { _ready.store(ready, std::memory_order_release); }

		void connected() {
			portENTER_CRITICAL(&_mux);
			++_connectedCount;
			portEXIT_CRITICAL(&_mux);
		}

		void disconnected() {
			portENTER_CRITICAL(&_mux);
			if (_connectedCount != 0) {
				--_connectedCount;
			}
			auto const timestamp = uptimeMs();
			for (auto const usage : _keys) {
				if (usage > 3U) {
					pushLocked(InputEventType::KeyUp, usage, _modifiers, 0, timestamp);
				}
			}
			_keys.fill(0);
			_modifiers = 0;
			portEXIT_CRITICAL(&_mux);
		}

		[[nodiscard]] bool connectedState() const {
			portENTER_CRITICAL(&_mux);
			auto const result = _connectedCount != 0;
			portEXIT_CRITICAL(&_mux);
			return result;
		}

		void report(std::uint8_t const* data, std::size_t length) {
			if (data == nullptr || length < 8U) {
				return;
			}
			std::array<std::uint8_t, keyboardReportKeyCount> next{};
			std::memcpy(next.data(), data + 2, next.size());
			auto const nextModifiers = data[0];
			auto const timestamp = uptimeMs();
			portENTER_CRITICAL(&_mux);
			for (auto const usage : _keys) {
				if (usage > 3U && !containsUsage(next, usage)) {
					pushLocked(InputEventType::KeyUp, usage, _modifiers, 0, timestamp);
				}
			}
			for (auto const usage : next) {
				if (usage <= 3U || containsUsage(_keys, usage)) {
					continue;
				}
				if (usage == 0x39U) {
					_capsLock = !_capsLock;
				}
				pushLocked(InputEventType::KeyDown, usage, nextModifiers,
				    keyCharacter(usage, nextModifiers, _capsLock), timestamp);
			}
			_keys = next;
			_modifiers = nextModifiers;
			portEXIT_CRITICAL(&_mux);
		}

	private:
		void pushLocked(InputEventType type, std::uint8_t usage, std::uint8_t modifiers,
		    std::uint8_t character, std::uint32_t timestamp) {
			auto const key = mapHidKey(usage);
			if (key == KeyCode::Unknown && !_rawReporting) {
				return;
			}
			if (_eventCount == keyboardEventCapacity) {
				_eventTail = (_eventTail + 1U) % keyboardEventCapacity;
				--_eventCount;
			}
			_events[_eventHead] = {
			    .type = type,
			    .key = key,
			    .timestampMs = timestamp,
			    .character = type == InputEventType::KeyDown ? character : static_cast<std::uint8_t>(0),
			    .scanCode = usage,
			    .modifiers = inputModifiers(modifiers),
			};
			_eventHead = (_eventHead + 1U) % keyboardEventCapacity;
			++_eventCount;
		}

		std::atomic_bool _ready{false};
		mutable portMUX_TYPE _mux = portMUX_INITIALIZER_UNLOCKED;
		std::array<InputEvent, keyboardEventCapacity> _events{};
		std::array<std::uint8_t, keyboardReportKeyCount> _keys{};
		std::size_t _eventHead{0};
		std::size_t _eventTail{0};
		std::size_t _eventCount{0};
		std::uint8_t _modifiers{0};
		std::uint8_t _connectedCount{0};
		bool _capsLock{false};
		bool _rawReporting{false};
	};

	class UsbMouse final : public PointingDevice {
	public:
		[[nodiscard]] std::string_view name() const override { return "usb-mouse"; }

		[[nodiscard]] bool isReady() const override { return _ready.load(std::memory_order_acquire); }

		[[nodiscard]] PointingDeviceState state() const override {
			portENTER_CRITICAL(&_mux);
			auto result = _state;
			result.connected = _connectedCount != 0;
			portEXIT_CRITICAL(&_mux);
			return result;
		}

		void resetPosition() override {
			portENTER_CRITICAL(&_mux);
			_state.x = 0;
			_state.y = 0;
			_state.wheel = 0;
			portEXIT_CRITICAL(&_mux);
		}

		void setReady(bool ready) { _ready.store(ready, std::memory_order_release); }

		void connected() {
			portENTER_CRITICAL(&_mux);
			++_connectedCount;
			portEXIT_CRITICAL(&_mux);
		}

		void disconnected() {
			portENTER_CRITICAL(&_mux);
			if (_connectedCount != 0) {
				--_connectedCount;
			}
			_state.buttons = 0;
			portEXIT_CRITICAL(&_mux);
		}

		void report(std::uint8_t const* data, std::size_t length) {
			if (data == nullptr || length < 3U) {
				return;
			}
			auto const timestamp = uptimeMs();
			portENTER_CRITICAL(&_mux);
			_state.buttons = data[0] & 0x1FU;
			_state.x += static_cast<std::int8_t>(data[1]);
			_state.y += static_cast<std::int8_t>(data[2]);
			if (length >= 4U) {
				_state.wheel += static_cast<std::int8_t>(data[3]);
			}
			++_state.reportCount;
			_state.lastReportTimestampMs = timestamp;
			portEXIT_CRITICAL(&_mux);
		}

	private:
		std::atomic_bool _ready{false};
		mutable portMUX_TYPE _mux = portMUX_INITIALIZER_UNLOCKED;
		PointingDeviceState _state{};
		std::uint8_t _connectedCount{0};
	};

	struct HidUsage {
		std::uint16_t page{0};
		std::uint16_t id{0};
	};

	struct HidField {
		HidUsage usage{};
		std::uint16_t bitOffset{0};
		std::uint8_t bitSize{0};
		std::int32_t logicalMin{0};
		std::int32_t logicalMax{1};
	};

	struct GamepadReportLayout {
		bool hasReportId{false};
		std::uint8_t reportId{0};
		bool hasXAxis{false};
		bool hasYAxis{false};
		bool hasHat{false};
		HidField xAxis{};
		HidField yAxis{};
		HidField hat{};
		std::array<HidField, gamepadButtonFieldCapacity> buttons{};
		std::size_t buttonCount{0};
	};

	struct GamepadDiagnostics {
		bool connected{false};
		std::uint32_t reportCount{0};
		std::uint32_t lastReportTimestampMs{0};
		std::uint8_t lastReportBytes{0};
		std::uint8_t reportId{0};
	};

	struct HidLocalState {
		std::array<HidUsage, hidLocalUsageCapacity> usages{};
		std::size_t usageCount{0};
		bool hasUsageRange{false};
		HidUsage usageMinimum{};
		HidUsage usageMaximum{};

		void clear() {
			usageCount = 0;
			hasUsageRange = false;
			usageMinimum = {};
			usageMaximum = {};
		}
	};

	struct HidGlobalState {
		std::uint16_t usagePage{0};
		std::int32_t logicalMin{0};
		std::int32_t logicalMax{1};
		std::uint8_t reportSize{0};
		std::uint8_t reportCount{0};
		std::uint8_t reportId{0};
	};

	struct HidReportOffset {
		std::uint8_t reportId{0};
		std::uint16_t bitOffset{0};
		bool used{false};
	};

	std::uint32_t readLittleEndian(std::uint8_t const* data, std::size_t size) {
		std::uint32_t value = 0;
		for (std::size_t index = 0; index < size; ++index) {
			value |= static_cast<std::uint32_t>(data[index]) << (index * 8U);
		}
		return value;
	}

	std::int32_t signedItemValue(std::uint32_t value, std::size_t size) {
		if (size == 0) {
			return 0;
		}
		if (size >= 4) {
			return static_cast<std::int32_t>(value);
		}
		auto const bitCount = static_cast<unsigned>(size * 8U);
		auto const signBit = 1U << (bitCount - 1U);
		if ((value & signBit) == 0) {
			return static_cast<std::int32_t>(value);
		}
		auto const mask = ~((1U << bitCount) - 1U);
		return static_cast<std::int32_t>(value | mask);
	}

	std::size_t hidItemDataSize(std::uint8_t prefix) {
		switch (prefix & 0x03U) {
			case 0 : return 0;
			case 1 : return 1;
			case 2 : return 2;
			default: return 4;
		}
	}

	HidUsage hidUsageFromItem(std::uint32_t value, std::uint16_t defaultPage) {
		if (value > 0xFFFFU) {
			return {
			    .page = static_cast<std::uint16_t>(value >> 16U),
			    .id = static_cast<std::uint16_t>(value & 0xFFFFU),
			};
		}
		return {
		    .page = defaultPage,
		    .id = static_cast<std::uint16_t>(value),
		};
	}

	void appendLocalUsage(HidLocalState& local, HidUsage usage) {
		if (local.usageCount >= local.usages.size()) {
			return;
		}
		local.usages[local.usageCount] = usage;
		++local.usageCount;
	}

	HidUsage localUsageAt(HidLocalState const& local, std::size_t index) {
		if (index < local.usageCount) {
			return local.usages[index];
		}
		if (local.hasUsageRange && local.usageMinimum.page == local.usageMaximum.page) {
			auto const id = static_cast<std::uint32_t>(local.usageMinimum.id) + index;
			if (id <= local.usageMaximum.id) {
				return {
				    .page = local.usageMinimum.page,
				    .id = static_cast<std::uint16_t>(id),
				};
			}
		}
		if (local.usageCount > 0) {
			return local.usages[local.usageCount - 1U];
		}
		return {};
	}

	HidReportOffset& reportOffsetFor(std::array<HidReportOffset, hidReportOffsetCapacity>& offsets, std::uint8_t reportId) {
		for (auto& offset : offsets) {
			if (offset.used && offset.reportId == reportId) {
				return offset;
			}
		}
		for (auto& offset : offsets) {
			if (!offset.used) {
				offset.used = true;
				offset.reportId = reportId;
				offset.bitOffset = 0;
				return offset;
			}
		}
		return offsets[0];
	}

	bool layoutAcceptsReport(GamepadReportLayout& layout, std::uint8_t reportId) {
		if (layout.buttonCount == 0 && !layout.hasXAxis && !layout.hasYAxis && !layout.hasHat) {
			layout.reportId = reportId;
			layout.hasReportId = reportId != 0;
			return true;
		}
		return layout.reportId == reportId;
	}

	void addGamepadField(GamepadReportLayout& layout, std::uint8_t reportId, HidField const& field) {
		if (field.bitSize == 0 || !layoutAcceptsReport(layout, reportId)) {
			return;
		}
		if (field.usage.page == hidUsagePageButton && field.usage.id > 0) {
			if (layout.buttonCount >= layout.buttons.size()) {
				return;
			}
			layout.buttons[layout.buttonCount] = field;
			++layout.buttonCount;
			return;
		}
		if (field.usage.page != hidUsagePageGenericDesktop) {
			return;
		}
		if (field.usage.id == hidUsageX && !layout.hasXAxis) {
			layout.xAxis = field;
			layout.hasXAxis = true;
		} else if (field.usage.id == hidUsageY && !layout.hasYAxis) {
			layout.yAxis = field;
			layout.hasYAxis = true;
		} else if (field.usage.id == hidUsageHatSwitch && !layout.hasHat) {
			layout.hat = field;
			layout.hasHat = true;
		}
	}

	void addInputFields(
	    GamepadReportLayout& layout,
	    HidGlobalState const& globals,
	    HidLocalState const& local,
	    std::uint32_t inputFlags,
	    std::uint16_t bitOffset) {
		bool const constant = (inputFlags & 0x01U) != 0;
		if (constant || globals.reportSize == 0 || globals.reportCount == 0) {
			return;
		}
		for (std::uint8_t index = 0; index < globals.reportCount; ++index) {
			auto const usage = localUsageAt(local, index);
			if (usage.page == 0 || usage.id == 0) {
				continue;
			}
			addGamepadField(layout, globals.reportId, {
			                                              .usage = usage,
			                                              .bitOffset = static_cast<std::uint16_t>(bitOffset + index * globals.reportSize),
			                                              .bitSize = globals.reportSize,
			                                              .logicalMin = globals.logicalMin,
			                                              .logicalMax = globals.logicalMax,
			                                          });
		}
	}

	bool parseGamepadReportDescriptor(std::uint8_t const* descriptor, std::size_t length, GamepadReportLayout& layout) {
		layout = {};
		if (descriptor == nullptr || length == 0) {
			return false;
		}
		HidGlobalState globals{};
		HidLocalState local{};
		std::array<HidReportOffset, hidReportOffsetCapacity> offsets{};
		std::size_t cursor = 0;
		while (cursor < length) {
			auto const prefix = descriptor[cursor++];
			if (prefix == 0xFEU) {
				if (cursor + 2U > length) {
					return false;
				}
				auto const longSize = descriptor[cursor];
				cursor += 2U + longSize;
				continue;
			}
			auto const dataSize = hidItemDataSize(prefix);
			if (cursor + dataSize > length) {
				return false;
			}
			auto const unsignedValue = readLittleEndian(descriptor + cursor, dataSize);
			auto const signedValue = signedItemValue(unsignedValue, dataSize);
			cursor += dataSize;

			auto const type = (prefix >> 2U) & 0x03U;
			auto const tagValue = (prefix >> 4U) & 0x0FU;
			if (type == 0) {
				if (tagValue == 0x08U) {
					auto& offset = reportOffsetFor(offsets, globals.reportId);
					addInputFields(layout, globals, local, unsignedValue, offset.bitOffset);
					offset.bitOffset = static_cast<std::uint16_t>(offset.bitOffset + globals.reportSize * globals.reportCount);
					local.clear();
				} else if (tagValue == 0x0AU || tagValue == 0x0CU) {
					local.clear();
				}
			} else if (type == 1) {
				switch (tagValue) {
					case 0x00: globals.usagePage = static_cast<std::uint16_t>(unsignedValue); break;
					case 0x01: globals.logicalMin = signedValue; break;
					case 0x02: globals.logicalMax = signedValue; break;
					case 0x07: globals.reportSize = static_cast<std::uint8_t>(unsignedValue); break;
					case 0x08: globals.reportId = static_cast<std::uint8_t>(unsignedValue); break;
					case 0x09: globals.reportCount = static_cast<std::uint8_t>(unsignedValue); break;
					default  : break;
				}
			} else if (type == 2) {
				switch (tagValue) {
					case 0x00: appendLocalUsage(local, hidUsageFromItem(unsignedValue, globals.usagePage)); break;
					case 0x01:
						local.usageMinimum = hidUsageFromItem(unsignedValue, globals.usagePage);
						local.hasUsageRange = true;
						break;
					case 0x02:
						local.usageMaximum = hidUsageFromItem(unsignedValue, globals.usagePage);
						local.hasUsageRange = true;
						break;
					default: break;
				}
			}
		}
		return layout.buttonCount > 0 && (layout.hasHat || layout.hasXAxis || layout.hasYAxis);
	}

	std::uint32_t readBits(std::uint8_t const* data, std::size_t length, std::uint16_t bitOffset, std::uint8_t bitSize) {
		if (data == nullptr || bitSize == 0 || bitSize > 32U) {
			return 0;
		}
		std::uint32_t value = 0;
		for (std::uint8_t bit = 0; bit < bitSize; ++bit) {
			auto const sourceBit = static_cast<std::uint32_t>(bitOffset) + bit;
			auto const byteIndex = sourceBit / 8U;
			if (byteIndex >= length) {
				return value;
			}
			if ((data[byteIndex] & (1U << (sourceBit % 8U))) != 0) {
				value |= 1U << bit;
			}
		}
		return value;
	}

	std::int32_t hidFieldValue(HidField const& field, std::uint32_t rawValue) {
		if (field.logicalMin >= 0 || field.bitSize == 0 || field.bitSize >= 32U) {
			return static_cast<std::int32_t>(rawValue);
		}
		auto const signBit = 1U << (field.bitSize - 1U);
		if ((rawValue & signBit) == 0) {
			return static_cast<std::int32_t>(rawValue);
		}
		auto const mask = ~((1U << field.bitSize) - 1U);
		return static_cast<std::int32_t>(rawValue | mask);
	}

	bool fieldIsPressed(HidField const& field, std::uint8_t const* data, std::size_t length) {
		return readBits(data, length, field.bitOffset, field.bitSize) != 0;
	}

	bool buttonUsagePressed(GamepadReportLayout const& layout, std::uint16_t usage, std::uint8_t const* data, std::size_t length) {
		for (std::size_t index = 0; index < layout.buttonCount; ++index) {
			auto const& field = layout.buttons[index];
			if (field.usage.id == usage && fieldIsPressed(field, data, length)) {
				return true;
			}
		}
		return false;
	}

	void logicalRangeForAxis(HidField const& field, std::int32_t& minValue, std::int32_t& maxValue) {
		minValue = field.logicalMin;
		maxValue = field.logicalMax;
		if (maxValue > minValue) {
			return;
		}
		if (field.logicalMin < 0 && field.bitSize > 1U) {
			auto const magnitude = static_cast<std::int64_t>(1) << (field.bitSize - 1U);
			minValue = magnitude > 0x7FFFFFFFLL ? (-2147483647 - 1) : static_cast<std::int32_t>(-magnitude);
			maxValue = magnitude > 0x7FFFFFFFLL ? static_cast<std::int32_t>(0x7FFFFFFFU) : static_cast<std::int32_t>(magnitude - 1);
		} else {
			minValue = 0;
			if (field.bitSize >= 31U) {
				maxValue = static_cast<std::int32_t>(0x7FFFFFFFU);
			} else {
				maxValue = static_cast<std::int32_t>((static_cast<std::int64_t>(1) << field.bitSize) - 1);
			}
		}
	}

	bool axisNegative(HidField const& field, std::uint8_t const* data, std::size_t length) {
		auto raw = readBits(data, length, field.bitOffset, field.bitSize);
		auto const value = hidFieldValue(field, raw);
		std::int32_t minValue = 0;
		std::int32_t maxValue = 0;
		logicalRangeForAxis(field, minValue, maxValue);
		auto const span = static_cast<std::int64_t>(maxValue) - static_cast<std::int64_t>(minValue);
		return static_cast<std::int64_t>(value) < static_cast<std::int64_t>(minValue) + span / 4;
	}

	bool axisPositive(HidField const& field, std::uint8_t const* data, std::size_t length) {
		auto raw = readBits(data, length, field.bitOffset, field.bitSize);
		auto const value = hidFieldValue(field, raw);
		std::int32_t minValue = 0;
		std::int32_t maxValue = 0;
		logicalRangeForAxis(field, minValue, maxValue);
		auto const span = static_cast<std::int64_t>(maxValue) - static_cast<std::int64_t>(minValue);
		return static_cast<std::int64_t>(value) > static_cast<std::int64_t>(maxValue) - span / 4;
	}

	void applyHatSwitch(GamepadState& state, std::uint32_t value) {
		switch (value) {
			case 0:
				state.up = true;
				break;
			case 1:
				state.up = true;
				state.right = true;
				break;
			case 2:
				state.right = true;
				break;
			case 3:
				state.down = true;
				state.right = true;
				break;
			case 4:
				state.down = true;
				break;
			case 5:
				state.down = true;
				state.left = true;
				break;
			case 6:
				state.left = true;
				break;
			case 7:
				state.up = true;
				state.left = true;
				break;
			default:
				break;
		}
	}

	GamepadState parseGamepadState(
	    GamepadReportLayout const& layout,
	    std::uint8_t const* data,
	    std::size_t length) {
		GamepadState state{};
		state.a = buttonUsagePressed(layout, 1, data, length);
		state.b = buttonUsagePressed(layout, 2, data, length);
		state.x = buttonUsagePressed(layout, 3, data, length);
		state.y = buttonUsagePressed(layout, 4, data, length);
		state.select = buttonUsagePressed(layout, 7, data, length) || buttonUsagePressed(layout, 9, data, length);
		state.start = buttonUsagePressed(layout, 8, data, length) || buttonUsagePressed(layout, 10, data, length);
		if (layout.hasHat) {
			applyHatSwitch(state, readBits(data, length, layout.hat.bitOffset, layout.hat.bitSize));
		}
		if (layout.hasXAxis) {
			state.left = state.left || axisNegative(layout.xAxis, data, length);
			state.right = state.right || axisPositive(layout.xAxis, data, length);
		}
		if (layout.hasYAxis) {
			state.up = state.up || axisNegative(layout.yAxis, data, length);
			state.down = state.down || axisPositive(layout.yAxis, data, length);
		}
		return state;
	}

	class UsbGamepad final : public GamepadDevice {
	public:
		[[nodiscard]] std::string_view name() const override { return "usb-gamepad"; }

		[[nodiscard]] bool isReady() const override { return _ready.load(std::memory_order_acquire); }

		[[nodiscard]] GamepadState state() override {
			portENTER_CRITICAL(&_mux);
			auto result = _state;
			portEXIT_CRITICAL(&_mux);
			return result;
		}

		void setReady(bool ready) { _ready.store(ready, std::memory_order_release); }

		void connected(GamepadReportLayout const& layout) {
			portENTER_CRITICAL(&_mux);
			_layout = layout;
			_state = {};
			++_connectedCount;
			portEXIT_CRITICAL(&_mux);
		}

		void disconnected() {
			portENTER_CRITICAL(&_mux);
			if (_connectedCount != 0) {
				--_connectedCount;
			}
			if (_connectedCount == 0) {
				_state = {};
			}
			portEXIT_CRITICAL(&_mux);
		}

		[[nodiscard]] bool connectedState() const {
			portENTER_CRITICAL(&_mux);
			auto const result = _connectedCount != 0;
			portEXIT_CRITICAL(&_mux);
			return result;
		}

		[[nodiscard]] GamepadDiagnostics diagnostics() const {
			portENTER_CRITICAL(&_mux);
			auto result = GamepadDiagnostics{
			    .connected = _connectedCount != 0,
			    .reportCount = _reportCount,
			    .lastReportTimestampMs = _lastReportTimestampMs,
			    .lastReportBytes = _lastReportBytes,
			    .reportId = _layout.reportId,
			};
			portEXIT_CRITICAL(&_mux);
			return result;
		}

		void report(std::uint8_t const* data, std::size_t length) {
			if (data == nullptr || length == 0) {
				return;
			}
			auto const timestamp = uptimeMs();
			portENTER_CRITICAL(&_mux);
			_lastReportBytes = length > 255U ? 255U : static_cast<std::uint8_t>(length);
			if (_layout.hasReportId) {
				if (data[0] != _layout.reportId || length <= 1U) {
					portEXIT_CRITICAL(&_mux);
					return;
				}
				_state = parseGamepadState(_layout, data + 1, length - 1U);
			} else {
				_state = parseGamepadState(_layout, data, length);
			}
			++_reportCount;
			_lastReportTimestampMs = timestamp;
			portEXIT_CRITICAL(&_mux);
		}

	private:
		std::atomic_bool _ready{false};
		mutable portMUX_TYPE _mux = portMUX_INITIALIZER_UNLOCKED;
		GamepadReportLayout _layout{};
		GamepadState _state{};
		std::uint32_t _reportCount{0};
		std::uint32_t _lastReportTimestampMs{0};
		std::uint8_t _lastReportBytes{0};
		std::uint8_t _connectedCount{0};
	};

	struct DriverEvent {
		hid_host_device_handle_t handle{nullptr};
		hid_host_driver_event_t event{};
	};

	UsbKeyboard g_keyboard;
	UsbMouse g_mouse;
	UsbGamepad g_gamepad;
	QueueHandle_t g_driverEvents{nullptr};
	std::atomic_bool g_initialized{false};
	std::atomic_bool g_usbInstalled{false};
	std::atomic_bool g_hidInstalled{false};
	std::atomic<hid_host_device_handle_t> g_gamepadHandle{nullptr};
	std::atomic<std::uint32_t> g_connectionEvents{0};
	std::atomic<std::uint32_t> g_droppedEvents{0};
	std::atomic<std::uint32_t> g_errors{0};
	std::atomic<std::int32_t> g_lastError{ESP_OK};
	std::atomic<std::int32_t> g_usbInstallResult{ESP_FAIL};

	void recordError(esp_err_t error, char const* operation) {
		g_lastError.store(error, std::memory_order_release);
		g_errors.fetch_add(1U, std::memory_order_relaxed);
		ESP_LOGE(tag, "%s failed: %s", operation, esp_err_to_name(error));
	}

	void usbLibraryTask(void* argument) {
		usb_host_config_t config{};
		config.skip_phy_setup = false;
		config.intr_flags = ESP_INTR_FLAG_LOWMED;
#if ESP_IDF_VERSION_MAJOR >= 6
	#if defined(LEXXOS_ESPIDF_USB_HOST_PORT_FS)
		config.peripheral_map = (1U << 1U);
		ESP_LOGI(tag, "USB Host using ESP32-P4 Full-Speed OTG peripheral (USB2)");
	#elif defined(LEXXOS_ESPIDF_USB_HOST_PORT_DUAL)
		config.peripheral_map = (1U << 0U) | (1U << 1U);
		ESP_LOGI(tag, "USB Host using ESP32-P4 High-Speed and Full-Speed OTG peripherals");
	#else
		config.peripheral_map = (1U << 0U);
		ESP_LOGI(tag, "USB Host using ESP32-P4 High-Speed OTG peripheral (USB3)");
	#endif
#else
		ESP_LOGW(tag, "ESP-IDF 5.x cannot select the ESP32-P4 FS host through usb_host_config_t; use ESP-IDF 6.x for hubs");
#endif
		auto const result = usb_host_install(&config);
		g_usbInstallResult.store(result, std::memory_order_release);
		if (result == ESP_OK) {
			g_usbInstalled.store(true, std::memory_order_release);
		}
		xTaskNotifyGive(static_cast<TaskHandle_t>(argument));
		if (result != ESP_OK) {
			recordError(result, "usb_host_install");
			vTaskDelete(nullptr);
			return;
		}
		for (;;) {
			std::uint32_t flags = 0;
			auto const error = usb_host_lib_handle_events(portMAX_DELAY, &flags);
			if (error != ESP_OK) {
				recordError(error, "usb_host_lib_handle_events");
			}
		}
	}

	void interfaceCallback(hid_host_device_handle_t handle, hid_host_interface_event_t event, void*) {
		hid_host_dev_params_t params{};
		auto error = hid_host_device_get_params(handle, &params);
		if (error != ESP_OK) {
			recordError(error, "hid_host_device_get_params");
			return;
		}
		if (event == HID_HOST_INTERFACE_EVENT_INPUT_REPORT) {
			std::array<std::uint8_t, hidReportBufferBytes> report{};
			std::size_t length = 0;
			error = hid_host_device_get_raw_input_report_data(handle, report.data(), report.size(), &length);
			if (error != ESP_OK) {
				recordError(error, "hid_host_device_get_raw_input_report_data");
				return;
			}
			if (params.sub_class == HID_SUBCLASS_BOOT_INTERFACE && params.proto == HID_PROTOCOL_KEYBOARD) {
				g_keyboard.report(report.data(), length);
			} else if (params.sub_class == HID_SUBCLASS_BOOT_INTERFACE && params.proto == HID_PROTOCOL_MOUSE) {
				g_mouse.report(report.data(), length);
			} else if (handle == g_gamepadHandle.load(std::memory_order_acquire)) {
				g_gamepad.report(report.data(), length);
			}
			return;
		}
		if (event == HID_HOST_INTERFACE_EVENT_DISCONNECTED) {
			if (params.proto == HID_PROTOCOL_KEYBOARD) {
				g_keyboard.disconnected();
			}
			if (params.proto == HID_PROTOCOL_MOUSE) {
				g_mouse.disconnected();
			}
			if (handle == g_gamepadHandle.load(std::memory_order_acquire)) {
				g_gamepad.disconnected();
				g_gamepadHandle.store(nullptr, std::memory_order_release);
			}
			error = hid_host_device_close(handle);
			if (error != ESP_OK) {
				recordError(error, "hid_host_device_close");
			}
			return;
		}
		if (event == HID_HOST_INTERFACE_EVENT_TRANSFER_ERROR) {
			recordError(ESP_FAIL, "USB HID interrupt transfer");
		}
	}

	void openDevice(hid_host_device_handle_t handle) {
		hid_host_dev_params_t params{};
		auto error = hid_host_device_get_params(handle, &params);
		if (error != ESP_OK) {
			recordError(error, "hid_host_device_get_params");
			return;
		}
		auto const isBootKeyboard = params.sub_class == HID_SUBCLASS_BOOT_INTERFACE && params.proto == HID_PROTOCOL_KEYBOARD;
		auto const isBootMouse = params.sub_class == HID_SUBCLASS_BOOT_INTERFACE && params.proto == HID_PROTOCOL_MOUSE;
		hid_host_device_config_t config{};
		config.callback = interfaceCallback;
		config.callback_arg = nullptr;
		error = hid_host_device_open(handle, &config);
		if (error != ESP_OK) {
			recordError(error, "hid_host_device_open");
			return;
		}

		GamepadReportLayout gamepadLayout{};
		if (isBootKeyboard || isBootMouse) {
			error = hid_class_request_set_protocol(handle, HID_REPORT_PROTOCOL_BOOT);
			if (error != ESP_OK) {
				ESP_LOGW(tag, "set boot protocol failed: %s", esp_err_to_name(error));
			}
		} else {
			std::size_t descriptorLength = 0;
			auto const* descriptor = hid_host_get_report_descriptor(handle, &descriptorLength);
			if (!parseGamepadReportDescriptor(descriptor, descriptorLength, gamepadLayout)) {
				ESP_LOGW(tag, "ignoring HID interface (protocol=%d, subclass=%d, report-desc=%u bytes)",
				    params.proto, params.sub_class, static_cast<unsigned>(descriptorLength));
				(void) hid_host_device_close(handle);
				return;
			}
			if (g_gamepadHandle.load(std::memory_order_acquire) != nullptr) {
				ESP_LOGW(tag, "ignoring extra HID gamepad interface");
				(void) hid_host_device_close(handle);
				return;
			}
		}
		if (isBootKeyboard) {
			error = hid_class_request_set_idle(handle, 0, 0);
			if (error != ESP_OK) {
				ESP_LOGW(tag, "keyboard set-idle failed: %s", esp_err_to_name(error));
			}
		}
		error = hid_host_device_start(handle);
		if (error != ESP_OK) {
			recordError(error, "hid_host_device_start");
			(void) hid_host_device_close(handle);
			return;
		}
		if (isBootKeyboard) {
			g_keyboard.connected();
		}
		if (isBootMouse) {
			g_mouse.connected();
		}
		if (!isBootKeyboard && !isBootMouse) {
			g_gamepad.connected(gamepadLayout);
			g_gamepadHandle.store(handle, std::memory_order_release);
		}
		g_connectionEvents.fetch_add(1U, std::memory_order_relaxed);
		ESP_LOGI(tag, "USB HID %s connected",
		    isBootKeyboard ? "keyboard" : isBootMouse ? "mouse"
		                                              : "gamepad");
	}

	void hidEventTask(void*) {
		DriverEvent event{};
		for (;;) {
			if (xQueueReceive(g_driverEvents, &event, portMAX_DELAY) == pdTRUE
			    && event.event == HID_HOST_DRIVER_EVENT_CONNECTED) {
				openDevice(event.handle);
			}
		}
	}

	void driverCallback(hid_host_device_handle_t handle, hid_host_driver_event_t event, void*) {
		DriverEvent queued{.handle = handle, .event = event};
		if (g_driverEvents == nullptr || xQueueSend(g_driverEvents, &queued, 0) != pdTRUE) {
			g_droppedEvents.fetch_add(1U, std::memory_order_relaxed);
		}
	}

} // namespace

bool initialize() {
	if (g_initialized.load(std::memory_order_acquire)) {
		return true;
	}
	g_driverEvents = xQueueCreate(16, sizeof(DriverEvent));
	if (g_driverEvents == nullptr) {
		recordError(ESP_ERR_NO_MEM, "xQueueCreate");
		return false;
	}
	auto const currentTask = xTaskGetCurrentTaskHandle();
	if (xTaskCreate(usbLibraryTask, "usb-host", usbHostStackBytes, currentTask, 4, nullptr) != pdPASS) {
		recordError(ESP_ERR_NO_MEM, "xTaskCreate(usb-host)");
		return false;
	}
	if (ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(initializationTimeoutMs)) == 0
	    || g_usbInstallResult.load(std::memory_order_acquire) != ESP_OK) {
		recordError(g_usbInstallResult.load(std::memory_order_acquire), "USB host startup");
		return false;
	}
	hid_host_driver_config_t hidConfig{};
	hidConfig.create_background_task = true;
	hidConfig.task_priority = 5;
	hidConfig.stack_size = 4096;
	hidConfig.core_id = 0;
	hidConfig.callback = driverCallback;
	hidConfig.callback_arg = nullptr;
	auto const error = hid_host_install(&hidConfig);
	if (error != ESP_OK) {
		recordError(error, "hid_host_install");
		return false;
	}
	g_hidInstalled.store(true, std::memory_order_release);
	if (xTaskCreate(hidEventTask, "usb-hid-events", hidEventStackBytes, nullptr, 4, nullptr) != pdPASS) {
		recordError(ESP_ERR_NO_MEM, "xTaskCreate(usb-hid-events)");
		return false;
	}
	g_keyboard.setReady(true);
	g_mouse.setReady(true);
	g_gamepad.setReady(true);
	g_initialized.store(true, std::memory_order_release);
	ESP_LOGI(tag, "USB Host HID ready");
	return true;
}

InputDevice& keyboardDevice() {
	return g_keyboard;
}

PointingDevice& mouseDevice() {
	return g_mouse;
}

GamepadDevice& gamepadDevice() {
	return g_gamepad;
}

UsbHidHostStatus status() {
	auto const gamepad = g_gamepad.diagnostics();
	auto const usbInstalled = g_usbInstalled.load(std::memory_order_acquire);
	usb_host_lib_info_t usbInfo{};
	auto usbInfoError = ESP_ERR_INVALID_STATE;
	if (usbInstalled) {
		usbInfoError = usb_host_lib_info(&usbInfo);
	}
	auto const usbInfoAvailable = usbInfoError == ESP_OK;
	return {
	    .initialized = g_initialized.load(std::memory_order_acquire),
	    .usbHostInstalled = usbInstalled,
	    .hidDriverInstalled = g_hidInstalled.load(std::memory_order_acquire),
	    .keyboardConnected = g_keyboard.connectedState(),
	    .mouseConnected = g_mouse.state().connected,
	    .gamepadConnected = gamepad.connected,
	    .usbInfoAvailable = usbInfoAvailable,
	    .rootPortSuspended = usbInfoAvailable ? usbInfo.root_port_suspended : false,
	    .usbDeviceCount = usbInfoAvailable ? static_cast<std::uint32_t>(usbInfo.num_devices) : 0U,
	    .usbClientCount = usbInfoAvailable ? static_cast<std::uint32_t>(usbInfo.num_clients) : 0U,
	    .usbInfoError = usbInfoError,
	    .connectionEvents = g_connectionEvents.load(std::memory_order_acquire),
	    .droppedEvents = g_droppedEvents.load(std::memory_order_acquire),
	    .errors = g_errors.load(std::memory_order_acquire),
	    .lastError = g_lastError.load(std::memory_order_acquire),
	    .gamepadReports = gamepad.reportCount,
	    .gamepadLastReportTimestampMs = gamepad.lastReportTimestampMs,
	    .gamepadLastReportBytes = gamepad.lastReportBytes,
	    .gamepadReportId = gamepad.reportId,
	};
}

} // namespace lexxos::devices::espidf_usb_hid
