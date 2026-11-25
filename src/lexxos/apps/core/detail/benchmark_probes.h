#pragma once

#include <array>
#include <cstdint>
#include <string_view>

namespace lexxos::os {

class AppContext;

} // namespace lexxos::os

namespace lexxos::apps::system_programs::detail {

// Report sink keeps probes independent of console/file formatting and retention.
class BenchmarkMetrics {
public:
	virtual ~BenchmarkMetrics() = default;
	virtual void add(std::string_view section, std::string_view metric, std::string_view value,
	    std::string_view unit, std::string_view note = {}) = 0;
	void addUnsigned(std::string_view section, std::string_view metric, std::uint64_t value,
	    std::string_view unit, std::string_view note = {});
};

struct BenchmarkProbeWorkspace {
	alignas(4) std::array<std::uint8_t, 4096> reference{};
	alignas(4) std::array<std::uint8_t, 4096> transfer{};
};

void sampleBenchmarkNative(BenchmarkMetrics& report);
void sampleBenchmarkResources(lexxos::os::AppContext& context, BenchmarkMetrics& report);
void sampleBenchmarkDevices(lexxos::os::AppContext& context, BenchmarkMetrics& report);
void runBenchmarkStorage(lexxos::os::AppContext& context, BenchmarkMetrics& report,
    BenchmarkProbeWorkspace& workspace, std::string_view deviceName, std::uint64_t firstBlock,
    std::uint32_t blocks, std::string_view writeDirectory, std::uint32_t fileBytes);

} // namespace lexxos::apps::system_programs::detail
