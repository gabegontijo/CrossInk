#include "OrbiteDayActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>
#include <Logging.h>

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

orbite::Row headerRow(std::string label) {
  orbite::Row row;
  row.label = std::move(label);
  row.header = true;
  return row;
}

}  // namespace

OrbiteDayActivity::OrbiteDayActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, const Mode mode)
    : Activity("OrbiteDay", renderer, mappedInput),
      mode(mode),
      uiTarget(makeUiTarget(renderer)),
      app(uiTarget, uiTarget.deviceContext()) {}

void OrbiteDayActivity::buildRows() {
  rows.clear();
  if (!loaded) return;
  const auto& source = mode == Mode::Today ? day.today : day.workout;
  if (source.empty()) return;
  rows.reserve(source.size() + 2);
  // The first line says how fresh the list is: the RTC of the X3 knows today's date.
  if (orbite::isStale(day)) {
    rows.push_back(headerRow(std::string(tr(STR_ORBITE_STALE)) + " " + shortDate(day.date)));
  } else if (!day.madeAt.empty()) {
    rows.push_back(headerRow(std::string(tr(STR_ORBITE_MADE_AT)) + " " + day.madeAt));
  }
  if (mode == Mode::Workout && !day.workoutDetail.empty()) rows.push_back(headerRow(day.workoutDetail));
  rows.insert(rows.end(), source.begin(), source.end());
}

void OrbiteDayActivity::onEnter() {
  Activity::onEnter();
  loaded = orbite::loadDay(day);
  buildRows();
  topIndex = 0;
  visibleRows = 1;
  uiReady = false;
  applySharedUiTheme(app, uiTarget);
  app.setScreen(&OrbiteDayActivity::listScreen, this);
  requestUpdate();
}

void OrbiteDayActivity::onExit() {
  rows.clear();
  rows.shrink_to_fit();
  day = orbite::Day{};
  Activity::onExit();
}

void OrbiteDayActivity::scrollBy(const int delta) {
  const int next = scrollListBy(topIndex, delta, visibleRows, static_cast<int>(rows.size()));
  if (next == topIndex) return;
  topIndex = next;
  requestUpdate();
}

void OrbiteDayActivity::loop() {
  if (TouchHeaderBackButton::wasTapped(mappedInput, TouchHeaderBackButton::compactHeaderRect(renderer)) ||
      mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    onGoHome();
    return;
  }
  if (rows.empty()) return;

  const auto swipe = mappedInput.wasSwipe();
  if (swipe == MappedInputManager::SwipeDir::Up || swipe == MappedInputManager::SwipeDir::Down) {
    scrollBy(swipe == MappedInputManager::SwipeDir::Up ? visibleRows : -visibleRows);
    return;
  }
  // Nothing to select: the page buttons move a page at a time.
  buttonNavigator.onNextRelease([this] { scrollBy(visibleRows); });
  buttonNavigator.onPreviousRelease([this] { scrollBy(-visibleRows); });
}

void OrbiteDayActivity::listScreen(UiApp::ScreenType& screen, void* user) {
  static_cast<OrbiteDayActivity*>(user)->buildListScreen(screen);
}

void OrbiteDayActivity::buildListScreen(UiApp::ScreenType& screen) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  screen.setContentMargin(fui::Insets{static_cast<int16_t>(CompactHeader::contentTop(metrics)), 0,
                                      static_cast<int16_t>(metrics.buttonHintsHeight), 0});
  screen.spacer(static_cast<int16_t>(metrics.verticalSpacing));
  if (rows.empty()) {
    screen.centeredText(loaded ? tr(STR_ORBITE_EMPTY) : tr(STR_ORBITE_NO_FILE), screen.theme().bodyText);
    return;
  }
  // Built per render; the strings stay owned by `rows`.
  std::vector<fui::ListItem> items;
  items.reserve(rows.size());
  for (const auto& row : rows) {
    fui::ListItem item;
    item.label = row.label.c_str();
    if (!row.subtitle.empty()) item.subtitle = row.subtitle.c_str();
    if (!row.value.empty()) item.value = row.value.c_str();
    item.isHeader = row.header;
    item.enabled = !row.header;
    items.push_back(item);
  }
  fui::ListProps props;
  props.items = items.data();
  props.count = static_cast<uint16_t>(items.size());
  props.selectedIndex = -1;  // read-only: nothing is selected
  props.inputMask = 0;
  props.valueInset = 8;
  props.labelText = screen.theme().bodyText;
  props.labelText.maxLines = 2;
  configureUiListSectionHeaders(props, screen.theme());
  const auto visible = configureUiList(props, screen.theme(), screen.body(), UiListRowType::WithSubtitle);
  visibleRows = visible > 0 ? visible : 1;
  topIndex = scrollListBy(topIndex, 0, visibleRows, static_cast<int>(rows.size()));
  props.topIndex = static_cast<uint16_t>(topIndex);
  screen.list(props);
}

const char* OrbiteDayActivity::title() const {
  if (mode == Mode::Workout) return day.workoutTitle.empty() ? tr(STR_ORBITE_WORKOUT) : day.workoutTitle.c_str();
  return day.title.empty() ? tr(STR_ORBITE_TODAY) : day.title.c_str();
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
      mappedInput.mapLabels(mappedInput.withBackArrow(tr(STR_HOME)), "", tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}
