#include <cstdlib>
#include <iostream>
#include <string>

#include <lexxos/app/rk3506/picosd_service_state.h>
#include <lexxos/composition/products/lyra_runtime_composition.h>
#include <lexxos/hardware/products/lyra_products.h>

namespace {

namespace composition = lexxos::composition::products;
namespace hardware = lexxos::hardware::products;
using Result = composition::LyraRuntimeResult;

struct State {
	std::string calls;
	bool taskResult{true};
	bool configureResult{true};
	unsigned configurations{};
	bool autoMount{};
	bool readOnly{};
};

void expect(bool condition, char const* message) {
	if (!condition) {
		std::cerr << message << '\n';
		std::exit(1);
	}
}

void systemStorage(void* p) {
	static_cast<State*>(p)->calls += 's';
}

void debug(void* p) {
	static_cast<State*>(p)->calls += 'd';
}

void removable(void* p) {
	static_cast<State*>(p)->calls += 'r';
}

void diagnostics(void* p) {
	static_cast<State*>(p)->calls += 'x';
}

bool hotplug(void* p) {
	auto& state = *static_cast<State*>(p);
	state.calls += 'h';
	return state.taskResult;
}

bool configure(void* p, std::span<hardware::StoragePolicy const> slots) {
	auto& state = *static_cast<State*>(p);
	++state.configurations;
	expect(slots.size() == 2, "configuration lost slots");
	state.autoMount = slots[1].autoMount;
	state.readOnly = slots[1].readOnlyByDefault;
	return state.configureResult;
}

composition::LyraRuntimeProviders providers(State& state) {
	return {&state, systemStorage, debug, removable, diagnostics, hotplug, configure};
}

} // namespace

int main() {
	{
		State displayState;
		auto displayHooks = providers(displayState);
		displayHooks.startDisplay = [](void* p) {
			auto& state = *static_cast<State*>(p);
			state.calls += 'l';
			return state.taskResult;
		};

		expect(composition::startLyraRuntimeTasks(hardware::picoCalcLyraProduct, displayHooks) == Result::ProvidersInvoked,
		    "display worker not created");
		expect(displayState.calls == "lh", "display worker hidden in storage provider");
		displayState.calls.clear();
		displayState.taskResult = false;
		expect(composition::startLyraRuntimeTasks(hardware::picoCalcLyraProduct, displayHooks) == Result::TaskCreationFailed,
		    "display worker failure swallowed");
		expect(displayState.calls == "l", "hotplug started after display worker failure");
	}

	State state;
	auto hooks = providers(state);
	expect(composition::registerLyraRuntimePrograms(hardware::picoCalcLyraProduct, true, hooks) == Result::ProvidersInvoked,
	    "PicoCalc registration failed");
	expect(state.calls == "srdx", "PicoCalc program order changed");
	expect(composition::startLyraRuntimeTasks(hardware::picoCalcLyraProduct, hooks) == Result::ProvidersInvoked,
	    "hotplug task not created");
	expect(state.calls == "srdxh", "unexpected task creation");
	state.calls.clear();
	hooks.removableDiagnostics = nullptr;
	expect(composition::registerLyraRuntimePrograms(hardware::picoCalcLyraProduct, false, hooks) == Result::ProvidersInvoked,
	    "disabled diagnostics requires provider");
	expect(state.configurations == 3, "policy not configured for registration/start");
	expect(state.calls == "srd", "diagnostics-off changed programs");
	state.calls.clear();
	expect(composition::registerLyraRuntimePrograms(hardware::picoCalcLyraProduct, true, hooks) == Result::MissingProvider,
	    "missing diagnostics accepted");
	expect(state.calls.empty(), "partial registration before validation");
	hooks = providers(state);
	hooks.removableStorage = nullptr;
	expect(composition::registerLyraRuntimePrograms(hardware::picoCalcLyraProduct, false, hooks) == Result::MissingProvider,
	    "missing removable provider accepted");
	expect(state.calls.empty(), "partial storage registration");
	hooks = providers(state);
	hooks.startHotplug = nullptr;
	expect(composition::startLyraRuntimeTasks(hardware::picoCalcLyraProduct, hooks) == Result::MissingProvider,
	    "missing task provider accepted");
	hooks = providers(state);
	state.taskResult = false;
	expect(composition::startLyraRuntimeTasks(hardware::picoCalcLyraProduct, hooks) == Result::TaskCreationFailed,
	    "task failure swallowed");
	state.calls.clear();
	hooks.removableStorage = nullptr;
	hooks.removableDiagnostics = nullptr;
	hooks.startHotplug = nullptr;
	expect(composition::registerLyraRuntimePrograms(hardware::lyraStandaloneProduct, true, hooks) == Result::ProvidersInvoked,
	    "standalone requires removable providers");
	expect(state.calls == "sd", "standalone registered removable commands");
	expect(composition::startLyraRuntimeTasks(hardware::lyraStandaloneProduct, {}) == Result::NoTasks,
	    "standalone started removable hotplug");
	state.calls.clear();
	auto const standaloneConfigurations = state.configurations;
	expect(composition::startLyraRuntimeTasks(hardware::lyraStandaloneProduct, hooks) == Result::NoTasks,
	    "standalone task dispatch failed");
	expect(state.configurations == standaloneConfigurations, "standalone configured removable SD");
	state.configureResult = false;
	expect(composition::registerLyraRuntimePrograms(hardware::picoCalcLyraProduct, false, providers(state))
	        == Result::StorageConfigurationFailed,
	    "configuration failure swallowed");
	expect(composition::startLyraRuntimeTasks(hardware::picoCalcLyraProduct, providers(state))
	        == Result::StorageConfigurationFailed,
	    "task started without configuration");
	expect(state.calls.empty(), "configuration failure caused side effects");
	state.configureResult = true;
	state.calls.clear();
	auto product = hardware::picoCalcLyraProduct;
	product.console = {1};
	expect(composition::registerLyraRuntimePrograms(product, true, providers(state)) == Result::InvalidProduct,
	    "conflicting product accepted");
	expect(composition::startLyraRuntimeTasks(product, providers(state)) == Result::InvalidProduct,
	    "conflicting product created task");
	expect(state.calls.empty(), "invalid product had side effects");
	auto slots = hardware::picoCalcLyraStoragePolicy;
	product = hardware::picoCalcLyraProduct;
	product.storage = slots;
	slots[0].readOnlyByDefault = false;
	expect(composition::registerLyraRuntimePrograms(product, false, providers(state)) == Result::InvalidProduct,
	    "writable system SD accepted");
	slots = hardware::picoCalcLyraStoragePolicy;
	slots[1].mountPath = "/unsupported";
	expect(composition::startLyraRuntimeTasks(product, providers(state)) == Result::InvalidProduct,
	    "unsupported mount path silently ignored");
	slots = hardware::picoCalcLyraStoragePolicy;
	slots[1].autoMount = false;
	slots[1].readOnlyByDefault = true;
	state.taskResult = true;
	expect(composition::startLyraRuntimeTasks(product, providers(state)) == Result::ProvidersInvoked,
	    "manual/read-only media lost removal monitoring");
	expect(state.calls == "h", "unexpected callbacks for manual media");
	expect(!state.autoMount && state.readOnly, "product policy not forwarded");

	lexxos::app::rk3506::PicoSdServiceState service;
	expect(service.policy().empty() && !service.removablePolicy() && !service.writeArmed && !service.autoEnabled,
	    "unconfigured service is not inert");
	auto snapshot = hardware::picoCalcLyraStoragePolicy;
	snapshot[0].readOnlyByDefault = false;
	expect(!service.configure(snapshot) && service.policy().empty(), "unsafe system policy accepted");
	snapshot = hardware::picoCalcLyraStoragePolicy;
	std::string name = "sd1", path = "/mnt/sd";
	snapshot[1].deviceName = name.c_str();
	snapshot[1].mountPath = path.c_str();
	expect(service.configure(snapshot), "valid policy rejected");
	expect(service.autoEnabled && !service.autoSuppressed,
	    "removable slot automount was disabled before boot");
	name[0] = 'X';
	path[1] = 'X';
	expect(std::string_view{service.removablePolicy()->deviceName} == "sd1"
	        && std::string_view{service.removablePolicy()->mountPath} == "/mnt/sd",
	    "snapshot borrows caller strings");
	service.writeArmed = true;
	service.autoSuppressed = true;
	service.autoEnabled = false;
	expect(service.configure(hardware::picoCalcLyraStoragePolicy), "identical reconfiguration rejected");
	expect(service.writeArmed && service.autoSuppressed && !service.autoEnabled, "reconfiguration reset live state");
	snapshot = hardware::picoCalcLyraStoragePolicy;
	snapshot[1].readOnlyByDefault = true;
	expect(!service.configure(snapshot) && service.writeArmed, "live policy changed");
	// Explicit RO and write failures take priority over board-default RW.
	lexxos::app::rk3506::PicoSdServiceState writeMode;
	expect(writeMode.configure(hardware::picoCalcLyraStoragePolicy), "RW state configuration failed");
	writeMode.armForMount();
	expect(writeMode.writeArmed, "default RW not armed");
	writeMode.writeArmed = false;
	writeMode.armForMount();
	expect(writeMode.writeArmed, "default RW not restored after unmount");
	writeMode.manualReadOnly = true;
	writeMode.armForMount();
	expect(!writeMode.writeArmed, "explicit RO overridden by default RW");
	writeMode.manualReadOnly = false;
	writeMode.writeFaulted = true;
	writeMode.armForMount();
	expect(!writeMode.writeArmed, "failed write automatically rearmed");
	// Only a confirmed new media session may release a failed-write latch.
	writeMode.cardRemoved();
	writeMode.armForMount();
	expect(writeMode.writeArmed && !writeMode.writeFaulted, "physical removal did not clear fault");

	// A manually unmounted card must not remount by itself; new media may.
	writeMode.autoSuppressed = true;
	expect(writeMode.autoSuppressed, "manual-unmount suppression missing");
	writeMode.cardRemoved();
	writeMode.autoSuppressed = false; // Debounced removal in the hotplug worker.
	writeMode.armForMount();
	expect(writeMode.writeArmed && !writeMode.autoSuppressed,
	    "newly inserted card did not restore default RW policy");

	lexxos::app::rk3506::PicoSdServiceState readOnlyService;
	snapshot[1].autoMount = false;
	expect(readOnlyService.configure(snapshot) && !readOnlyService.autoEnabled && !readOnlyService.writeArmed && readOnlyService.removablePolicy()->readOnlyByDefault, "RO/manual policy lost");
}
