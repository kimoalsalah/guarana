// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include "common/memory_attribution.h"

#include <fmt/format.h>

namespace Common::MemoryAttribution {

std::string FormatReport(std::string_view reason, const Report& r) {
    const u64 staging_total = r.stream_staging_bytes + r.upload_staging_bytes +
                              r.download_staging_bytes + r.device_staging_bytes;
    const u64 transcode_total = r.transcode_in_flight_bytes + r.transcode_idle_bytes;
    const u64 buffer_total = r.buffer_cache_bytes + r.multi_range_gathered_bytes;

    const std::string guest_committed =
        r.guest_committed_bytes.has_value()
            ? fmt::format("{:.1f} MiB", BytesToMiB(*r.guest_committed_bytes))
            : std::string{"n/a"};
    const std::string_view img_eq = r.image_bytes_estimated ? "~=" : "=";
    const std::string_view img_est_prefix = r.image_bytes_estimated ? "est, " : "";
    const std::string_view img_est_suffix = r.image_bytes_estimated ? " est" : "";

    return fmt::format(
        "MemoryAttribution({}): "
        "proc commit={:.1f} ws={:.1f} priv_ws={:.1f} MiB | "
        "guest committed={} handed_out={:.1f} MiB | "
        "tex_live{}{:.1f} MiB ({}{} imgs, thresholds exp={:.1f} crit={:.1f} min={:.1f} MiB) | "
        "retire img{}{:.1f} MiB{} buf={:.1f} MiB | "
        "pools staging={:.1f} (stream={:.1f} up={:.1f} down={:.1f} dev={:.1f}) "
        "transcode={:.1f} (flight={:.1f} idle={:.1f}) MiB | "
        "buf_cache={:.1f} MiB ({} bufs, gather={:.1f} MiB) | "
        "shaders cache={}/{:.1f} MiB disk_v2={}/{:.1f} MiB reuse={}/{:.1f} MiB "
        "kept_spirv={}/{:.1f} MiB gfx_pipe={}+{}ret gpl={} comp_pipe={}+{}ret "
        "in_flight_ir={:.1f} MiB | "
        "driver_cache={:.1f} MiB (vk_bin_idx={})",
        reason,
        BytesToMiB(r.process_commit_bytes),
        BytesToMiB(r.process_working_set_bytes),
        BytesToMiB(r.process_private_working_set_bytes),
        guest_committed,
        BytesToMiB(r.guest_handed_out_bytes),
        img_eq,
        BytesToMiB(r.live_image_bytes),
        img_est_prefix,
        r.live_images,
        BytesToMiB(r.texture_expected_bytes),
        BytesToMiB(r.texture_critical_bytes),
        BytesToMiB(r.texture_minimum_bytes),
        img_eq,
        BytesToMiB(r.retired_image_bytes),
        img_est_suffix,
        BytesToMiB(r.retired_buffer_bytes),
        BytesToMiB(staging_total),
        BytesToMiB(r.stream_staging_bytes),
        BytesToMiB(r.upload_staging_bytes),
        BytesToMiB(r.download_staging_bytes),
        BytesToMiB(r.device_staging_bytes),
        BytesToMiB(transcode_total),
        BytesToMiB(r.transcode_in_flight_bytes),
        BytesToMiB(r.transcode_idle_bytes),
        BytesToMiB(buffer_total),
        r.live_buffers,
        BytesToMiB(r.multi_range_gathered_bytes),
        r.shader_cache_entries,
        BytesToMiB(r.shader_cache_owned_bytes),
        r.shader_cache_v2_entries,
        BytesToMiB(r.shader_cache_v2_bytes),
        r.shader_reuse_entries,
        BytesToMiB(r.shader_reuse_bytes),
        r.kept_spirv_entries,
        BytesToMiB(r.kept_spirv_bytes),
        r.graphics_pipelines,
        r.retired_graphics_pipelines,
        r.graphics_libraries,
        r.compute_pipelines,
        r.retired_compute_pipelines,
        BytesToMiB(r.in_flight_ir_bytes),
        BytesToMiB(r.driver_cache_blob_bytes),
        r.pipeline_binary_index_entries);
}

} // namespace Common::MemoryAttribution
