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
#include <rex/system/xthread.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <mutex>

REXCVAR_DEFINE_BOOL(
    naruto_timing_trace_on_start, false, "Diagnostics",
    "Start Naruto frame/wait/vblank timing diagnostics on the first guest "
    "frame. F10 still toggles the trace.");
REXCVAR_DEFINE_BOOL(
    naruto_60fps_experiment, false, "Diagnostics",
    "Let F8 enable or suspend the validated 120 Hz guest-vblank and 1/60 "
    "simulation-clock combination across menus, pause, transitions, and "
    "gameplay. The experiment starts disarmed.");
REXCVAR_DEFINE_BOOL(
    naruto_menu_60fps_experiment, false, "Diagnostics",
    "Allow the menu-only queue wait hook to return after one guest vblank. "
    "The guard disables itself when a world-only timing consumer runs.");
REXCVAR_DEFINE_BOOL(
    naruto_menu_120hz_vblank_experiment, false, "Diagnostics",
    "Double the guest-visible vblank rate only in menu-candidate contexts. "
    "The host presentation rate and original XEX remain unchanged.");
REXCVAR_DEFINE_BOOL(
    naruto_world_120hz_vblank_experiment, false, "Diagnostics",
    "After open-world timing consumers validate the context, double the "
    "guest-visible vblank rate until F8 suspends it. The experiment starts "
    "disarmed.");

namespace {

std::atomic<uint64_t> g_guest_frame_count{0};
std::atomic<double> g_guest_fps{0.0};
std::atomic<double> g_guest_frame_time_ms{0.0};
std::atomic<bool> g_timing_trace_toggle_requested{false};
std::atomic<bool> g_timing_trace_enabled{false};
std::atomic<bool> g_60fps_experiment_requested{false};
std::atomic<bool> g_60fps_experiment_effective{false};
std::atomic<bool> g_menu_experiment_toggle_requested{false};
std::atomic<bool> g_menu_experiment_requested{false};
std::atomic<bool> g_menu_experiment_effective{false};
std::atomic<bool> g_menu_experiment_context_violation{false};
std::atomic<bool> g_vblank_experiment_requested{false};
std::atomic<bool> g_vblank_experiment_effective{false};
std::atomic<bool> g_vblank_experiment_context_violation{false};
std::atomic<bool> g_world_vblank_experiment_requested{false};
std::atomic<bool> g_world_vblank_experiment_effective{false};
std::atomic<uint32_t> g_simulation_clock{0};
std::atomic<uint32_t> g_original_fixed_step_bits{0};
std::atomic<uint32_t> g_timing_consumer_frame_mask{0};

constexpr uint32_t kSimulationFixedStepOffset = 76;
constexpr uint32_t kSimulationStep60HzBits = 0x3C888889;

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
// Phase-one traces identified these two consumers as world-only in the
// sampled contexts. The pause path kept only 0x82199B00 active, so a world
// transition can be rejected without disarming during front-end transitions.
constexpr uint32_t kWorldTimingConsumerMask = (uint32_t{1} << 3) |
                                               (uint32_t{1} << 10);
std::array<std::atomic<uint64_t>, kTimingConsumerFunctions.size()>
    g_timing_consumer_calls{};

constexpr uint32_t kWorldStepAddress = 0x820E8B58;
constexpr uint32_t kFormerGameLoopProbeFunction = 0x821C1468;
constexpr uint32_t kGpuWaitProbeFunction = 0x8219F990;
constexpr uint32_t kFrameDeltaWriterFunction = 0x821C0620;
constexpr uint32_t kMainFrameProbeFunction = 0x82160E28;
constexpr uint32_t kMenuExperimentStableFrames = 30;

struct MenuQueueWaitScope {
  bool active = false;
  bool forced_release = false;
  uint64_t begin_vblank = 0;
  uint32_t owner = 0;
  uint32_t target = 0;
  uint32_t caller = 0;
  uint32_t read_pointer_at_release = 0;
};

thread_local MenuQueueWaitScope g_menu_queue_wait_scope;

struct MenuQueueExperimentAggregate {
  uint64_t calls = 0;
  uint64_t forced_releases = 0;
  uint64_t observed_vblanks = 0;
  uint64_t pending_distance = 0;
  uint32_t last_caller = 0;

  void Reset() { *this = {}; }
};

MenuQueueExperimentAggregate g_menu_experiment_aggregate;
std::mutex g_menu_experiment_mutex;

struct GuestEventWaitAggregate {
  uint32_t caller = 0;
  uint32_t thread_id = 0;
  uint64_t calls = 0;
  uint64_t requested_ms = 0;
  uint64_t wall_ns = 0;
  uint64_t observed_vblanks = 0;

  void Reset() { *this = {}; }
};

constexpr size_t kMaxGuestEventWaitCallers = 32;
std::array<GuestEventWaitAggregate, kMaxGuestEventWaitCallers>
    g_guest_event_wait_aggregates{};
std::mutex g_guest_event_wait_mutex;

struct SimulationConsumerCallerAggregate {
  uint32_t caller = 0;
  uint32_t object_vtable = 0;
  uint64_t calls = 0;

  void Reset() { *this = {}; }
};

constexpr size_t kMaxSimulationConsumerCallers = 64;
std::array<SimulationConsumerCallerAggregate,
           kMaxSimulationConsumerCallers>
    g_simulation_consumer_callers{};
std::mutex g_simulation_consumer_caller_mutex;

struct SchedulerVirtualCallAggregate {
  uint32_t caller = 0;
  uint32_t object_vtable = 0;
  uint32_t target = 0;
  uint32_t object_type = 0;
  uint64_t calls = 0;

  void Reset() { *this = {}; }
};

constexpr size_t kMaxSchedulerVirtualCalls = 64;
std::array<SchedulerVirtualCallAggregate, kMaxSchedulerVirtualCalls>
    g_scheduler_virtual_calls{};
std::mutex g_scheduler_virtual_call_mutex;

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
  uint32_t longest_wait_caller = 0;
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
      longest_wait_caller = sample.longest_wait_caller;
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
TimingAggregate g_main_frame_probe_aggregate;
std::mutex g_loop_probe_mutex;
std::mutex g_gpu_wait_probe_mutex;
std::mutex g_main_frame_probe_mutex;

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
      "vblank_rate_multiplier={} vblank_avg={:.3f} "
      "vblank_dist_0_1_2_3plus=[{},{},{},{}] "
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
      rex::diagnostics::GuestVblankRateMultiplier(),
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
        "guest_object={:08X} guest_caller={:08X} duration_ms={:.3f}",
        marker, function, WaitKindName(aggregate.longest_wait_kind),
        aggregate.longest_wait_object_type, aggregate.longest_wait_object,
        aggregate.longest_wait_caller,
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
    std::lock_guard<std::mutex> lock(g_main_frame_probe_mutex);
    LogTimingAggregate("NARUTO_MAIN_FRAME_PROBE", kMainFrameProbeFunction,
                       g_main_frame_probe_aggregate);
    g_main_frame_probe_aggregate.Reset();
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
  {
    std::lock_guard<std::mutex> lock(g_menu_experiment_mutex);
    if (g_menu_experiment_aggregate.calls != 0) {
      const double count =
          static_cast<double>(g_menu_experiment_aggregate.calls);
      const double forced_count = std::max(
          1.0,
          static_cast<double>(g_menu_experiment_aggregate.forced_releases));
      REXLOG_INFO(
          "NARUTO_MENU_QUEUE_EXPERIMENT state={} calls={} "
          "forced_releases={} forced_percent={:.1f} "
          "vblank_avg={:.3f} pending_distance_avg={:.3f} "
          "last_caller={:08X}",
          g_menu_experiment_effective.load(std::memory_order_relaxed)
              ? "active"
              : "guarded",
          g_menu_experiment_aggregate.calls,
          g_menu_experiment_aggregate.forced_releases,
          100.0 * static_cast<double>(
                      g_menu_experiment_aggregate.forced_releases) /
              count,
          static_cast<double>(
              g_menu_experiment_aggregate.observed_vblanks) /
              count,
          static_cast<double>(g_menu_experiment_aggregate.pending_distance) /
              forced_count,
          g_menu_experiment_aggregate.last_caller);
    }
    g_menu_experiment_aggregate.Reset();
  }
  {
    std::lock_guard<std::mutex> lock(g_guest_event_wait_mutex);
    for (auto &aggregate : g_guest_event_wait_aggregates) {
      if (aggregate.calls == 0) {
        continue;
      }
      const double count = static_cast<double>(aggregate.calls);
      REXLOG_INFO(
          "NARUTO_EVENT_WAIT caller={:08X} thread={} calls={} "
          "requested_avg_ms={:.3f} wall_avg_ms={:.3f} vblank_avg={:.3f}",
          aggregate.caller, aggregate.thread_id, aggregate.calls,
          static_cast<double>(aggregate.requested_ms) / count,
          static_cast<double>(aggregate.wall_ns) / 1'000'000.0 / count,
          static_cast<double>(aggregate.observed_vblanks) / count);
      aggregate.Reset();
    }
  }
  {
    std::lock_guard<std::mutex> lock(g_simulation_consumer_caller_mutex);
    for (auto &aggregate : g_simulation_consumer_callers) {
      if (aggregate.calls == 0) {
        continue;
      }
      REXLOG_INFO(
          "NARUTO_SIMULATION_CONSUMER caller={:08X} "
          "object_vtable={:08X} calls={}",
          aggregate.caller, aggregate.object_vtable, aggregate.calls);
      aggregate.Reset();
    }
  }
  {
    std::lock_guard<std::mutex> lock(g_scheduler_virtual_call_mutex);
    for (auto &aggregate : g_scheduler_virtual_calls) {
      if (aggregate.calls == 0) {
        continue;
      }
      REXLOG_INFO(
          "NARUTO_SCHEDULER_VIRTUAL caller={:08X} "
          "object_vtable={:08X} target={:08X} object_type={} calls={}",
          aggregate.caller, aggregate.object_vtable, aggregate.target,
          aggregate.object_type, aggregate.calls);
      aggregate.Reset();
    }
  }
  const auto vblank_worker =
      rex::diagnostics::ConsumeGuestVblankWorkerTiming();
  if (vblank_worker.samples != 0) {
    const double count = static_cast<double>(vblank_worker.samples);
    REXLOG_INFO(
        "NARUTO_VBLANK_WORKER rate_multiplier={} samples={} "
        "total_avg_ms={:.3f} interrupt_avg_ms={:.3f} "
        "total_max_ms={:.3f} interrupt_max_ms={:.3f} "
        "coalesced_intervals={}",
        rex::diagnostics::GuestVblankRateMultiplier(),
        vblank_worker.samples,
        static_cast<double>(vblank_worker.total_ns) / 1'000'000.0 / count,
        static_cast<double>(vblank_worker.interrupt_ns) / 1'000'000.0 /
            count,
        static_cast<double>(vblank_worker.max_total_ns) / 1'000'000.0,
        static_cast<double>(vblank_worker.max_interrupt_ns) / 1'000'000.0,
        vblank_worker.coalesced_intervals);
  }
  LogTimingConsumerCounts(frame);
  g_timing_log_start = std::chrono::steady_clock::now();
}

void CountTimingConsumer(size_t index) {
  const uint32_t bit = uint32_t{1} << index;
  if (REXCVAR_GET(naruto_menu_60fps_experiment) ||
      REXCVAR_GET(naruto_menu_120hz_vblank_experiment) ||
      REXCVAR_GET(naruto_world_120hz_vblank_experiment)) {
    g_timing_consumer_frame_mask.fetch_or(bit, std::memory_order_relaxed);
    if ((bit & kWorldTimingConsumerMask) != 0 &&
        g_menu_experiment_effective.exchange(false,
                                             std::memory_order_acq_rel)) {
      g_menu_experiment_context_violation.store(true,
                                                std::memory_order_release);
      g_menu_experiment_requested.store(false, std::memory_order_release);
    }
    if ((bit & kWorldTimingConsumerMask) != 0 &&
        g_vblank_experiment_effective.exchange(false,
                                               std::memory_order_acq_rel)) {
      rex::diagnostics::SetGuestVblankRateMultiplier(1);
      g_vblank_experiment_context_violation.store(
          true, std::memory_order_release);
      g_vblank_experiment_requested.store(false,
                                          std::memory_order_release);
    }
  }
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

void RequestNarutoMenuExperimentToggle() {
  g_menu_experiment_toggle_requested.store(true, std::memory_order_release);
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
  static uint32_t menu_stable_frames = 0;
  static uint32_t world_stable_frames = 0;

  if (!start_option_consumed) {
    start_option_consumed = true;
    if (REXCVAR_GET(naruto_timing_trace_on_start)) {
      g_timing_trace_toggle_requested.store(true, std::memory_order_release);
      REXLOG_INFO("NARUTO_TIMING_TRACE auto-start requested");
    }
    if (REXCVAR_GET(naruto_60fps_experiment)) {
      if (REXCVAR_GET(naruto_menu_60fps_experiment) ||
          REXCVAR_GET(naruto_menu_120hz_vblank_experiment) ||
          REXCVAR_GET(naruto_world_120hz_vblank_experiment)) {
        REXLOG_ERROR(
            "NARUTO_60FPS_EXPERIMENT rejected because a legacy timing "
            "experiment is also configured");
      } else {
        REXLOG_INFO(
            "NARUTO_60FPS_EXPERIMENT configured; press F8 in a menu or "
            "gameplay to enable it");
      }
    }
    if (REXCVAR_GET(naruto_menu_60fps_experiment)) {
      g_menu_experiment_requested.store(true, std::memory_order_release);
      REXLOG_INFO(
          "NARUTO_MENU_QUEUE_EXPERIMENT configured; waiting for {} stable "
          "menu-candidate frames",
          kMenuExperimentStableFrames);
    }
    if (REXCVAR_GET(naruto_menu_120hz_vblank_experiment)) {
      if (REXCVAR_GET(naruto_menu_60fps_experiment) ||
          REXCVAR_GET(naruto_world_120hz_vblank_experiment)) {
        REXLOG_ERROR(
            "NARUTO_MENU_VBLANK_EXPERIMENT rejected because the phase-2 "
            "queue or world vblank experiment is also configured");
      } else {
        REXLOG_INFO(
            "NARUTO_MENU_VBLANK_EXPERIMENT configured; press F8 in the "
            "front-end menu to arm it");
      }
    }
    if (REXCVAR_GET(naruto_world_120hz_vblank_experiment)) {
      if (REXCVAR_GET(naruto_menu_60fps_experiment) ||
          REXCVAR_GET(naruto_menu_120hz_vblank_experiment)) {
        REXLOG_ERROR(
            "NARUTO_WORLD_VBLANK_EXPERIMENT rejected because a menu "
            "experiment is also configured");
      } else {
        REXLOG_INFO(
            "NARUTO_WORLD_VBLANK_EXPERIMENT configured; press F8 after "
            "open-world gameplay is stable to arm it");
      }
    }
  }

  const uint64_t frame =
      g_guest_frame_count.fetch_add(1, std::memory_order_relaxed) + 1;

  const uint32_t previous_consumer_mask =
      g_timing_consumer_frame_mask.exchange(0, std::memory_order_acq_rel);
  if ((previous_consumer_mask & kWorldTimingConsumerMask) == 0) {
    menu_stable_frames = std::min(menu_stable_frames + 1,
                                  kMenuExperimentStableFrames);
    world_stable_frames = 0;
  } else {
    menu_stable_frames = 0;
    world_stable_frames = std::min(world_stable_frames + 1,
                                   kMenuExperimentStableFrames);
  }

  if (g_menu_experiment_context_violation.exchange(
          false, std::memory_order_acq_rel)) {
    REXLOG_WARN(
        "NARUTO_MENU_QUEUE_EXPERIMENT auto-disabled "
        "consumer_mask={:04X} frame={}",
        previous_consumer_mask, frame);
  }
  if (g_vblank_experiment_context_violation.exchange(
          false, std::memory_order_acq_rel)) {
    REXLOG_WARN(
        "NARUTO_MENU_VBLANK_EXPERIMENT auto-disabled rate_hz=60 "
        "consumer_mask={:04X} frame={}",
        previous_consumer_mask, frame);
  }

  if (g_menu_experiment_toggle_requested.exchange(
          false, std::memory_order_acq_rel)) {
    if (REXCVAR_GET(naruto_60fps_experiment) &&
        !REXCVAR_GET(naruto_world_120hz_vblank_experiment) &&
        !REXCVAR_GET(naruto_menu_120hz_vblank_experiment) &&
        !REXCVAR_GET(naruto_menu_60fps_experiment)) {
      const bool requested =
          !g_60fps_experiment_requested.load(std::memory_order_relaxed);
      g_60fps_experiment_requested.store(requested,
                                         std::memory_order_release);
      g_60fps_experiment_effective.store(requested,
                                         std::memory_order_release);
      rex::diagnostics::SetGuestVblankRateMultiplier(requested ? 2 : 1);
      REXLOG_INFO("NARUTO_60FPS_EXPERIMENT state={} via F8 rate_hz={}",
                  requested ? "active" : "suspended",
                  requested ? 120 : 60);
    } else if (REXCVAR_GET(naruto_world_120hz_vblank_experiment) &&
               !REXCVAR_GET(naruto_60fps_experiment) &&
               !REXCVAR_GET(naruto_menu_120hz_vblank_experiment) &&
               !REXCVAR_GET(naruto_menu_60fps_experiment)) {
      const bool requested =
          !g_world_vblank_experiment_requested.load(
              std::memory_order_relaxed);
      g_world_vblank_experiment_requested.store(
          requested, std::memory_order_release);
      world_stable_frames = requested ? 0 : world_stable_frames;
      g_world_vblank_experiment_effective.store(
          false, std::memory_order_release);
      rex::diagnostics::SetGuestVblankRateMultiplier(1);
      REXLOG_INFO("NARUTO_WORLD_VBLANK_EXPERIMENT {} via F8 rate_hz=60",
                  requested ? "rearmed" : "suspended");
    } else if (REXCVAR_GET(naruto_menu_120hz_vblank_experiment) &&
               !REXCVAR_GET(naruto_60fps_experiment) &&
               !REXCVAR_GET(naruto_world_120hz_vblank_experiment) &&
               !REXCVAR_GET(naruto_menu_60fps_experiment)) {
      const bool requested =
          !g_vblank_experiment_requested.load(std::memory_order_relaxed);
      g_vblank_experiment_requested.store(requested,
                                          std::memory_order_release);
      menu_stable_frames = requested ? 0 : menu_stable_frames;
      g_vblank_experiment_effective.store(false,
                                          std::memory_order_release);
      rex::diagnostics::SetGuestVblankRateMultiplier(1);
      REXLOG_INFO("NARUTO_MENU_VBLANK_EXPERIMENT {} via F8 rate_hz=60",
                  requested ? "rearmed" : "suspended");
    } else if (!REXCVAR_GET(naruto_menu_60fps_experiment)) {
      REXLOG_WARN(
          "NARUTO_MENU_EXPERIMENT F8 ignored because no experiment launch "
          "cvar is enabled");
    } else {
      const bool requested =
          !g_menu_experiment_requested.load(std::memory_order_relaxed);
      g_menu_experiment_requested.store(requested, std::memory_order_release);
      menu_stable_frames = requested ? 0 : menu_stable_frames;
      g_menu_experiment_effective.store(false, std::memory_order_release);
      REXLOG_INFO("NARUTO_MENU_QUEUE_EXPERIMENT {} via F8",
                  requested ? "rearmed" : "suspended");
    }
  }

  const bool should_enable_menu_experiment =
      REXCVAR_GET(naruto_menu_60fps_experiment) &&
      g_menu_experiment_requested.load(std::memory_order_acquire) &&
      menu_stable_frames >= kMenuExperimentStableFrames;
  const bool was_menu_experiment_effective =
      g_menu_experiment_effective.exchange(should_enable_menu_experiment,
                                           std::memory_order_acq_rel);
  if (should_enable_menu_experiment != was_menu_experiment_effective) {
    REXLOG_INFO(
        "NARUTO_MENU_QUEUE_EXPERIMENT state={} frame={} stable_frames={}",
        should_enable_menu_experiment ? "active" : "guarded", frame,
        menu_stable_frames);
  }

  const bool should_enable_vblank_experiment =
      REXCVAR_GET(naruto_menu_120hz_vblank_experiment) &&
      !REXCVAR_GET(naruto_menu_60fps_experiment) &&
      g_vblank_experiment_requested.load(std::memory_order_acquire) &&
      menu_stable_frames >= kMenuExperimentStableFrames;
  const bool was_vblank_experiment_effective =
      g_vblank_experiment_effective.exchange(
          should_enable_vblank_experiment, std::memory_order_acq_rel);
  if (should_enable_vblank_experiment !=
      was_vblank_experiment_effective) {
    rex::diagnostics::SetGuestVblankRateMultiplier(
        should_enable_vblank_experiment ? 2 : 1);
    REXLOG_INFO(
        "NARUTO_MENU_VBLANK_EXPERIMENT state={} frame={} "
        "stable_frames={} rate_hz={}",
        should_enable_vblank_experiment ? "active" : "guarded", frame,
        menu_stable_frames, should_enable_vblank_experiment ? 120 : 60);
  }

  const bool should_enable_world_vblank_experiment =
      REXCVAR_GET(naruto_world_120hz_vblank_experiment) &&
      !REXCVAR_GET(naruto_menu_120hz_vblank_experiment) &&
      !REXCVAR_GET(naruto_menu_60fps_experiment) &&
      g_world_vblank_experiment_requested.load(std::memory_order_acquire) &&
      (world_stable_frames >= kMenuExperimentStableFrames ||
       g_world_vblank_experiment_effective.load(
           std::memory_order_relaxed));
  const bool was_world_vblank_experiment_effective =
      g_world_vblank_experiment_effective.exchange(
          should_enable_world_vblank_experiment,
          std::memory_order_acq_rel);
  if (should_enable_world_vblank_experiment !=
      was_world_vblank_experiment_effective) {
    rex::diagnostics::SetGuestVblankRateMultiplier(
        should_enable_world_vblank_experiment ? 2 : 1);
    REXLOG_INFO(
        "NARUTO_WORLD_VBLANK_EXPERIMENT state={} frame={} "
        "stable_frames={} consumer_mask={:04X} rate_hz={}",
        should_enable_world_vblank_experiment ? "active" : "guarded",
        frame, world_stable_frames, previous_consumer_mask,
        should_enable_world_vblank_experiment ? 120 : 60);
  }

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
        std::lock_guard<std::mutex> lock(g_main_frame_probe_mutex);
        g_main_frame_probe_aggregate.Reset();
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

// This guest helper wraps NtWaitForSingleObjectEx and is the source of the
// longest blocked interval still present after the queue-poll experiment.
// Attribute its parent call sites before changing any timeout or event state.
REX_EXTERN(__imp__sub_821F8438);
extern "C" __attribute__((noinline)) REX_FUNC(sub_821F8438) {
  using Clock = std::chrono::steady_clock;
  const bool trace_enabled =
      g_timing_trace_enabled.load(std::memory_order_acquire);
  const uint32_t caller = static_cast<uint32_t>(ctx.lr);
  const uint32_t requested_ms = ctx.r4.u32;
  const auto *thread = rex::system::XThread::GetCurrentThread();
  const uint32_t thread_id = thread ? thread->thread_id() : 0;
  const uint64_t begin_vblank =
      trace_enabled ? rex::diagnostics::GuestVblankPulseCount() : 0;
  const auto begin = trace_enabled ? Clock::now() : Clock::time_point{};

  __imp__sub_821F8438(ctx, base);

  if (!trace_enabled) {
    return;
  }
  const uint64_t wall_ns = static_cast<uint64_t>(
      std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now() - begin)
          .count());
  const uint64_t observed_vblanks =
      rex::diagnostics::GuestVblankPulseCount() - begin_vblank;
  std::lock_guard<std::mutex> lock(g_guest_event_wait_mutex);
  size_t target = 0;
  while (target < g_guest_event_wait_aggregates.size() &&
         g_guest_event_wait_aggregates[target].calls != 0 &&
         (g_guest_event_wait_aggregates[target].caller != caller ||
          g_guest_event_wait_aggregates[target].thread_id != thread_id)) {
    ++target;
  }
  if (target == g_guest_event_wait_aggregates.size()) {
    return;
  }
  auto &aggregate = g_guest_event_wait_aggregates[target];
  aggregate.caller = caller;
  aggregate.thread_id = thread_id;
  ++aggregate.calls;
  aggregate.requested_ms += requested_ms;
  aggregate.wall_ns += wall_ns;
  aggregate.observed_vblanks += observed_vblanks;
}

// The main thread enters this graphics handoff once per produced frame. Probe
// the full call so the ownership/event waits can be separated from useful
// guest work after the menu queue experiment shortens the render-side poll.
REX_EXTERN(__imp__sub_82160E28);
extern "C" __attribute__((noinline)) REX_FUNC(sub_82160E28) {
  const bool trace_enabled =
      g_timing_trace_enabled.load(std::memory_order_acquire);
  if (trace_enabled) {
    rex::diagnostics::BeginGuestTimingScope(kMainFrameProbeFunction);
  }
  __imp__sub_82160E28(ctx, base);
  if (trace_enabled) {
    const auto sample =
        rex::diagnostics::EndGuestTimingScope(kMainFrameProbeFunction);
    if (sample.valid) {
      std::lock_guard<std::mutex> lock(g_main_frame_probe_mutex);
      g_main_frame_probe_aggregate.Add(sample);
    }
  }
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
  const bool menu_experiment_active =
      g_menu_experiment_effective.load(std::memory_order_acquire);
  const MenuQueueWaitScope previous_menu_scope = g_menu_queue_wait_scope;
  if (menu_experiment_active) {
    g_menu_queue_wait_scope = {
        .active = true,
        .begin_vblank = rex::diagnostics::GuestVblankPulseCount(),
        .owner = ctx.r3.u32,
        .target = ctx.r4.u32,
        .caller = static_cast<uint32_t>(ctx.lr),
    };
  }
  __imp__sub_8219F990(ctx, base);
  if (menu_experiment_active) {
    const uint64_t observed_vblanks =
        rex::diagnostics::GuestVblankPulseCount() -
        g_menu_queue_wait_scope.begin_vblank;
    std::lock_guard<std::mutex> lock(g_menu_experiment_mutex);
    ++g_menu_experiment_aggregate.calls;
    g_menu_experiment_aggregate.observed_vblanks += observed_vblanks;
    g_menu_experiment_aggregate.last_caller =
        g_menu_queue_wait_scope.caller;
    if (g_menu_queue_wait_scope.forced_release) {
      ++g_menu_experiment_aggregate.forced_releases;
      g_menu_experiment_aggregate.pending_distance +=
          g_menu_queue_wait_scope.target -
          g_menu_queue_wait_scope.read_pointer_at_release;
    }
    g_menu_queue_wait_scope = previous_menu_scope;
  }
  if (trace_enabled) {
    const auto sample =
        rex::diagnostics::EndGuestTimingScope(kGpuWaitProbeFunction);
    if (sample.valid) {
      std::lock_guard<std::mutex> lock(g_gpu_wait_probe_mutex);
      g_gpu_wait_probe_aggregate.Add(sample);
    }
  }
}

// sub_8219F990 repeatedly calls this polling helper while waiting for a queue
// ticket. In the opt-in menu experiment, preserve the original poll and only
// replace its "keep waiting" result after one real guest vblank has passed.
REX_EXTERN(__imp__sub_821A1858);
extern "C" __attribute__((noinline)) REX_FUNC(sub_821A1858) {
  __imp__sub_821A1858(ctx, base);
  if (!g_menu_queue_wait_scope.active ||
      !g_menu_experiment_effective.load(std::memory_order_acquire) ||
      ctx.r3.s32 == 0 ||
      rex::diagnostics::GuestVblankPulseCount() <=
          g_menu_queue_wait_scope.begin_vblank) {
    return;
  }

  const uint32_t read_pointer_address =
      REX_LOAD_U32(g_menu_queue_wait_scope.owner + 10896);
  if (read_pointer_address != 0) {
    g_menu_queue_wait_scope.read_pointer_at_release =
        REX_LOAD_U32(read_pointer_address);
  }
  g_menu_queue_wait_scope.forced_release = true;
  ctx.r3.s32 = 0;
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

// The combat scheduler follows its flag-mask update with a second virtual
// stage at vtable +16. It is not a direct reader of the known timing constant,
// so identify its concrete targets during F10 traces before changing timing.
REX_EXTERN(__imp__sub_827E4EC0);
extern "C" __attribute__((noinline)) REX_FUNC(sub_827E4EC0) {
  if (g_timing_trace_enabled.load(std::memory_order_relaxed)) {
    const uint32_t caller = static_cast<uint32_t>(ctx.lr);
    const uint32_t owner = ctx.r3.u32;
    if (owner != 0 && (REX_LOAD_U32(owner) & 0x2000) != 0) {
      const uint32_t list_owner = REX_LOAD_U32(owner + 32);
      uint32_t object =
          list_owner != 0 ? REX_LOAD_U32(list_owner + 32) : 0;
      while (object != 0) {
        const uint32_t object_type = REX_LOAD_U32(object + 4);
        const uint32_t flags = REX_LOAD_U32(object + 8);
        if (object_type != 54 && (flags & 0x02000000) == 0 &&
            (flags & 0x01000000) != 0) {
          const uint32_t object_vtable = REX_LOAD_U32(object);
          const uint32_t target =
              object_vtable != 0 ? REX_LOAD_U32(object_vtable + 16) : 0;
          std::lock_guard<std::mutex> lock(g_scheduler_virtual_call_mutex);
          size_t slot = 0;
          while (slot < g_scheduler_virtual_calls.size() &&
                 g_scheduler_virtual_calls[slot].calls != 0 &&
                 (g_scheduler_virtual_calls[slot].caller != caller ||
                  g_scheduler_virtual_calls[slot].object_vtable !=
                      object_vtable ||
                  g_scheduler_virtual_calls[slot].target != target ||
                  g_scheduler_virtual_calls[slot].object_type !=
                      object_type)) {
            ++slot;
          }
          if (slot < g_scheduler_virtual_calls.size()) {
            auto &aggregate = g_scheduler_virtual_calls[slot];
            aggregate.caller = caller;
            aggregate.object_vtable = object_vtable;
            aggregate.target = target;
            aggregate.object_type = object_type;
            ++aggregate.calls;
          }
        }
        object = REX_LOAD_U32(object + 16);
      }
    }
  }
  __imp__sub_827E4EC0(ctx, base);
}

REX_EXTERN(__imp__sub_8276E338);
extern "C" __attribute__((noinline)) REX_FUNC(sub_8276E338) {
  CountTimingConsumer(3);
  if (g_timing_trace_enabled.load(std::memory_order_relaxed)) {
    const uint32_t caller = static_cast<uint32_t>(ctx.lr);
    const uint32_t object_vtable =
        ctx.r3.u32 != 0 ? REX_LOAD_U32(ctx.r3.u32) : 0;
    std::lock_guard<std::mutex> lock(g_simulation_consumer_caller_mutex);
    size_t target = 0;
    while (target < g_simulation_consumer_callers.size() &&
           g_simulation_consumer_callers[target].calls != 0 &&
           (g_simulation_consumer_callers[target].caller != caller ||
            g_simulation_consumer_callers[target].object_vtable !=
                object_vtable)) {
      ++target;
    }
    if (target < g_simulation_consumer_callers.size()) {
      auto &aggregate = g_simulation_consumer_callers[target];
      aggregate.caller = caller;
      aggregate.object_vtable = object_vtable;
      ++aggregate.calls;
    }
  }
  __imp__sub_8276E338(ctx, base);
}

#undef DEFINE_TIMING_CONSUMER_HOOK

// The central clock updater uses the fixed-step value at +76 to derive the
// simulation deltas at +64/+68 and its internal tick counters. Preserve the
// title's original value, and substitute 1/60 only while either guarded 120 Hz
// vblank experiment is effective. Restoring the captured value keeps F8 and
// every default launch fully reversible without modifying the XEX.
REX_EXTERN(__imp__sub_82BC8FA8);
extern "C" __attribute__((noinline)) REX_FUNC(sub_82BC8FA8) {
  const uint32_t simulation_clock = ctx.r3.u32;
  const bool experiment_active =
      g_60fps_experiment_effective.load(std::memory_order_acquire) ||
      g_world_vblank_experiment_effective.load(std::memory_order_acquire) ||
      g_vblank_experiment_effective.load(std::memory_order_acquire);

  if (experiment_active && simulation_clock != 0) {
    const uint32_t tracked_clock =
        g_simulation_clock.load(std::memory_order_relaxed);
    if (tracked_clock != simulation_clock) {
      if (tracked_clock != 0) {
        const uint32_t previous_original_step =
            g_original_fixed_step_bits.load(
                std::memory_order_relaxed);
        REX_STORE_U32(tracked_clock + kSimulationFixedStepOffset,
                      previous_original_step);
        REXLOG_INFO(
            "NARUTO_SIMULATION_STEP state=clock-restored "
            "clock={:08X} original_bits={:08X}",
            tracked_clock, previous_original_step);
      }
      const uint32_t original_step =
          REX_LOAD_U32(simulation_clock + kSimulationFixedStepOffset);
      g_original_fixed_step_bits.store(original_step,
                                       std::memory_order_relaxed);
      g_simulation_clock.store(simulation_clock, std::memory_order_release);
      REXLOG_INFO(
          "NARUTO_SIMULATION_STEP state=active clock={:08X} "
          "original_bits={:08X} replacement_bits={:08X}",
          simulation_clock, original_step, kSimulationStep60HzBits);
    }
    REX_STORE_U32(simulation_clock + kSimulationFixedStepOffset,
                  kSimulationStep60HzBits);
  } else {
    const uint32_t tracked_clock =
        g_simulation_clock.exchange(0, std::memory_order_acq_rel);
    if (tracked_clock != 0) {
      const uint32_t original_step =
          g_original_fixed_step_bits.exchange(0,
                                              std::memory_order_relaxed);
      REX_STORE_U32(tracked_clock + kSimulationFixedStepOffset,
                    original_step);
      REXLOG_INFO(
          "NARUTO_SIMULATION_STEP state=restored clock={:08X} "
          "original_bits={:08X}",
          tracked_clock, original_step);
    }
  }

  __imp__sub_82BC8FA8(ctx, base);
}

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
