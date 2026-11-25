#include <lexxos/apps/storage/detail/file_benchmark.h>

#include <algorithm>
#include <array>

#include <lexxos/apps/common/core_programs_support.h>
#include <lexxos/os/app_context.h>
#include <lexxos/vfs/vfs.h>

namespace lexxos::apps::storage_programs::detail {

namespace {

	char pattern(std::uint64_t offset) {
		// Двоичный шаблон зависит от всех битов смещения, включая номер кластера.
		auto value = offset + 0x9E3779B97F4A7C15ULL;
		value = (value ^ (value >> 30U)) * 0xBF58476D1CE4E5B9ULL;
		value = (value ^ (value >> 27U)) * 0x94D049BB133111EBULL;
		return static_cast<char>(value ^ (value >> 31U));
	}

	bool cancelled(lexxos::os::AppContext& ctx, std::uint32_t delay) {
		while (delay && !ctx.cancellationRequested()) {
			auto const slice = std::min(delay, static_cast<std::uint32_t>(25U));
			sleepMs(ctx, slice);
			delay -= slice;
		}

		return ctx.cancellationRequested();
	}

	int error(lexxos::os::AppContext& ctx, std::string_view reason) {
		ctx.write("sd-bench: ");
		ctx.writeLine(reason);
		return 1;
	}

	bool verify(std::span<char> data, std::size_t size, std::uint64_t offset) {
		for (std::size_t i = 0; i < size; ++i) {
			if (data[i] != pattern(offset + i)) {
				return false;
			}
		}

		return true;
	}

	int readPass(lexxos::os::AppContext& ctx, std::string_view path, std::uint64_t size,
	    std::size_t chunk, bool generated, std::uint32_t delay, std::span<char> buffer, std::uint32_t& hash) {
		lexxos::vfs::FileHandle file;
		if (ctx.vfs().open(path, lexxos::vfs::OpenMode::Read, file) != lexxos::vfs::OpenStatus::Ok) {
			return error(ctx, "cannot open benchmark file");
		}

		hash = 2166136261U;
		for (std::uint64_t offset = 0; offset < size;) {
			if (cancelled(ctx, 0)) {
				return 130;
			}

			auto const count = static_cast<std::size_t>(std::min<std::uint64_t>(chunk, size - offset));
			std::size_t read{};
			if (file.read(buffer.data(), count, read) != lexxos::vfs::ReadStatus::Ok || read != count) {
				return error(ctx, "short read or file read error");
			}

			if (generated && !verify(buffer, read, offset)) {
				return error(ctx, "file pattern mismatch");
			}

			for (std::size_t i = 0; i < read; ++i) {
				hash = (hash ^ static_cast<unsigned char>(buffer[i])) * 16777619U;
			}

			offset += read;
			if (cancelled(ctx, delay)) {
				return 130;
			}
		}

		std::size_t read{};
		if (file.read(buffer.data(), 1, read) != lexxos::vfs::ReadStatus::Ok || read != 0) {
			return error(ctx, "unexpected data after file end");
		}

		return 0;
	}

	int boundaryReads(lexxos::os::AppContext& ctx, std::string_view path, std::uint32_t bytes,
	    std::uint32_t cluster, std::span<char> buffer) {
		lexxos::vfs::FileHandle file;
		if (ctx.vfs().open(path, lexxos::vfs::OpenMode::Read, file) != lexxos::vfs::OpenStatus::Ok) {
			return error(ctx, "cannot reopen file for boundary checks");
		}

		// Пересекаем границы секторов, кластеров и хвоста файла отдельными seek/read.
		std::uint64_t const positions[]{0, 511, 512, 513, cluster ? cluster - 1ULL : 0,
		    cluster, std::uint64_t(cluster) + 1U, std::uint64_t(cluster) * 2U - (cluster ? 1U : 0U), bytes - 1ULL};
		for (auto offset : positions) {
			if (offset >= bytes) {
				continue;
			}

			if (ctx.cancellationRequested()) {
				return 130;
			}

			auto const count = static_cast<std::size_t>(std::min<std::uint64_t>(513, bytes - offset));
			std::size_t read{};
			if (!file.seek(offset) || file.read(buffer.data(), count, read) != lexxos::vfs::ReadStatus::Ok || read != count || !verify(buffer, read, offset)) {
				return error(ctx, "boundary seek/read mismatch");
			}
		}

		return 0;
	}

} // namespace

int FileBenchmark::run(lexxos::os::AppContext& ctx, std::string_view path, bool write,
    std::uint32_t bytes, std::uint32_t clusterBytes, std::uint32_t delayMs, std::span<char> buffer) {
	if (buffer.size() < 513 || bytes == 0) {
		return error(ctx, "invalid file benchmark workspace/size");
	}

	std::array<char, lexxos::vfs::maxPathLength> filePath{};
	bool owned = false;
	if (write) {
		lexxos::vfs::Stat existing{};
		if (ctx.vfs().stat(path, existing)) {
			return error(ctx, "test directory already exists; refusing to overwrite");
		}

		constexpr std::string_view suffix = "/data.bin";
		if (path.size() + suffix.size() >= filePath.size()) {
			return error(ctx, "test path too long");
		}

		std::copy(path.begin(), path.end(), filePath.begin());
		std::copy(suffix.begin(), suffix.end(), filePath.begin() + path.size());
		if (ctx.vfs().createDirectory(path) != lexxos::vfs::WriteStatus::Ok) {
			return error(ctx, "cannot create new test directory");
		}

		owned = true;
	} else {
		if (path.size() >= filePath.size()) {
			return error(ctx, "file path too long");
		}

		std::copy(path.begin(), path.end(), filePath.begin());
	}

	auto run = [&]() -> int {
		auto const started = uptimeMs(ctx);
		if (write) {
			lexxos::vfs::FileHandle output;
			if (ctx.vfs().open(filePath.data(), lexxos::vfs::OpenMode::CreateTruncateWrite, output) != lexxos::vfs::OpenStatus::Ok) {
				return error(ctx, "cannot create test file");
			}

			for (std::uint32_t offset = 0; offset < bytes;) {
				if (cancelled(ctx, 0)) {
					return 130;
				}

				auto const count = std::min<std::size_t>(buffer.size(), bytes - offset);
				for (std::size_t i = 0; i < count; ++i) {
					buffer[i] = pattern(offset + i);
				}

				if (output.write({buffer.data(), count}) != lexxos::vfs::WriteStatus::Ok) {
					return error(ctx, "file write failed");
				}
				offset += static_cast<std::uint32_t>(count);
				if (cancelled(ctx, delayMs)) {
					return 130;
				}
			}

			output.close();
			if (ctx.vfs().sync() != lexxos::vfs::SyncStatus::Ok) {
				return error(ctx, "file sync failed");
			}
		}

		lexxos::vfs::Stat stat{};
		if (!ctx.vfs().stat(filePath.data(), stat) || stat.type != lexxos::vfs::NodeType::RegularFile) {
			return error(ctx, "file stat failed");
		}

		if (write && stat.sizeBytes != bytes) {
			return error(ctx, "file size mismatch");
		}

		std::uint32_t firstHash{}, secondHash{};
		auto status = readPass(ctx, filePath.data(), stat.sizeBytes, buffer.size(), write, delayMs, buffer, firstHash);
		if (status) {
			return status;
		}

		status = readPass(ctx, filePath.data(), stat.sizeBytes, 513, write, delayMs, buffer, secondHash);
		if (status) {
			return status;
		}

		if (firstHash != secondHash) {
			return error(ctx, "file changed between read passes");
		}

		if (write && (status = boundaryReads(ctx, filePath.data(), bytes, clusterBytes, buffer))) {
			return status;
		}

		ctx.write("sd-bench: file verified bytes=");
		writeUnsigned64(ctx, stat.sizeBytes);
		ctx.write(" cluster-bytes=");
		writeUnsigned(ctx, clusterBytes);
		ctx.write(" elapsed-ms=");
		writeUnsigned(ctx, uptimeMs(ctx) - started);
		ctx.writeLine("");
		return 0;
	};
	auto const result = run();
	// При ошибке сохраняем данные для разбора; удаляем только успешно проверенный собственный файл.
	if (owned && result == 0) {
		if (ctx.vfs().remove(filePath.data()) != lexxos::vfs::RemoveStatus::Ok || ctx.vfs().removeDirectory(path) != lexxos::vfs::RemoveStatus::Ok || ctx.vfs().sync() != lexxos::vfs::SyncStatus::Ok) {
			return error(ctx, "test cleanup failed");
		}
	} else if (owned) {
		ctx.write("sd-bench: test data retained in ");
		ctx.writeLine(path);
	}

	return result;
}

} // namespace lexxos::apps::storage_programs::detail
