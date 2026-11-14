// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>

#include "common/common_types.h"

namespace VideoCommon {

struct AttributionImageSize {
    u64 bytes{};
    bool estimated{};
};

/// Queries an image's actual device allocation bytes when the backend exposes AllocationBytes()
/// (e.g. Vulkan base + sparse + scaled memory blocks), falling back to the collector's estimate
/// (labeled estimated = true) on backends without allocation tracking.
template <typename ImageType>
[[nodiscard]] constexpr AttributionImageSize QueryImageAttributionSize(
    const ImageType& image, u64 fallback_base_bytes, u64 fallback_scaled_bytes) noexcept {
    if constexpr (requires { image.AllocationBytes(); }) {
        return {
            .bytes = image.AllocationBytes(),
            .estimated = false,
        };
    } else {
        return {
            .bytes = fallback_base_bytes + (image.HasScaled() ? fallback_scaled_bytes : 0),
            .estimated = true,
        };
    }
}

template <size_t TicksToDestroy>
class ImageAttributionTracker {
public:
    template <typename ImageType>
    void Register(const ImageType& image, u64 fallback_base_bytes,
                  u64 fallback_scaled_bytes) noexcept {
        const auto size =
            QueryImageAttributionSize(image, fallback_base_bytes, fallback_scaled_bytes);
        ++live_images;
        live_bytes += size.bytes;
        estimated = estimated || size.estimated;
    }

    template <typename ImageType>
    [[nodiscard]] u64 SnapshotLiveBytes(const ImageType& image, bool is_registered,
                                        u64 fallback_base_bytes,
                                        u64 fallback_scaled_bytes) const noexcept {
        if (!is_registered) {
            return 0;
        }
        return QueryImageAttributionSize(image, fallback_base_bytes, fallback_scaled_bytes).bytes;
    }

    template <typename ImageType>
    void UpdateLiveBytes(const ImageType& image, bool is_registered, u64 before_bytes,
                         u64 fallback_base_bytes, u64 fallback_scaled_bytes) noexcept {
        if (!is_registered) {
            return;
        }
        const auto after =
            QueryImageAttributionSize(image, fallback_base_bytes, fallback_scaled_bytes);
        estimated = estimated || after.estimated;
        if (after.bytes >= before_bytes) {
            live_bytes += after.bytes - before_bytes;
        } else {
            live_bytes -= (std::min)(live_bytes, before_bytes - after.bytes);
        }
    }

    template <typename ImageType>
    void Unregister(const ImageType& image, u64 fallback_base_bytes,
                    u64 fallback_scaled_bytes) noexcept {
        const auto size =
            QueryImageAttributionSize(image, fallback_base_bytes, fallback_scaled_bytes);
        if (live_images != 0) {
            --live_images;
        }
        live_bytes -= (std::min)(live_bytes, size.bytes);
    }

    void SentenceGpu(AttributionImageSize size) noexcept {
        gpu_sentenced_bytes += size.bytes;
        estimated = estimated || size.estimated;
    }

    void RetireGpu(u64 bytes) noexcept {
        gpu_sentenced_bytes -= (std::min)(gpu_sentenced_bytes, bytes);
    }

    void SentenceRing(size_t slot_index, AttributionImageSize size) noexcept {
        ring_sentenced_bytes[slot_index] += size.bytes;
        ring_sentenced_total += size.bytes;
        estimated = estimated || size.estimated;
    }

    void AdvanceRingSlot(size_t slot_index) noexcept {
        const u64 expired = ring_sentenced_bytes[slot_index];
        ring_sentenced_bytes[slot_index] = 0;
        ring_sentenced_total -= (std::min)(ring_sentenced_total, expired);
    }

    void MarkRetainedDownload(AttributionImageSize size) noexcept {
        estimated = estimated || size.estimated;
    }

    [[nodiscard]] std::atomic<u64>* RetainedDownloadCounter() noexcept {
        return &retained_download_bytes;
    }

    [[nodiscard]] u64 LiveImages() const noexcept {
        return live_images;
    }

    [[nodiscard]] u64 LiveImageBytes() const noexcept {
        return live_bytes;
    }

    [[nodiscard]] u64 RetiredImageBytes() const noexcept {
        return gpu_sentenced_bytes + ring_sentenced_total +
               retained_download_bytes.load(std::memory_order_relaxed);
    }

    [[nodiscard]] bool IsEstimated() const noexcept {
        return estimated;
    }

private:
    u64 live_images{};
    u64 live_bytes{};
    u64 gpu_sentenced_bytes{};
    std::array<u64, TicksToDestroy> ring_sentenced_bytes{};
    u64 ring_sentenced_total{};
    std::atomic<u64> retained_download_bytes{};
    bool estimated{};
};

} // namespace VideoCommon
