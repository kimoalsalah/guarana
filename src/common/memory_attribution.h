// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <chrono>
#include <cstdlib>
#include <optional>
#include <string>
#include <string_view>

#include "common/common_types.h"

namespace Common::MemoryAttribution {

[[nodiscard]] inline bool ParseEnabled(const char* value) noexcept {
    return value != nullptr && std::strtoul(value, nullptr, 10) != 0;
}

[[nodiscard]] inline bool ReadEnvEnabled() noexcept {
    return ParseEnabled(std::getenv("EDEN_NX2_MEMORY_ATTRIBUTION"));
}

/// Latched once from EDEN_NX2_MEMORY_ATTRIBUTION (eagerly right after LoadNx2Switches in the Qt
/// frontend before any HostMemory or GPU owner exists, or on first query in CLI/headless/tests)
/// and never mutated afterward.
[[nodiscard]] inline bool Enabled() noexcept {
    static const bool enabled = ReadEnvEnabled();
    return enabled;
}

[[nodiscard]] constexpr double BytesToMiB(u64 bytes) noexcept {
    return static_cast<double>(bytes) / (1024.0 * 1024.0);
}

class PeriodTimer {
public:
    static constexpr std::chrono::seconds DefaultInterval{10};

    explicit constexpr PeriodTimer(
        std::chrono::steady_clock::duration interval_ = DefaultInterval) noexcept
        : interval{interval_} {}

    [[nodiscard]] bool Ready(std::chrono::steady_clock::time_point now) noexcept {
        if (!armed) {
            armed = true;
            next = now + interval;
            return false;
        }
        if (now < next) {
            return false;
        }
        next = now + interval;
        return true;
    }

private:
    std::chrono::steady_clock::duration interval{DefaultInterval};
    std::chrono::steady_clock::time_point next{};
    bool armed{};
};

struct Report {
    // 1. Process commit and working set.
    u64 process_commit_bytes{};
    u64 process_working_set_bytes{};
    u64 process_private_working_set_bytes{};

    // 2. Guest memory committed (nullopt where the OS backing cannot be queried cheaply) and
    //    64 KiB granules ever handed out by HostMemory::MarkUsed.
    std::optional<u64> guest_committed_bytes{};
    u64 guest_handed_out_bytes{};

    // 3. Live texture cache images and current cache thresholds.
    u64 live_images{};
    u64 live_image_bytes{};
    bool image_bytes_estimated{};
    u64 texture_expected_bytes{};
    u64 texture_critical_bytes{};
    u64 texture_minimum_bytes{};

    // 4. Images and buffers waiting for GPU retirement.
    u64 retired_image_bytes{};
    u64 retired_buffer_bytes{};

    // 5. Staging and transcode pools.
    u64 stream_staging_bytes{};
    u64 upload_staging_bytes{};
    u64 download_staging_bytes{};
    u64 device_staging_bytes{};
    u64 transcode_in_flight_bytes{};
    u64 transcode_idle_bytes{};

    // 6. Buffer cache (slot_buffers + non-sparse multi-range gathered buffers).
    u64 live_buffers{};
    u64 buffer_cache_bytes{};
    u64 multi_range_gathered_bytes{};

    // 7. Shader and pipeline maps (entry counts and, where known, bytes).
    u64 shader_cache_entries{};
    u64 shader_cache_owned_bytes{};
    u64 shader_cache_v2_entries{};
    u64 shader_cache_v2_bytes{};
    u64 shader_reuse_entries{};
    u64 shader_reuse_bytes{};
    u64 kept_spirv_entries{};
    u64 kept_spirv_bytes{};
    u64 graphics_pipelines{};
    u64 retired_graphics_pipelines{};
    u64 graphics_libraries{};
    u64 compute_pipelines{};
    u64 retired_compute_pipelines{};
    u64 in_flight_ir_bytes{};

    // 8. Driver pipeline cache blob (and VK_KHR_pipeline_binary in-memory index count).
    u64 driver_cache_blob_bytes{};
    u64 pipeline_binary_index_entries{};
};

[[nodiscard]] std::string FormatReport(std::string_view reason, const Report& report);

} // namespace Common::MemoryAttribution
