#pragma once

#include <string_view>

namespace lexxos::os {

// Политика ручного монтирования. Срок жизни задаёт composition root.
// Не выполняет I/O файловой системы; завершение получает результат операции.
class StorageMountPolicy {
public:
	virtual ~StorageMountPolicy() = default;
	[[nodiscard]] virtual std::string_view rejectionReason() const = 0;
	virtual bool prepareMount(std::string_view source, std::string_view target) = 0;
	virtual void mountFinished(std::string_view source, std::string_view target, bool success) = 0;
	virtual void unmounted(std::string_view target) = 0;
};

} // namespace lexxos::os
