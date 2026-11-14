// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <deque>
#include <optional>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "common/host_memory.h"
#include "common/memory_attribution.h"
#include "common/scope_exit.h"
#include "common/slot_vector.h"
#include "tests/common/host_memory_test_access.h"
#include "video_core/texture_cache/image_attribution.h"
#include "video_core/texture_cache/pending_download.h"

namespace {

constexpr u64 MiB(u64 mib) noexcept {
    return mib * 1024ULL * 1024ULL;
}

constexpr u64 KiB(u64 kib) noexcept {
    return kib * 1024ULL;
}

void SetEnvVar(const char* name, const char* value) {
#ifdef _WIN32
    _putenv_s(name, value != nullptr ? value : "");
#else
    if (value != nullptr) {
        setenv(name, value, 1);
    } else {
        unsetenv(name);
    }
#endif
}

struct FakeVulkanImage {
    u64 base_alloc{};
    u64 sparse_alloc{};
    u64 scaled_alloc{};
    bool has_scaled{};
    int marker{};

    [[nodiscard]] bool HasScaled() const noexcept {
        return has_scaled;
    }

    [[nodiscard]] u64 AllocationBytes() const noexcept {
        return base_alloc + sparse_alloc + scaled_alloc;
    }
};

struct FakeFallbackImage {
    bool has_scaled{};

    [[nodiscard]] bool HasScaled() const noexcept {
        return has_scaled;
    }
};

} // namespace

TEST_CASE("MemoryAttribution switch parser and immutable once-latch",
          "[common][memory_attribution]") {
    using Common::MemoryAttribution::Enabled;
    using Common::MemoryAttribution::ParseEnabled;
    REQUIRE_FALSE(ParseEnabled(nullptr));
    REQUIRE_FALSE(ParseEnabled(""));
    REQUIRE_FALSE(ParseEnabled("0"));
    REQUIRE_FALSE(ParseEnabled("00"));
    REQUIRE_FALSE(ParseEnabled("false"));
    REQUIRE(ParseEnabled("1"));
    REQUIRE(ParseEnabled("2"));

    const char* raw_prev = std::getenv("EDEN_NX2_MEMORY_ATTRIBUTION");
    const std::optional<std::string> prev =
        raw_prev != nullptr ? std::optional<std::string>{raw_prev} : std::nullopt;
    SCOPE_EXIT {
        SetEnvVar("EDEN_NX2_MEMORY_ATTRIBUTION", prev.has_value() ? prev->c_str() : nullptr);
    };

    const bool latched = Enabled();
    SetEnvVar("EDEN_NX2_MEMORY_ATTRIBUTION", latched ? "0" : "1");
    CHECK(Enabled() == latched);
}

TEST_CASE("MemoryAttribution PeriodTimer fires once per 10-second window",
          "[common][memory_attribution]") {
    using namespace std::chrono_literals;
    Common::MemoryAttribution::PeriodTimer timer;
    const auto t0 = std::chrono::steady_clock::time_point{100s};

    // First observation arms the 10-second window.
    REQUIRE_FALSE(timer.Ready(t0));
    REQUIRE_FALSE(timer.Ready(t0 + 9999ms));
    REQUIRE(timer.Ready(t0 + 10s));
    REQUIRE_FALSE(timer.Ready(t0 + 10s));
    REQUIRE_FALSE(timer.Ready(t0 + 19999ms));
    REQUIRE(timer.Ready(t0 + 20s));

    // A long pause emits one line and schedules the next 10s later rather than bursting.
    REQUIRE(timer.Ready(t0 + 55s));
    REQUIRE_FALSE(timer.Ready(t0 + 55s));
    REQUIRE_FALSE(timer.Ready(t0 + 64999ms));
    REQUIRE(timer.Ready(t0 + 65s));
}

TEST_CASE("MemoryAttribution formats a single MiB line covering all eight sections",
          "[common][memory_attribution]") {
    using Common::MemoryAttribution::FormatReport;
    using Common::MemoryAttribution::Report;

    const Report report{
        .process_commit_bytes = MiB(6144),
        .process_working_set_bytes = MiB(4096),
        .process_private_working_set_bytes = MiB(2560),
        .guest_committed_bytes = MiB(8192),
        .guest_handed_out_bytes = MiB(3584),
        .live_images = 412,
        .live_image_bytes = MiB(1536) + KiB(512),
        .image_bytes_estimated = false,
        .texture_expected_bytes = MiB(2048),
        .texture_critical_bytes = MiB(3072),
        .texture_minimum_bytes = MiB(1024),
        .retired_image_bytes = MiB(64),
        .retired_buffer_bytes = MiB(32) + KiB(512),
        .stream_staging_bytes = MiB(128),
        .upload_staging_bytes = MiB(48),
        .download_staging_bytes = MiB(16),
        .device_staging_bytes = MiB(8),
        .transcode_in_flight_bytes = MiB(12),
        .transcode_idle_bytes = MiB(20),
        .live_buffers = 1850,
        .buffer_cache_bytes = MiB(512),
        .multi_range_gathered_bytes = MiB(24),
        .shader_cache_entries = 940,
        .shader_cache_owned_bytes = MiB(14),
        .shader_cache_v2_entries = 720,
        .shader_cache_v2_bytes = MiB(22),
        .shader_reuse_entries = 310,
        .shader_reuse_bytes = MiB(9),
        .kept_spirv_entries = 45,
        .kept_spirv_bytes = MiB(3) + KiB(512),
        .graphics_pipelines = 610,
        .retired_graphics_pipelines = 18,
        .graphics_libraries = 420,
        .compute_pipelines = 135,
        .retired_compute_pipelines = 4,
        .in_flight_ir_bytes = MiB(16),
        .driver_cache_blob_bytes = MiB(96) + KiB(512),
        .pipeline_binary_index_entries = 745,
    };

    const std::string periodic = FormatReport("10s", report);
    REQUIRE(periodic.find('\n') == std::string::npos);
    REQUIRE(periodic.find('\r') == std::string::npos);
    REQUIRE(periodic ==
            "MemoryAttribution(10s): "
            "proc commit=6144.0 ws=4096.0 priv_ws=2560.0 MiB | "
            "guest committed=8192.0 MiB handed_out=3584.0 MiB | "
            "tex_live=1536.5 MiB (412 imgs, thresholds exp=2048.0 crit=3072.0 min=1024.0 MiB) | "
            "retire img=64.0 MiB buf=32.5 MiB | "
            "pools staging=200.0 (stream=128.0 up=48.0 down=16.0 dev=8.0) "
            "transcode=32.0 (flight=12.0 idle=20.0) MiB | "
            "buf_cache=536.0 MiB (1850 bufs, gather=24.0 MiB) | "
            "shaders cache=940/14.0 MiB disk_v2=720/22.0 MiB reuse=310/9.0 MiB "
            "kept_spirv=45/3.5 MiB gfx_pipe=610+18ret gpl=420 comp_pipe=135+4ret "
            "in_flight_ir=16.0 MiB | "
            "driver_cache=96.5 MiB (vk_bin_idx=745)");

    Report fallback_report{};
    fallback_report.guest_committed_bytes = std::nullopt;
    fallback_report.guest_handed_out_bytes = MiB(256);
    fallback_report.live_images = 3;
    fallback_report.live_image_bytes = MiB(12);
    fallback_report.image_bytes_estimated = true;
    fallback_report.retired_image_bytes = MiB(4);

    const std::string shutdown = FormatReport("shutdown", fallback_report);
    REQUIRE(shutdown.starts_with("MemoryAttribution(shutdown): "));
    REQUIRE(shutdown.find('\n') == std::string::npos);
    CHECK(shutdown.find("guest committed=n/a handed_out=256.0 MiB") != std::string::npos);
    CHECK(shutdown.find("tex_live~=12.0 MiB (est, 3 imgs") != std::string::npos);
    CHECK(shutdown.find("retire img~=4.0 MiB est buf=0.0 MiB") != std::string::npos);
}

TEST_CASE("HostMemory distinguishes handed-out granules from OS-committed backing",
          "[common][memory_attribution]") {
    constexpr size_t kBacking = static_cast<size_t>(MiB(2));
    constexpr size_t kVirtual = static_cast<size_t>(MiB(64));

    if (!Common::MemoryAttribution::Enabled()) {
        Common::HostMemory disabled_mem{kBacking, kVirtual};
        REQUIRE(disabled_mem.BackingBasePointer() != nullptr);
        disabled_mem.ClearBackingRegion(0, static_cast<size_t>(KiB(128)), 0);
        CHECK(disabled_mem.HandedOutBytes() == 0);
    }

    Common::HostMemory mem{kBacking, kVirtual};
    REQUIRE(mem.BackingBasePointer() != nullptr);
    Common::HostMemoryTestAccess::EnableAttribution(mem);

    // Zero-clearing fresh backing marks granules handed out without touching zero pages.
    mem.ClearBackingRegion(0, static_cast<size_t>(KiB(128)), 0);
    CHECK(mem.HandedOutBytes() == KiB(128));
    mem.ClearBackingRegion(0, static_cast<size_t>(KiB(64)), 0);
    CHECK(mem.HandedOutBytes() == KiB(128));

    const std::optional<u64> untouched_committed = mem.CommittedBytes();
#ifdef _WIN32
    REQUIRE(untouched_committed.has_value());
    CHECK(*untouched_committed == MiB(2));
    CHECK(*untouched_committed != mem.HandedOutBytes());
#elif defined(__linux__)
    if (mem.VirtualBasePointer() != nullptr) {
        REQUIRE(untouched_committed.has_value());
        CHECK(*untouched_committed == 0);
    }
#endif

    // Non-zero fill materializes backing across the whole 2 MiB buffer.
    mem.ClearBackingRegion(0, kBacking, 0x5a5a5a5au);
    CHECK(mem.HandedOutBytes() == MiB(2));
#if defined(_WIN32) || defined(__linux__)
    if (mem.VirtualBasePointer() != nullptr) {
        const std::optional<u64> dirty_committed = mem.CommittedBytes();
        REQUIRE(dirty_committed.has_value());
        CHECK(*dirty_committed == MiB(2));
    }
#endif

    // Zeroing dirty granules keeps HandedOutBytes monotonic while releasing tmpfs pages on Linux.
    mem.ClearBackingRegion(0, kBacking, 0);
    CHECK(mem.HandedOutBytes() == MiB(2));
#ifdef __linux__
    if (mem.VirtualBasePointer() != nullptr) {
        const char* punch = std::getenv("EDEN_PUNCH_HOLE");
        if (punch == nullptr || punch[0] != '0') {
            const std::optional<u64> punched_committed = mem.CommittedBytes();
            REQUIRE(punched_committed.has_value());
            CHECK(*punched_committed == 0);
        }
    }
#endif

    // Fallback anonymous buffer without fastmem/memfd reports nullopt outside Windows rather than
    // duplicating HandedOutBytes as committed OS memory.
    Common::HostMemoryTestAccess fallback_ctl{false};
    Common::HostMemory fallback{0x10000, 0x400000};
    REQUIRE(fallback.BackingBasePointer() != nullptr);
    Common::HostMemoryTestAccess::EnableAttribution(fallback);
    fallback.ClearBackingRegion(0, 0x10000, 0);
    CHECK(fallback.HandedOutBytes() == 0x10000);
#ifdef _WIN32
    REQUIRE(fallback.CommittedBytes().has_value());
    CHECK(*fallback.CommittedBytes() == 0x10000);
#else
    CHECK_FALSE(fallback.CommittedBytes().has_value());
#endif
}

TEST_CASE("ImageAttributionTracker uses actual base/sparse/scaled allocations over GC estimates",
          "[common][memory_attribution]") {
    using Download = VideoCommon::PendingImageDownload<FakeVulkanImage>;
    VideoCommon::ImageAttributionTracker<8> tracker;

    // Guest ASTC size is 1 MiB and 2x GC scaled estimate is 4 MiB, whereas actual host RGBA8
    // Vulkan allocations are 6 MiB (base) and 24 MiB (scaled).
    constexpr u64 kGcBaseEstimate = MiB(1);
    constexpr u64 kGcScaledEstimate = MiB(4);

    FakeVulkanImage img{
        .base_alloc = MiB(6),
        .sparse_alloc = 0,
        .scaled_alloc = 0,
        .has_scaled = false,
        .marker = 7,
    };

    tracker.Register(img, kGcBaseEstimate, 0);
    CHECK(tracker.LiveImages() == 1);
    CHECK(tracker.LiveImageBytes() == MiB(6));
    CHECK_FALSE(tracker.IsEstimated());

    // ScaleUp allocates the 24 MiB scaled VkImage.
    const u64 before_scale = tracker.SnapshotLiveBytes(img, true, kGcBaseEstimate, 0);
    img.has_scaled = true;
    img.scaled_alloc = MiB(24);
    tracker.UpdateLiveBytes(img, true, before_scale, kGcBaseEstimate, kGcScaledEstimate);
    CHECK(tracker.LiveImageBytes() == MiB(30));

    // Sparse tile bind adds 8 MiB of bound memory.
    const u64 before_sparse =
        tracker.SnapshotLiveBytes(img, true, kGcBaseEstimate, kGcScaledEstimate);
    img.sparse_alloc = MiB(8);
    tracker.UpdateLiveBytes(img, true, before_sparse, kGcBaseEstimate, kGcScaledEstimate);
    CHECK(tracker.LiveImageBytes() == MiB(38));

    // Unregister removes the exact 38 MiB live allocation.
    tracker.Unregister(img, kGcBaseEstimate, kGcScaledEstimate);
    CHECK(tracker.LiveImages() == 0);
    CHECK(tracker.LiveImageBytes() == 0);

    // Retained pending downloads hold the full 38 MiB allocation once across all download refs.
    Common::SlotVector<FakeVulkanImage> slot_images;
    const Common::SlotId target_id = slot_images.insert(img);
    const Common::SlotId other_id = slot_images.insert(FakeVulkanImage{.base_alloc = MiB(2)});

    std::vector<Download> pending{
        Download{.is_swizzle = true, .async_buffer_id = 0, .object_id = target_id},
        Download{.is_swizzle = true, .async_buffer_id = 1, .object_id = other_id},
    };
    std::deque<std::vector<Download>> submitted{
        std::vector<Download>{
            Download{.is_swizzle = true, .async_buffer_id = 2, .object_id = target_id},
        },
    };

    const auto retained_size = VideoCommon::QueryImageAttributionSize(
        slot_images[target_id], kGcBaseEstimate, kGcScaledEstimate);
    REQUIRE(retained_size.bytes == MiB(38));
    REQUIRE_FALSE(retained_size.estimated);

    REQUIRE(VideoCommon::RetainPendingImageDownloads(target_id, slot_images[target_id], pending,
                                                     submitted, tracker.RetainedDownloadCounter(),
                                                     retained_size.bytes));
    tracker.MarkRetainedDownload(retained_size);
    slot_images.erase(target_id);

    CHECK(tracker.RetiredImageBytes() == MiB(38));
    CHECK(pending[0].GetImage(slot_images).marker == 7);
    CHECK(submitted[0][0].GetImage(slot_images).marker == 7);
    pending.clear();
    CHECK(tracker.RetiredImageBytes() == MiB(38));
    submitted.clear();
    CHECK(tracker.RetiredImageBytes() == 0);

    // GPU-sentenced retirement tracks the actual allocation separately from GC freed_bytes.
    tracker.SentenceGpu(retained_size);
    CHECK(tracker.RetiredImageBytes() == MiB(38));
    tracker.RetireGpu(retained_size.bytes);
    CHECK(tracker.RetiredImageBytes() == 0);

    // Backends without AllocationBytes() fall back to GC estimates and mark IsEstimated().
    VideoCommon::ImageAttributionTracker<4> fallback_tracker;
    const FakeFallbackImage fb{.has_scaled = true};
    fallback_tracker.Register(fb, MiB(2), MiB(6));
    CHECK(fallback_tracker.IsEstimated());
    CHECK(fallback_tracker.LiveImageBytes() == MiB(8));
    fallback_tracker.Unregister(fb, MiB(2), MiB(6));
    CHECK(fallback_tracker.LiveImageBytes() == 0);

    const auto fb_size = VideoCommon::QueryImageAttributionSize(fb, MiB(2), MiB(6));
    CHECK(fb_size.estimated);
    fallback_tracker.SentenceRing(1, fb_size);
    CHECK(fallback_tracker.RetiredImageBytes() == MiB(8));
    fallback_tracker.AdvanceRingSlot(0);
    CHECK(fallback_tracker.RetiredImageBytes() == MiB(8));
    fallback_tracker.AdvanceRingSlot(1);
    CHECK(fallback_tracker.RetiredImageBytes() == 0);
}
