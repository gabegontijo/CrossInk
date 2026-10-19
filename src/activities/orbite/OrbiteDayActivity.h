#pragma once

// Orbite add-on: one part of what the Orbite app wrote to /Orbite/hoje.json — Today, the Lists or the Workouts — as a
// list with a checkbox on each item that can be marked, checked when the Orbite says it is done. The Lists and the
// Workouts first show the groups (the lists, the sheets), each with how many of its items are checked ("3/7"), and
// Confirm opens one (a single group opens straight away). Up and Down move item by item; Confirm (or the side button,
// outside the reader) marks and unmarks, kept in /Orbite/feitos.json until the Orbite syncs. Back goes from a group to
// the groups, and from there Home.

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
  enum class Mode : uint8_t { Today, Lists, Workouts };

  OrbiteDayActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, Mode mode);

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  // One line of the screen: the group (-1: the freshness line) and the row in it (-1: the group's heading; kGroupLine:
  // the group itself, on the list of groups, which Confirm opens).
  struct Line {
    int group = -1;
    int row = -1;
  };
  static constexpr int kGroupLine = -2;

  Mode mode;
  orbite::Day day;
  bool loaded = false;
  orbite::Marks marks;
  std::string markDate;  // the day a mark counts for (the RTC's, or the file's)
  int shownGroup = -1;   // Lists and Workouts: the group whose items are shown, or -1 for the list of groups
  std::vector<Line> lines;
  std::vector<std::string> headings;    // the freshness line, then one per group
  std::vector<std::string> groupTexts;  // on the list of groups: each one's subtitle, then its progress ("3/7")
  int selected = -1;
  ButtonNavigator buttonNavigator;
  using UiApp = freeink::ui::FreeInkApp<20, 4>;
  freeink::ui::GfxRendererTarget uiTarget;
  UiApp app;
  std::atomic<bool> uiReady{false};
  int visibleRows = 1;
  int topIndex = 0;

  static void listScreen(UiApp::ScreenType& screen, void* user);
  void buildListScreen(UiApp::ScreenType& screen);
  void buildLines();
  void buildGroupLines();
  const orbite::Row* rowOf(const Line& line) const;
  // The Lists and the Workouts show the groups first; Today is one list with headings.
  bool groupsFirst() const { return mode == Mode::Lists || mode == Mode::Workouts; }
  bool onGroupList() const { return groupsFirst() && shownGroup < 0; }
  // A group is open that Back closes (with a single group, Back goes Home).
  bool canCloseGroup() const { return groupsFirst() && shownGroup >= 0 && day.groups.size() > 1; }
  void openGroup(int group);
  void closeGroup();
  void selectFirstToDo();
  void select(int index);
  void move(int step);
  void activateSelected();
  const char* title() const;
};
