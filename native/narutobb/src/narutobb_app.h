// narutobb - ReXGlue Recompiled Project
//
// Customize your app by overriding virtual hooks from rex::ReXApp.

#pragma once

#include "frame_stats.h"

#include <rex/filesystem.h>
#include <rex/graphics/graphics_system.h>
#include <rex/logging.h>
#include <rex/rex_app.h>
#include <rex/ui/keybinds.h>

class NarutobbApp : public rex::ReXApp {
public:
  using rex::ReXApp::ReXApp;

  static std::unique_ptr<rex::ui::WindowedApp>
  Create(rex::ui::WindowedAppContext &ctx) {
    return std::unique_ptr<NarutobbApp>(
        new NarutobbApp(ctx, "narutobb", PPCImageConfig));
  }

  void OnPostSetup() override {
    SetGuestFrameStats(GetNarutoFrameStats);
    rex::ui::RegisterBind(
        "bind_naruto_timing_trace", "F10",
        "Toggle Naruto frame timing diagnostics",
        [] { RequestNarutoTimingTraceToggle(); });
    rex::ui::RegisterBind(
        "bind_naruto_menu_queue_experiment", "F8",
        "Suspend or rearm the selected Naruto menu timing experiment",
        [] { RequestNarutoMenuExperimentToggle(); });
    rex::ui::RegisterBind(
        "bind_naruto_gpu_trace", "F9", "Capture one Naruto GPU frame", [this] {
          auto *app_runtime = runtime();
          if (!app_runtime || !app_runtime->graphics_system()) {
            REXLOG_WARN(
                "NARUTO_GPU_TRACE ignored: graphics system unavailable");
            return;
          }
          auto *graphics_system = static_cast<rex::graphics::GraphicsSystem *>(
              app_runtime->graphics_system());
          graphics_system->RequestFrameTrace();
          REXLOG_INFO("NARUTO_GPU_TRACE requested via F9");
        });
  }

  void OnShutdown() override {
    rex::ui::UnregisterBind("bind_naruto_menu_queue_experiment");
    rex::ui::UnregisterBind("bind_naruto_timing_trace");
    rex::ui::UnregisterBind("bind_naruto_gpu_trace");
  }

  void OnConfigurePaths(rex::PathConfig &paths) override {
    if (!paths.game_data_root.empty()) {
      return;
    }

    // Allow launching narutobb.exe directly (including visible automated
    // tests) while keeping --game_data_root as the preferred override.
    auto project_root = rex::filesystem::GetExecutableFolder();
    for (int i = 0; i < 5; ++i) {
      project_root = project_root.parent_path();
    }
    const auto bundled_assets = project_root / "recomp" / "fase4" / "assets";
    if (std::filesystem::is_regular_file(bundled_assets / "default.xex")) {
      paths.game_data_root = bundled_assets;
    }
  }

  // Override virtual hooks for customization:
  // void OnPostInitLogging() override {}
  // void OnPreSetup(rex::RuntimeConfig& config) override {}
  // void OnLoadXexImage(std::string& xex_image) override {}
  // void OnCreateDialogs(rex::ui::ImGuiDrawer* drawer) override {}
  // void OnShutdown() override {}
};
