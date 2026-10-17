#pragma once

// Orbite add-on: the day (Today) or the workout of the day (Workout) that the Orbite app wrote to
// /Orbite/hoje.json. Read-only: Back goes Home, the page buttons scroll.

#include <FreeInkApp.h>
#include <FreeInkUIGfxRenderer.h>

#include <atomic>
#include <string>
#include <vector>

#include "activities/Activity.h"
#include "orbite/OrbiteDay.h"
#include "util/ButtonNavigator.h"

class OrbiteDayActivity final : public Activity {
 public:
  enum class Mode : uint8_t { Today, Workout };

  OrbiteDayActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, Mode mode);

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  Mode mode;
  orbite::Day day;
  bool loaded = false;
  // What the screen lists: a first header row (when it was made, or that it is out of date), then the rows.
  std::vector<orbite::Row> rows;
  ButtonNavigator buttonNavigator;
  using UiApp = freeink::ui::FreeInkApp<20, 4>;
  freeink::ui::GfxRendererTarget uiTarget;
  UiApp app;
  std::atomic<bool> uiReady{false};
  int visibleRows = 1;
  int topIndex = 0;

  static void listScreen(UiApp::ScreenType& screen, void* user);
  void buildListScreen(UiApp::ScreenType& screen);
  void buildRows();
  void scrollBy(int delta);
  const char* title() const;
};
