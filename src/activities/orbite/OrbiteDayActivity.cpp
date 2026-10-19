#include "OrbiteDayActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>
#include <Logging.h>

#include <algorithm>
#include <cstdio>

#include "MappedInputManager.h"
#include "components/CompactHeader.h"
#include "components/TouchHeaderBackButton.h"
#include "components/UITheme.h"
#include "components/UIThemeTokens.h"
#include "components/UiAppHelpers.h"
#include "orbite/OrbiteStore.h"

namespace fui = freeink::ui;

namespace {

// "2026-10-15" -> "15/10"
std::string shortDate(const std::string& date) {
  if (date.size() != 10) return date;
  return date.substr(8, 2) + "/" + date.substr(5, 2);
}

orbite::Part partOf(const OrbiteDayActivity::Mode mode) {
  switch (mode) {
    case OrbiteDayActivity::Mode::Lists:
      return orbite::Part::Lists;
    case OrbiteDayActivity::Mode::Workouts:
      return orbite::Part::Workouts;
    case OrbiteDayActivity::Mode::Today:
    default:
      return orbite::Part::Today;
  }
}

}  // namespace

OrbiteDayActivity::OrbiteDayActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, const Mode mode)
    : Activity("OrbiteDay", renderer, mappedInput),
      mode(mode),
      uiTarget(makeUiTarget(renderer)),
      app(uiTarget, uiTarget.deviceContext()) {}

// Lines only point at the day's rows (no copies of the text: a long list would cost the C3 its heap twice); headings
// are the few strings built here, the freshness line first. A group open shows only its own items.
void OrbiteDayActivity::buildLines() {
  lines.clear();
  headings.clear();
  groupTexts.clear();
  if (!loaded || day.groups.empty()) return;
  headings.reserve(day.groups.size() + 1);
  // The first line says how fresh the list is: the RTC of the X3 knows today's date.
  headings.push_back(orbite::isStale(day) ? std::string(tr(STR_ORBITE_STALE)) + " " + shortDate(day.date)
                                          : std::string(tr(STR_ORBITE_MADE_AT)) + " " + day.madeAt);
  for (const auto& group : day.groups) {
    std::string heading = group.title;
    if (group.today) heading += std::string(" · ") + tr(STR_ORBITE_TODAY_TAG);
    if (!group.detail.empty()) heading += " · " + group.detail;
    headings.push_back(std::move(heading));
  }
  lines.push_back(Line{-1, -1});
  if (onGroupList()) {
    buildGroupLines();
    return;
  }
  for (size_t g = 0; g < day.groups.size(); ++g) {
    if (shownGroup >= 0 && static_cast<int>(g) != shownGroup) continue;
    const auto& group = day.groups[g];
    lines.reserve(lines.size() + 1 + group.rows.size());
    lines.push_back(Line{static_cast<int>(g), -1});
    for (size_t r = 0; r < group.rows.size(); ++r) lines.push_back(Line{static_cast<int>(g), static_cast<int>(r)});
  }
}

// The list of groups (the lists, the sheets): one line each, with what is said of it (today, the gym's hours) and how
// many of its items are checked.
void OrbiteDayActivity::buildGroupLines() {
  lines.reserve(1 + day.groups.size());
  groupTexts.reserve(2 * day.groups.size());
  for (size_t g = 0; g < day.groups.size(); ++g) {
    const auto& group = day.groups[g];
    std::string subtitle = group.today ? tr(STR_ORBITE_TODAY_TAG) : "";
    if (!group.detail.empty()) subtitle += (subtitle.empty() ? "" : " · ") + group.detail;
    const orbite::Progress progress = orbite::progressOf(marks, group);
    char count[16];
    snprintf(count, sizeof(count), "%u/%u", static_cast<unsigned>(progress.checked),
             static_cast<unsigned>(progress.total));
    groupTexts.push_back(std::move(subtitle));
    groupTexts.emplace_back(count);
    lines.push_back(Line{static_cast<int>(g), kGroupLine});
  }
}

const orbite::Row* OrbiteDayActivity::rowOf(const Line& line) const {
  if (line.row < 0 || line.group < 0 || line.group >= static_cast<int>(day.groups.size())) return nullptr;
  const auto& rows = day.groups[line.group].rows;
  return line.row < static_cast<int>(rows.size()) ? &rows[line.row] : nullptr;
}

void OrbiteDayActivity::onEnter() {
  Activity::onEnter();
  loaded = orbite::loadDay(partOf(mode), day);
  orbite::loadMarks(marks);
  markDate = orbite::markDate(day);
  // The Lists and the Workouts open on the list of groups, or straight on the only one.
  shownGroup = groupsFirst() && day.groups.size() == 1 ? 0 : -1;
  buildLines();
  visibleRows = 1;
  selectFirstToDo();
  uiReady = false;
  applySharedUiTheme(app, uiTarget);
  app.setScreen(&OrbiteDayActivity::listScreen, this);
  requestUpdate();
}

void OrbiteDayActivity::onExit() {
  lines.clear();
  lines.shrink_to_fit();
  headings.clear();
  headings.shrink_to_fit();
  groupTexts.clear();
  groupTexts.shrink_to_fit();
  shownGroup = -1;
  marks = orbite::Marks{};
  day = orbite::Day{};
  Activity::onExit();
}

// The first item still to do (a birthday or a bill above it is only to read, a done one is checked), or the first that
// can be marked, or the first line that can be chosen (a group); the list opens at the top, the freshness line in view.
void OrbiteDayActivity::selectFirstToDo() {
  selected = -1;
  topIndex = 0;
  const auto firstMarkable = [this](const bool toDo) {
    return std::find_if(lines.begin(), lines.end(), [this, toDo](const Line& line) {
      const orbite::Row* row = rowOf(line);
      return row != nullptr && !row->id.empty() && !(toDo && orbite::isChecked(marks, *row));
    });
  };
  auto start = firstMarkable(true);
  if (start == lines.end()) start = firstMarkable(false);
  if (start != lines.end()) {
    selected = static_cast<int>(start - lines.begin());
  } else {
    move(1);
  }
  topIndex = 0;
}

void OrbiteDayActivity::openGroup(const int group) {
  shownGroup = group;
  buildLines();
  selectFirstToDo();
  requestUpdate();
}

// Back to the list of groups, on the one that was open (its count brought up to date).
void OrbiteDayActivity::closeGroup() {
  const int was = shownGroup;
  shownGroup = -1;
  buildLines();
  const auto at = std::find_if(lines.begin(), lines.end(),
                               [was](const Line& line) { return line.group == was && line.row == kGroupLine; });
  selected = at == lines.end() ? -1 : static_cast<int>(at - lines.begin());
  topIndex = 0;
  requestUpdate();
}

void OrbiteDayActivity::select(const int index) {
  if (index == selected) return;
  selected = index;
  topIndex = followListSelection(selected, topIndex, visibleRows, static_cast<int>(lines.size()));
  requestUpdate();
}

// The next line that is an item or a group (headings are skipped), wrapping around like the other CrossInk lists.
void OrbiteDayActivity::move(const int step) {
  const int count = static_cast<int>(lines.size());
  if (count == 0) return;
  int index = selected < 0 ? (step > 0 ? -1 : 0) : selected;
  for (int tries = 0; tries < count; ++tries) {
    index = (index + step + count) % count;
    if (lines[index].row >= 0 || lines[index].row == kGroupLine) {
      select(index);
      return;
    }
  }
}

// Confirm: opens the group, or marks and unmarks the item.
void OrbiteDayActivity::activateSelected() {
  if (selected < 0 || selected >= static_cast<int>(lines.size())) return;
  const Line& line = lines[selected];
  if (line.row == kGroupLine) {
    openGroup(line.group);
    return;
  }
  const orbite::Row* row = rowOf(line);
  if (row == nullptr || row->id.empty()) return;
  orbite::toggle(marks, mode == Mode::Workouts ? &day.groups[line.group] : nullptr, *row, markDate);
  if (!orbite::saveMarks(marks)) LOG_ERR("ORB", "Mark not saved: %s", row->id.c_str());
  requestUpdate();
}

void OrbiteDayActivity::loop() {
  if (TouchHeaderBackButton::wasTapped(mappedInput, TouchHeaderBackButton::compactHeaderRect(renderer)) ||
      mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    if (canCloseGroup()) {
      closeGroup();
    } else {
      onGoHome();
    }
    return;
  }
  if (lines.empty()) return;
  // Confirm, or the side button (outside the reader its page-turn press arrives as Confirm), opens a group, or marks
  // and unmarks.
  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    activateSelected();
    return;
  }
  buttonNavigator.onNextRelease([this] { move(1); });
  buttonNavigator.onPreviousRelease([this] { move(-1); });
  buttonNavigator.onNextContinuous([this] {
    for (int i = 0; i < visibleRows; ++i) move(1);
  });
  buttonNavigator.onPreviousContinuous([this] {
    for (int i = 0; i < visibleRows; ++i) move(-1);
  });
}

void OrbiteDayActivity::listScreen(UiApp::ScreenType& screen, void* user) {
  static_cast<OrbiteDayActivity*>(user)->buildListScreen(screen);
}

void OrbiteDayActivity::buildListScreen(UiApp::ScreenType& screen) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  screen.setContentMargin(fui::Insets{static_cast<int16_t>(CompactHeader::contentTop(metrics)), 0,
                                      static_cast<int16_t>(metrics.buttonHintsHeight), 0});
  screen.spacer(static_cast<int16_t>(metrics.verticalSpacing));
  if (lines.empty()) {
    screen.centeredText(loaded ? tr(STR_ORBITE_EMPTY) : tr(STR_ORBITE_NO_FILE), screen.theme().bodyText);
    return;
  }
  // Built per render; the strings stay owned by `day` and `headings`.
  std::vector<fui::ListItem> items;
  items.reserve(lines.size());
  for (const auto& line : lines) {
    fui::ListItem item;
    const orbite::Row* row = rowOf(line);
    if (line.row == kGroupLine) {
      const auto g = static_cast<size_t>(line.group);
      item.label = day.groups[g].title.c_str();
      if (!groupTexts[2 * g].empty()) item.subtitle = groupTexts[2 * g].c_str();
      item.value = groupTexts[2 * g + 1].c_str();
    } else if (row == nullptr) {
      item.label = headings[static_cast<size_t>(line.group + 1)].c_str();
      item.isHeader = true;
    } else {
      item.label = row->label.c_str();
      if (!row->subtitle.empty()) item.subtitle = row->subtitle.c_str();
      item.toggle = !row->id.empty();
      item.toggleChecked = item.toggle && orbite::isChecked(marks, *row);
    }
    items.push_back(item);
  }
  fui::ListProps props;
  props.items = items.data();
  props.count = static_cast<uint16_t>(items.size());
  props.selectedIndex = static_cast<int16_t>(selected);
  props.inputMask = 0;  // physical buttons stay in loop()
  props.valueInset = 8;
  props.toggleCheckbox = true;
  props.toggleWidth = 32;  // a checkbox big enough for e-ink: 32 - 6 = 26 px
  props.toggleHeight = 32;
  props.labelText = screen.theme().bodyText;
  props.labelText.maxLines = 2;
  configureUiListSectionHeaders(props, screen.theme());
  const auto visible = configureUiList(props, screen.theme(), screen.body(), UiListRowType::WithSubtitle);
  visibleRows = visible > 0 ? visible : 1;
  topIndex = followListSelection(selected < 0 ? 0 : selected, topIndex, visibleRows, static_cast<int>(lines.size()));
  props.topIndex = static_cast<uint16_t>(topIndex);
  screen.list(props);
}

const char* OrbiteDayActivity::title() const {
  switch (mode) {
    case Mode::Lists:
      return tr(STR_ORBITE_LISTS);
    case Mode::Workouts:
      return tr(STR_ORBITE_WORKOUTS);
    case Mode::Today:
    default:
      return day.title.empty() ? tr(STR_ORBITE_TODAY) : day.title.c_str();
  }
}

void OrbiteDayActivity::render(RenderLock&&) {
  renderer.clearScreen();
  if (mappedInput.hasTouchHardware()) {
    TouchHeaderBackButton::drawCompact(renderer, title());
  } else {
    CompactHeader::drawTitle(renderer, title());
  }
  uiReady = false;
  app.render();
  uiReady = true;

  const auto labels =
      mappedInput.mapLabels(mappedInput.withBackArrow(canCloseGroup() ? title() : tr(STR_HOME)),
                            onGroupList() ? tr(STR_OPEN) : tr(STR_ORBITE_MARK), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}
