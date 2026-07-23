// Naruto: The Broken Bond - game-specific hooks.
// CompareBackEnds is an optional Fox Engine debug shader compiler path. Its
// embedded Xbox 360 compiler returns E_FAIL under ReXGlue and leaves a null
// object that the guest dereferences forever, so preserve the initialized
// object state and skip only that diagnostic path.
#include "frame_stats.h"
#include "generated/default/narutobb_init.h"

#include <rex/cvar.h>
#include <rex/chrono/clock.h>
#include <rex/diagnostics/guest_timing.h>
#include <rex/logging.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <mutex>

REXCVAR_DEFINE_BOOL(
    naruto_timing_trace_on_start, false, "Diagnostics",
    "Start Naruto frame/wait/vblank timing diagnostics on the first guest "
    "frame. F10 still toggles the trace.");

namespace {

std::atomic<uint64_t> g_guest_frame_count{0};
std::atomic<double> g_guest_fps{0.0};
std::atomic<double> g_guest_frame_time_ms{0.0};
std::atomic<bool> g_timing_trace_toggle_requested{false};
std::atomic<bool> g_timing_trace_enabled{false};

// Static disassembly proves that these guest functions contain direct reads
// of 0x820E8B58. The value is widely used as a threshold/default, not as a
// single authoritative world step, which explains why changing it had no
// visible effect on the 30 FPS cap. Count live calls to narrow the search to
// consumers actually active in the open world without modifying the XEX.
constexpr std::array<uint32_t, 16> kTimingConsumerFunctions = {
    0x82196750, 0x82199B00, 0x821BDFD8, 0x8276E338, 0x827A5898, 0x827A6C60,
    0x827A7510, 0x827A76F0, 0x827A7930, 0x827F8AA0, 0x8281CEF0, 0x8281D080,
    0x82821510, 0x82910BF0, 0x82BC43B8, 0x82BFF8D8,
};
std::array<std::atomic<uint64_t>, kTimingConsumerFunctions.size()>
    g_timing_consumer_calls{};

constexpr uint32_t kWorldStepAddress = 0x820E8B58;
constexpr uint32_t kFormerGameLoopProbeFunction = 0x821C1468;
constexpr uint32_t kGpuWaitProbeFunction = 0x8219F990;
constexpr uint32_t kFrameDeltaWriterFunction = 0x821C0620;

struct TimingAggregate {
  uint64_t first_sequence = 0;
  uint64_t last_sequence = 0;
  uint32_t first_thread_id = 0;
  uint32_t other_thread_calls = 0;
  size_t sample_count = 0;
  uint64_t wall_ns = 0;
  uint64_t cpu_ns = 0;
  uint64_t blocked_ns = 0;
  uint64_t longest_wait_ns = 0;
  uint32_t longest_wait_object = 0;
  uint32_t longest_wait_object_type = 0;
  rex::diagnostics::GuestWaitKind longest_wait_kind =
      rex::diagnostics::GuestWaitKind::kObject;
  std::array<uint64_t, rex::diagnostics::kGuestWaitKindCount> wait_ns{};
  std::array<uint64_t, rex::diagnostics::kGuestWaitKindCount> wait_calls{};
  std::array<uint32_t, 4> vblank_distribution{};
  std::array<uint32_t, 4> vd_swap_distribution{};
  uint64_t vblank_pulses = 0;
  uint64_t vd_swap_calls = 0;
  uint64_t timebase_reads = 0;
  std::array<rex::diagnostics::GuestTimeSourceSite,
             rex::diagnostics::kMaxGuestTimeSourceSites>
      time_source_sites{};
  size_t time_source_site_count = 0;
  std::array<double, 256> wall_samples_ms{};

  void Add(const rex::diagnostics::GuestTimingSample &sample) {
    if (!sample.valid) {
      return;
    }
    if (sample_count == 0) {
      first_sequence = sample.sequence;
      first_thread_id = sample.thread_id;
    } else if (sample.thread_id != first_thread_id) {
      ++other_thread_calls;
    }
    last_sequence = sample.sequence;
    if (sample_count < wall_samples_ms.size()) {
      wall_samples_ms[sample_count] =
          static_cast<double>(sample.wall_ns) / 1'000'000.0;
    }
    ++sample_count;
    wall_ns += sample.wall_ns;
    cpu_ns += sample.cpu_ns;
    blocked_ns += sample.blocked_ns;
    for (size_t i = 0; i < wait_ns.size(); ++i) {
      wait_ns[i] += sample.wait_ns[i];
      wait_calls[i] += sample.wait_calls[i];
    }
    if (sample.longest_wait_ns > longest_wait_ns) {
      longest_wait_ns = sample.longest_wait_ns;
      longest_wait_object = sample.longest_wait_object;
      longest_wait_object_type = sample.longest_wait_object_type;
      longest_wait_kind = sample.longest_wait_kind;
    }
    vblank_pulses += sample.vblank_pulses;
    vd_swap_calls += sample.vd_swap_calls;
    ++vblank_distribution[std::min(sample.vblank_pulses, 3u)];
    ++vd_swap_distribution[std::min(sample.vd_swap_calls, 3u)];
    timebase_reads += sample.timebase_reads;
    for (size_t i = 0; i < sample.time_source_site_count; ++i) {
      const auto &incoming = sample.time_source_sites[i];
      size_t target = 0;
      while (target < time_source_site_count &&
             time_source_sites[target].guest_pc != incoming.guest_pc) {
        ++target;
      }
      if (target == time_source_site_count) {
        if (time_source_site_count >= time_source_sites.size()) {
          continue;
        }
        time_source_sites[time_source_site_count++].guest_pc =
            incoming.guest_pc;
      }
      time_source_sites[target].calls += incoming.calls;
    }
  }

  void Reset() { *this = {}; }
};

TimingAggregate g_frame_timing_aggregate;
TimingAggregate g_loop_probe_aggregate;
TimingAggregate g_gpu_wait_probe_aggregate;
std::mutex g_loop_probe_mutex;
std::mutex g_gpu_wait_probe_mutex;

struct DeltaWriterAggregate {
  uint64_t samples = 0;
  uint64_t frame_delta_ticks = 0;
  uint64_t gpu_wait_ticks = 0;
  uint64_t gpu_wait_secondary_ticks = 0;

  void Reset() { *this = {}; }
};

DeltaWriterAggregate g_delta_writer_aggregate;
std::mutex g_delta_writer_mutex;
std::chrono::steady_clock::time_point g_timing_log_start =
    std::chrono::steady_clock::now();

void LogTimingConsumerCounts(uint64_t frame);

const char *WaitKindName(rex::diagnostics::GuestWaitKind kind) {
  switch (kind) {
  case rex::diagnostics::GuestWaitKind::kObject:
    return "object";
  case rex::diagnostics::GuestWaitKind::kMultiple:
    return "multiple";
  case rex::diagnostics::GuestWaitKind::kSignalAndWait:
    return "signal_and_wait";
  case rex::diagnostics::GuestWaitKind::kDelay:
    return "delay";
  default:
    return "unknown";
  }
}

void LogTimingAggregate(const char *marker, uint32_t function,
                        TimingAggregate &aggregate) {
  if (aggregate.sample_count == 0) {
    return;
  }

  const double wall_ms =
      static_cast<double>(aggregate.wall_ns) / 1'000'000.0;
  const double cpu_ms =
      static_cast<double>(aggregate.cpu_ns) / 1'000'000.0;
  const double blocked_ms =
      static_cast<double>(aggregate.blocked_ns) / 1'000'000.0;
  const double active_ms = std::max(0.0, wall_ms - blocked_ms);
  const double unaccounted_ms =
      std::max(0.0, wall_ms - blocked_ms - cpu_ms);
  const double count = static_cast<double>(aggregate.sample_count);
  const double cpu_percent =
      aggregate.wall_ns
          ? 100.0 * static_cast<double>(aggregate.cpu_ns) /
                static_cast<double>(aggregate.wall_ns)
          : 0.0;
  const double blocked_percent =
      aggregate.wall_ns
          ? 100.0 * static_cast<double>(aggregate.blocked_ns) /
                static_cast<double>(aggregate.wall_ns)
          : 0.0;

  const size_t percentile_count =
      std::min(aggregate.sample_count, aggregate.wall_samples_ms.size());
  std::sort(aggregate.wall_samples_ms.begin(),
            aggregate.wall_samples_ms.begin() + percentile_count);
  const double wall_p50 =
      aggregate.wall_samples_ms[(percentile_count - 1) * 50 / 100];
  const double wall_p95 =
      aggregate.wall_samples_ms[(percentile_count - 1) * 95 / 100];

  REXLOG_INFO(
      "{} function={:08X} samples={} sequence={}..{} thread={} "
      "other_thread_calls={} wall_avg_ms={:.3f} cpu_avg_ms={:.3f} "
      "cpu_percent={:.1f} active_avg_ms={:.3f} blocked_avg_ms={:.3f} "
      "unaccounted_avg_ms={:.3f} blocked_percent={:.1f} "
      "wall_p50_ms={:.3f} "
      "wall_p95_ms={:.3f} wait_calls=[{},{},{},{}] "
      "wait_ms=[{:.3f},{:.3f},{:.3f},{:.3f}] "
      "vblank_avg={:.3f} vblank_dist_0_1_2_3plus=[{},{},{},{}] "
      "vdswap_avg={:.3f} vdswap_dist_0_1_2_3plus=[{},{},{},{}] "
      "timebase_reads={}",
      marker, function, aggregate.sample_count, aggregate.first_sequence,
      aggregate.last_sequence, aggregate.first_thread_id,
      aggregate.other_thread_calls, wall_ms / count, cpu_ms / count,
      cpu_percent, active_ms / count, blocked_ms / count,
      unaccounted_ms / count, blocked_percent, wall_p50, wall_p95,
      aggregate.wait_calls[0], aggregate.wait_calls[1],
      aggregate.wait_calls[2], aggregate.wait_calls[3],
      static_cast<double>(aggregate.wait_ns[0]) / 1'000'000.0,
      static_cast<double>(aggregate.wait_ns[1]) / 1'000'000.0,
      static_cast<double>(aggregate.wait_ns[2]) / 1'000'000.0,
      static_cast<double>(aggregate.wait_ns[3]) / 1'000'000.0,
      static_cast<double>(aggregate.vblank_pulses) / count,
      aggregate.vblank_distribution[0], aggregate.vblank_distribution[1],
      aggregate.vblank_distribution[2], aggregate.vblank_distribution[3],
      static_cast<double>(aggregate.vd_swap_calls) / count,
      aggregate.vd_swap_distribution[0], aggregate.vd_swap_distribution[1],
      aggregate.vd_swap_distribution[2], aggregate.vd_swap_distribution[3],
      aggregate.timebase_reads);

  if (aggregate.longest_wait_ns != 0) {
    REXLOG_INFO(
        "NARUTO_WAIT_TOP owner={} function={:08X} kind={} object_type={} "
        "guest_object={:08X} duration_ms={:.3f}",
        marker, function, WaitKindName(aggregate.longest_wait_kind),
        aggregate.longest_wait_object_type, aggregate.longest_wait_object,
        static_cast<double>(aggregate.longest_wait_ns) / 1'000'000.0);
  }
  for (size_t i = 0; i < aggregate.time_source_site_count; ++i) {
    const auto &site = aggregate.time_source_sites[i];
    REXLOG_INFO(
        "NARUTO_TIME_SOURCE owner={} function={:08X} guest_pc={:08X} "
        "calls={}",
        marker, function, site.guest_pc, site.calls);
  }
}

void FlushTimingAggregates(uint64_t frame) {
  LogTimingAggregate("NARUTO_FRAME_TIMING", 0x821B1DD0,
                     g_frame_timing_aggregate);
  g_frame_timing_aggregate.Reset();
  {
    std::lock_guard<std::mutex> lock(g_loop_probe_mutex);
    LogTimingAggregate("NARUTO_LOOP_PROBE", kFormerGameLoopProbeFunction,
                       g_loop_probe_aggregate);
    g_loop_probe_aggregate.Reset();
  }
  {
    std::lock_guard<std::mutex> lock(g_gpu_wait_probe_mutex);
    LogTimingAggregate("NARUTO_GPU_WAIT_PROBE", kGpuWaitProbeFunction,
                       g_gpu_wait_probe_aggregate);
    g_gpu_wait_probe_aggregate.Reset();
  }
  {
    std::lock_guard<std::mutex> lock(g_delta_writer_mutex);
    if (g_delta_writer_aggregate.samples != 0) {
      const double tick_frequency =
          static_cast<double>(rex::chrono::Clock::guest_tick_frequency());
      const double count =
          static_cast<double>(g_delta_writer_aggregate.samples);
      REXLOG_INFO(
          "NARUTO_DELTA_WRITER function={:08X} field_offset={} samples={} "
          "frame_delta_avg_ms={:.3f} gpu_wait_avg_ms={:.3f} "
          "gpu_wait_secondary_avg_ms={:.3f}",
          kFrameDeltaWriterFunction, 21576,
          g_delta_writer_aggregate.samples,
          static_cast<double>(g_delta_writer_aggregate.frame_delta_ticks) *
              1000.0 / tick_frequency / count,
          static_cast<double>(g_delta_writer_aggregate.gpu_wait_ticks) *
              1000.0 / tick_frequency / count,
          static_cast<double>(
              g_delta_writer_aggregate.gpu_wait_secondary_ticks) *
              1000.0 / tick_frequency / count);
    }
    g_delta_writer_aggregate.Reset();
  }
  LogTimingConsumerCounts(frame);
  g_timing_log_start = std::chrono::steady_clock::now();
}

void CountTimingConsumer(size_t index) {
  if (g_timing_trace_enabled.load(std::memory_order_relaxed)) {
    g_timing_consumer_calls[index].fetch_add(1, std::memory_order_relaxed);
  }
}

void ResetTimingConsumerCounts() {
  for (auto &count : g_timing_consumer_calls) {
    count.store(0, std::memory_order_relaxed);
  }
}

void LogTimingConsumerCounts(uint64_t frame) {
  uint32_t active_count = 0;
  for (size_t i = 0; i < kTimingConsumerFunctions.size(); ++i) {
    const uint64_t calls =
        g_timing_consumer_calls[i].exchange(0, std::memory_order_relaxed);
    if (calls == 0) {
      continue;
    }
    ++active_count;
    REXLOG_INFO("NARUTO_TIMING_CONSUMER function={:08X} calls={}",
                kTimingConsumerFunctions[i], calls);
  }
  REXLOG_INFO("NARUTO_TIMING_TRACE sample frame={} active_functions={} "
              "source_address={:08X}",
              frame, active_count, kWorldStepAddress);
}

} // namespace

rex::ui::FrameStats GetNarutoFrameStats() {
  return {
      .frame_time_ms = g_guest_frame_time_ms.load(std::memory_order_relaxed),
      .fps = g_guest_fps.load(std::memory_order_relaxed),
      .frame_count = g_guest_frame_count.load(std::memory_order_relaxed),
  };
}

void RequestNarutoTimingTraceToggle() {
  g_timing_trace_toggle_requested.store(true, std::memory_order_release);
}

// The Fox renderer calls this function once for each complete guest frame.
// Keep the original implementation and only measure its cadence so F3 can
// distinguish real game frames from the monitor's refresh rate.
REX_EXTERN(__imp__sub_821B1DD0);
extern "C" __attribute__((noinline)) REX_FUNC(sub_821B1DD0) {
  using Clock = std::chrono::steady_clock;
  static Clock::time_point sample_start = Clock::now();
  static uint64_t sample_start_frame = 0;
  static bool start_option_consumed = false;

  if (!start_option_consumed) {
    start_option_consumed = true;
    if (REXCVAR_GET(naruto_timing_trace_on_start)) {
      g_timing_trace_toggle_requested.store(true, std::memory_order_release);
      REXLOG_INFO("NARUTO_TIMING_TRACE auto-start requested");
    }
  }

  const uint64_t frame =
      g_guest_frame_count.fetch_add(1, std::memory_order_relaxed) + 1;

  if (g_timing_trace_enabled.load(std::memory_order_acquire)) {
    g_frame_timing_aggregate.Add(
        rex::diagnostics::EndGuestFrameTiming());
  }

  const bool toggle_requested = g_timing_trace_toggle_requested.exchange(
      false, std::memory_order_acq_rel);
  if (toggle_requested) {
    const bool was_enabled =
        g_timing_trace_enabled.load(std::memory_order_relaxed);
    if (was_enabled) {
      FlushTimingAggregates(frame);
      rex::diagnostics::SetGuestTimingTraceEnabled(false);
    } else {
      g_frame_timing_aggregate.Reset();
      {
        std::lock_guard<std::mutex> lock(g_loop_probe_mutex);
        g_loop_probe_aggregate.Reset();
      }
      {
        std::lock_guard<std::mutex> lock(g_gpu_wait_probe_mutex);
        g_gpu_wait_probe_aggregate.Reset();
      }
      {
        std::lock_guard<std::mutex> lock(g_delta_writer_mutex);
        g_delta_writer_aggregate.Reset();
      }
      ResetTimingConsumerCounts();
      rex::diagnostics::SetGuestTimingTraceEnabled(true);
      g_timing_log_start = Clock::now();
    }
    ResetTimingConsumerCounts();
    g_timing_trace_enabled.store(!was_enabled, std::memory_order_release);
    REXLOG_INFO("NARUTO_TIMING_TRACE {} at guest frame {}",
                was_enabled ? "disabled" : "enabled", frame);
  }

  const Clock::time_point now = Clock::now();
  const std::chrono::duration<double> elapsed = now - sample_start;
  // One-second samples avoid the apparent 56-62 FPS oscillation caused by a
  // half-second window containing one frame more or less at its boundaries.
  if (elapsed.count() >= 1.0) {
    const double fps =
        static_cast<double>(frame - sample_start_frame) / elapsed.count();
    g_guest_fps.store(fps, std::memory_order_relaxed);
    g_guest_frame_time_ms.store(fps > 0.0 ? 1000.0 / fps : 0.0,
                                std::memory_order_relaxed);
    sample_start = now;
    sample_start_frame = frame;
  }

  if (g_timing_trace_enabled.load(std::memory_order_acquire)) {
    const std::chrono::duration<double> trace_elapsed =
        Clock::now() - g_timing_log_start;
    if (trace_elapsed.count() >= 1.0) {
      FlushTimingAggregates(frame);
    }
    rex::diagnostics::BeginGuestFrameTiming(frame);
  }

  __imp__sub_821B1DD0(ctx, base);
}

// A previous backtrace suggested sub_821C1468 as a game-loop wait path. Keep
// the probe as negative evidence: live tracing showed only 16 microsecond-scale
// initialization calls on another thread, so this is not the frame loop.
REX_EXTERN(__imp__sub_821C1468);
extern "C" __attribute__((noinline)) REX_FUNC(sub_821C1468) {
  const bool trace_enabled =
      g_timing_trace_enabled.load(std::memory_order_acquire);
  if (trace_enabled) {
    rex::diagnostics::BeginGuestTimingScope(kFormerGameLoopProbeFunction);
  }
  __imp__sub_821C1468(ctx, base);
  if (trace_enabled) {
    const auto sample =
        rex::diagnostics::EndGuestTimingScope(kFormerGameLoopProbeFunction);
    if (sample.valid) {
      std::lock_guard<std::mutex> lock(g_loop_probe_mutex);
      g_loop_probe_aggregate.Add(sample);
    }
  }
}

// Jade's render queue wait is an explicit guest busy loop (sub_821A1858),
// rather than a kernel wait. Measure the whole owner function so CPU-active
// polling is not misclassified as useful frame work.
REX_EXTERN(__imp__sub_8219F990);
extern "C" __attribute__((noinline)) REX_FUNC(sub_8219F990) {
  const bool trace_enabled =
      g_timing_trace_enabled.load(std::memory_order_acquire);
  if (trace_enabled) {
    rex::diagnostics::BeginGuestTimingScope(kGpuWaitProbeFunction);
  }
  __imp__sub_8219F990(ctx, base);
  if (trace_enabled) {
    const auto sample =
        rex::diagnostics::EndGuestTimingScope(kGpuWaitProbeFunction);
    if (sample.valid) {
      std::lock_guard<std::mutex> lock(g_gpu_wait_probe_mutex);
      g_gpu_wait_probe_aggregate.Add(sample);
    }
  }
}

// This active timebase consumer writes the per-frame tick delta at +21576
// and snapshots the two render-queue wait accumulators at +21616/+21620.
// Record the values after the original code writes them.
REX_EXTERN(__imp__sub_821C0620);
extern "C" __attribute__((noinline)) REX_FUNC(sub_821C0620) {
  const uint32_t owner = ctx.r3.u32;
  __imp__sub_821C0620(ctx, base);
  if (owner != 0 &&
      g_timing_trace_enabled.load(std::memory_order_acquire)) {
    std::lock_guard<std::mutex> lock(g_delta_writer_mutex);
    ++g_delta_writer_aggregate.samples;
    g_delta_writer_aggregate.frame_delta_ticks +=
        REX_LOAD_U32(owner + 21576);
    g_delta_writer_aggregate.gpu_wait_ticks += REX_LOAD_U32(owner + 21616);
    g_delta_writer_aggregate.gpu_wait_secondary_ticks +=
        REX_LOAD_U32(owner + 21620);
  }
}

#define DEFINE_TIMING_CONSUMER_HOOK(name, index)                               \
  REX_EXTERN(__imp__##name);                                                   \
  extern "C" __attribute__((noinline)) REX_FUNC(name) {                        \
    CountTimingConsumer(index);                                                \
    __imp__##name(ctx, base);                                                  \
  }

DEFINE_TIMING_CONSUMER_HOOK(sub_82196750, 0)
DEFINE_TIMING_CONSUMER_HOOK(sub_82199B00, 1)
DEFINE_TIMING_CONSUMER_HOOK(sub_821BDFD8, 2)
DEFINE_TIMING_CONSUMER_HOOK(sub_8276E338, 3)
DEFINE_TIMING_CONSUMER_HOOK(sub_827A5898, 4)
DEFINE_TIMING_CONSUMER_HOOK(sub_827A6C60, 5)
DEFINE_TIMING_CONSUMER_HOOK(sub_827A7510, 6)
DEFINE_TIMING_CONSUMER_HOOK(sub_827A76F0, 7)
DEFINE_TIMING_CONSUMER_HOOK(sub_827A7930, 8)
DEFINE_TIMING_CONSUMER_HOOK(sub_827F8AA0, 9)
DEFINE_TIMING_CONSUMER_HOOK(sub_8281CEF0, 10)
DEFINE_TIMING_CONSUMER_HOOK(sub_8281D080, 11)
DEFINE_TIMING_CONSUMER_HOOK(sub_82821510, 12)
DEFINE_TIMING_CONSUMER_HOOK(sub_82910BF0, 13)
DEFINE_TIMING_CONSUMER_HOOK(sub_82BC43B8, 14)
DEFINE_TIMING_CONSUMER_HOOK(sub_82BFF8D8, 15)

#undef DEFINE_TIMING_CONSUMER_HOOK

REX_EXTERN(__imp__sub_8217AB20);
extern "C" __attribute__((noinline)) REX_FUNC(sub_8217AB20) {
  uint32_t self = ctx.r3.u32;
  static bool logged = false;
  if (!logged) {
    logged = true;
    REXLOG_WARN("Skipping Fox CompareBackEnds debug shader compiler");
  }
  for (uint32_t offset : {0u, 4u, 8u, 40u, 44u, 48u}) {
    *reinterpret_cast<volatile uint32_t *>(base + self + offset) = 0;
  }
  ctx.r3.u32 = self;
}
