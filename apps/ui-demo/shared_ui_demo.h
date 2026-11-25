#pragma once

#include <lexx/ui.h>
#include <lexx/ui/transport.h>

// This is the SAME application logic for a native built-in and RV32I .lexx.
// Neither widget code nor callbacks know how the transport reaches LexxOS.
namespace lexxos::ui_demo {

class Demo final {
public:
	explicit Demo(lexx::ui::Transport const& transport)
	    : _transport(transport)
	    , _window("LexxUI Settings", transport) {
		_brightness.on_change(&Demo::updateProgress, this);
		_save.on_click(&Demo::save, this);
		_dialogs.on_click(&Demo::showNotifications, this);
	}

	int run() {
		char const* themes[] = {"Dark theme", "Light theme", "Classic theme"};
		if (_theme.set_items(themes, 3U) != 0) {
			return 1;
		}

		lexx::ui::Row actions{};
		if (!actions.add(_save) || !actions.add(_dialogs) || !_window.add(_heading) || !_window.add(_deviceName) || !_window.add(_notifications) || !_window.add(_theme) || !_window.add(_brightness) || !_window.add(_progress) || !_window.add(actions)) {
			return 1;
		}

		return _window.exec();
	}

private:
	static void updateProgress(void* context, lexx::uint32_t percent) {
		auto& self = *static_cast<Demo*>(context);
		(void) self._progress.set_value(percent);
	}

	static void save(void* context) {
		auto& self = *static_cast<Demo*>(context);
		(void) lexx::ui::MessageBox::information("Saved", "Settings were stored.")
		    .exec(self._transport);
	}

	static void showNotifications(void* context) {
		auto& self = *static_cast<Demo*>(context);
		switch (self._dialogStep++ % 4U) {
			case 0U:
				(void) lexx::ui::MessageBox::information("Information", "LexxUI is running.")
				    .exec(self._transport);
				break;
			case 1U:
				(void) lexx::ui::MessageBox::warning("Warning", "Check the configuration before applying changes.")
				    .exec(self._transport);
				break;
			case 2U:
				(void) lexx::ui::MessageBox::error("Error", "Example of a system error dialog.")
				    .exec(self._transport);
				break;
			default:
				(void) lexx::ui::MessageBox::question("Question", "Do you want to continue?")
				    .exec(self._transport);
				break;
		}
	}

	lexx::ui::Transport const& _transport;
	lexx::ui::Label _heading{1, "System UI controls"};
	lexx::ui::TextField _deviceName{2, "PicoCalc Lyra"};
	lexx::ui::Checkbox _notifications{3, "Notifications", true};
	lexx::ui::ListView _theme{4};
	lexx::ui::Slider _brightness{5, 35};
	lexx::ui::ProgressBar _progress{6, 35};
	lexx::ui::Button _save{7, "Save"};
	lexx::ui::Button _dialogs{8, "Dialogs"};
	lexx::ui::Window _window;
	lexx::uint32_t _dialogStep{0U};
};

} // namespace lexxos::ui_demo
