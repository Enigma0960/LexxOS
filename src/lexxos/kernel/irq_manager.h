#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>

#include <lexxos/kernel/detail/irq_update_guard.h>
#include <lexxos/kernel/irq.h>
#include <lexxos/kernel/irq_controller.h>

namespace lexxos::kernel {

// Контроллер реализуется адаптером CPU/SoC/SDK. Статическая таблица векторов
// может вызывать IrqManager::dispatch() через ISR, подключённый при линковке.

// Реестр подписок фиксированной ёмкости без выделения памяти. Ёмкость задаётся
// профилем памяти продукта. Этот слой не изменяет таблицу векторов и не использует
// глобальные IRQ ID, RTOS API или singleton-объекты. Контракт beginUpdate/endUpdate
// контроллера исключает одновременную диспетчеризацию и изменение подписок;
// сама диспетчеризация не захватывает блокировки.
template <std::size_t Capacity>
class IrqManager final : public IrqService {
	static_assert(Capacity > 0 && Capacity < std::numeric_limits<std::uint32_t>::max());

public:
	explicit IrqManager(IrqController& controller)
	    : _controller(controller) {}

	[[nodiscard]] std::uint32_t domainCount() const override { return _controller.domainCount(); }

	[[nodiscard]] IrqStatus domainAt(std::uint32_t index, IrqDomainInfo& output) const override {
		return _controller.domainAt(index, output);
	}

	[[nodiscard]] IrqStatus securityInfo(IrqSourceId source, IrqSecurityInfo& output) const override {
		return _controller.securityInfo(source, output);
	}

	[[nodiscard]] IrqStatus subscribe(IrqRequest const& request, IrqSubscription& output) override {
		if (!request.source.valid() || request.owner == nullptr || request.handler == nullptr) {
			return IrqStatus::InvalidArgument;
		}

		UpdateGuard const guard{_controller};
		IrqDomainInfo domain{};
		IrqStatus const domainStatus = findDomain(request.source.domain, domain);
		if (domainStatus != IrqStatus::Ok) {
			return domainStatus;
		}

		if (!domain.runtimeSubscriptions) {
			return IrqStatus::Unsupported;
		}

		if (request.sharing == IrqSharing::Shared && !domain.sharedSubscriptions) {
			return IrqStatus::Unsupported;
		}

		if ((request.trigger != IrqTrigger::Preserve && !domain.configurableTrigger)
		    || (request.priority != IrqPriority::Preserve && !domain.configurablePriority)
		    || (request.cpuAffinity != anyIrqCpu && !domain.configurableAffinity)) {
			return IrqStatus::Unsupported;
		}

		bool alreadyAttached = false;
		for (auto const& slot : _slots) {
			if (!slot.active || slot.request.source != request.source) {
				continue;
			}

			alreadyAttached = true;

			// Конфигурация линии — общее аппаратное состояние. Несовместимые
			// настройки отклоняются без изменения настроек текущего владельца,
			// в том числе при сочетании Preserve с явным значением.
			if (slot.request.sharing != IrqSharing::Shared || request.sharing != IrqSharing::Shared || slot.request.trigger != request.trigger || slot.request.priority != request.priority || slot.request.cpuAffinity != request.cpuAffinity) {
				return IrqStatus::Busy;
			}
		}

		std::size_t freeIndex = Capacity;
		for (std::size_t index = 0; index < Capacity; ++index) {
			if (!_slots[index].active && _slots[index].generation != std::numeric_limits<std::uint32_t>::max()) {
				freeIndex = index;
				break;
			}
		}

		if (freeIndex == Capacity) {
			return IrqStatus::NoCapacity;
		}

		if (!alreadyAttached) {
			IrqStatus const status = _controller.attachSource(request);
			if (status != IrqStatus::Ok) {
				return status;
			}
		}

		auto& slot = _slots[freeIndex];
		++slot.generation; // Нулевое поколение пропускается; переполнение исключено.
		slot.request = request;
		slot.enabled = false;
		slot.active = true;
		output = {request.source, static_cast<std::uint32_t>(freeIndex + 1U), slot.generation};
		return IrqStatus::Ok;
	}

	[[nodiscard]] IrqStatus setEnabled(
	    IrqSubscription const& subscription, void const* owner, bool enabled) override {
		if (!subscription.valid() || owner == nullptr) {
			return IrqStatus::InvalidArgument;
		}

		UpdateGuard const guard{_controller};
		Slot* slot = findSlot(subscription);
		if (slot == nullptr) {
			return IrqStatus::StaleHandle;
		}

		if (slot->request.owner != owner) {
			return IrqStatus::AccessDenied;
		}

		if (slot->enabled == enabled) {
			return IrqStatus::Ok;
		}

		bool anyOtherEnabled = false;
		for (auto const& candidate : _slots) {
			if (&candidate != slot && candidate.active && candidate.enabled && candidate.request.source == subscription.source) {
				anyOtherEnabled = true;
				break;
			}
		}

		if (!anyOtherEnabled) {
			IrqStatus const status = _controller.setLineEnabled(subscription.source, enabled);
			if (status != IrqStatus::Ok) {
				return status;
			}
		}

		slot->enabled = enabled;
		return IrqStatus::Ok;
	}

	[[nodiscard]] IrqStatus unsubscribe(
	    IrqSubscription const& subscription, void const* owner) override {
		if (!subscription.valid() || owner == nullptr) {
			return IrqStatus::InvalidArgument;
		}

		UpdateGuard const guard{_controller};
		Slot* slot = findSlot(subscription);
		if (slot == nullptr) {
			return IrqStatus::StaleHandle;
		}

		if (slot->request.owner != owner) {
			return IrqStatus::AccessDenied;
		}

		bool anyOther = false;
		bool anyOtherEnabled = false;
		for (auto const& candidate : _slots) {
			if (&candidate == slot || !candidate.active || candidate.request.source != subscription.source) {
				continue;
			}

			anyOther = true;
			anyOtherEnabled = anyOtherEnabled || candidate.enabled;
		}

		if (!anyOther) {
			IrqStatus const status = _controller.detachSource(subscription.source);
			if (status != IrqStatus::Ok) {
				return status;
			}
		} else if (slot->enabled && !anyOtherEnabled) {
			IrqStatus const status = _controller.setLineEnabled(subscription.source, false);
			if (status != IrqStatus::Ok) {
				return status;
			}
		}

		slot->request = {};
		slot->enabled = false;
		slot->active = false;
		// Сохраняем поколение, чтобы повторное использование слота не оживило старый дескриптор.
		return IrqStatus::Ok;
	}

	// Вызывается только из платформенного ISR после определения источника,
	// включая демультиплексирование каскадных прерываний. Backend отвечает за ACK
	// периферии, GIC/NVIC EOI и защиту от шторма прерываний. Вызываем всех включённых
	// подписчиков даже после Handled или Reschedule, чтобы устройства общей линии
	// могли независимо проверить и сбросить свой статус. Callback не должен
	// изменять подписки или уничтожать другой callback.
	[[nodiscard]] IrqDisposition dispatch(IrqSourceId source) noexcept {
		IrqDisposition result = IrqDisposition::NotHandled;
		if (!source.valid()) {
			return result;
		}

		for (auto const& slot : _slots) {
			if (!slot.active || !slot.enabled || slot.request.source != source) {
				continue;
			}

			IrqDisposition const disposition = slot.request.handler(source, slot.request.userData);
			if (disposition == IrqDisposition::Reschedule) {
				result = IrqDisposition::Reschedule;
			} else if (disposition == IrqDisposition::Handled && result == IrqDisposition::NotHandled) {
				result = IrqDisposition::Handled;
			}
		}

		return result;
	}

private:
	struct Slot {
		IrqRequest request{};
		std::uint32_t generation{};
		bool active{};
		bool enabled{};
	};

	using UpdateGuard = detail::IrqUpdateGuard;

	[[nodiscard]] IrqStatus findDomain(IrqDomainId id, IrqDomainInfo& output) const {
		for (std::uint32_t index = 0; index < _controller.domainCount(); ++index) {
			IrqDomainInfo info{};
			IrqStatus const status = _controller.domainAt(index, info);
			if (status != IrqStatus::Ok) {
				return status;
			}

			if (info.id == id) {
				output = info;
				return IrqStatus::Ok;
			}
		}

		return IrqStatus::NotFound;
	}

	[[nodiscard]] Slot* findSlot(IrqSubscription const& handle) noexcept {
		if (handle.slot == 0U || handle.slot > Capacity) {
			return nullptr;
		}

		auto& slot = _slots[handle.slot - 1U];
		return slot.active && slot.generation == handle.generation && slot.request.source == handle.source
		    ? &slot
		    : nullptr;
	}

	IrqController& _controller;
	std::array<Slot, Capacity> _slots{};
};

} // namespace lexxos::kernel
