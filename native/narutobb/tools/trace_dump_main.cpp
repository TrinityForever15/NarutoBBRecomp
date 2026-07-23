#include <memory>
#include <string>
#include <vector>

#include <rex/cvar.h>
#include <rex/graphics/d3d12/graphics_system.h>
#include <rex/graphics/trace_dump.h>
#include <rex/logging.h>

namespace {

class D3D12TraceDump final : public rex::graphics::TraceDump {
 protected:
  std::unique_ptr<rex::graphics::GraphicsSystem> CreateGraphicsSystem() override {
    return std::make_unique<rex::graphics::d3d12::D3D12GraphicsSystem>();
  }

  // PIX capture is optional and unrelated to the PNG readback used here.
  void BeginHostCapture() override {}
  void EndHostCapture() override {}
};

}  // namespace

int main(int argc, char** argv) {
  auto remaining = rex::cvar::Init(argc, argv);
  rex::cvar::ApplyEnvironment();
  rex::InitLoggingEarly();
  rex::LogConfig log_config;
  log_config.log_to_console = true;
  log_config.default_level = spdlog::level::debug;
  rex::InitLogging(log_config);

  std::vector<std::string> args;
  args.reserve(remaining.size() + 1);
  args.emplace_back(argc > 0 ? argv[0] : "narutobb_trace_dump");
  args.insert(args.end(), remaining.begin(), remaining.end());

  D3D12TraceDump trace_dump;
  return trace_dump.Main(args);
}
