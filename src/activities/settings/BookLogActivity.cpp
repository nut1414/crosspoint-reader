#include "BookLogActivity.h"

#include <GfxRenderer.h>
#include <HalStorage.h>
#include <I18n.h>
#include <Logging.h>

#include <memory>
#include <utility>

#include "MappedInputManager.h"
#include "activities/util/ConfirmationActivity.h"
#include "components/UITheme.h"
#include "components/UiAppHelpers.h"
#include "fontIds.h"

namespace fui = freeink::ui;

namespace {
constexpr unsigned long LONG_PRESS_MS = 1000;
}

void BookLogActivity::loadBookLog() {
  books = BOOK_LOG.getBooks();
  unavailable.clear();
  unavailable.reserve(books.size());
  for (const auto& book : books) {
    unavailable.push_back(BookLogStore::isMissing(book) ? 1 : 0);
  }
  rebuildRowItems();
}

void BookLogActivity::rebuildRowItems() {
  rowSubtitles.clear();
  rowItems.clear();
  rowSubtitles.reserve(books.size());
  rowItems.reserve(books.size());

  for (size_t i = 0; i < books.size(); ++i) {
    const auto& book = books[i];
    if (unavailable[i]) {
      std::string description = tr(STR_BOOK_UNAVAILABLE);
      if (!book.author.empty()) description += ": " + book.author;
      rowSubtitles.push_back(std::move(description));
    } else {
      rowSubtitles.push_back(book.author);
    }
  }

  for (size_t i = 0; i < books.size(); ++i) {
    fui::ListItem item;
    item.label = books[i].title.c_str();
    item.subtitle = rowSubtitles[i].empty() ? nullptr : rowSubtitles[i].c_str();
    item.icon = listIconFor(UITheme::getFileIcon(books[i].path));
    item.actionValue = static_cast<int16_t>(i);
    rowItems.push_back(item);
  }
}

void BookLogActivity::onEnter() {
  RenderLock lock(*this);
  UiListActivity::onEnter();
  loadBookLog();
  nav.reset();
  showingUnavailableMessage = false;
  unavailableTitle.clear();
  requestUpdate();
}

const char* BookLogActivity::headerTitle() const { return tr(STR_BOOK_LOG); }

void BookLogActivity::drawFooter() {
  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_OPEN), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
}

void BookLogActivity::activateIndex(const int index) {
  if (index < 0 || index >= listCount()) return;
  nav.selected = index;
  app.clearTapFlash();

  const auto& book = books[static_cast<size_t>(index)];
  if (!Storage.exists(book.path.c_str())) {
    RenderLock lock(*this);
    unavailable[static_cast<size_t>(index)] = 1;
    unavailableTitle = book.title;
    rebuildRowItems();
    showingUnavailableMessage = true;
    closeRouting();
    requestUpdate();
    return;
  }

  LOG_DBG("BLOG", "Opening logged book: %s", book.path.c_str());
  onSelectBook(book.path);
}

void BookLogActivity::promptRemoveBook(const std::string& path, const std::string& title) {
  auto handler = [this, path](const ActivityResult& result) {
    if (result.isCancelled) return;

    RenderLock lock(*this);
    closeRouting();
    if (BOOK_LOG.removeByPath(path)) {
      loadBookLog();
      if (books.empty()) {
        nav.reset();
      } else if (nav.selected >= listCount()) {
        nav.selected = listCount() - 1;
      }
      nav.follow(listCount());
      requestUpdate(true);
    }
  };

  startActivityForResult(
      std::make_unique<ConfirmationActivity>(renderer, mappedInput, tr(STR_REMOVE_FROM_BOOK_LOG), title),
      std::move(handler));
}

bool BookLogActivity::handleCustomInput() {
  if (!showingUnavailableMessage) return false;

  if (mappedInput.wasReleased(MappedInputManager::Button::Back) ||
      mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    showingUnavailableMessage = false;
    requestUpdate();
    return true;
  }

  int x = 0;
  int y = 0;
  if (mappedInput.wasScreenTapped(x, y)) {
    showingUnavailableMessage = false;
    requestUpdate();
  }
  return true;
}

bool BookLogActivity::handleButtons() {
  if (nav.selected >= 0 && nav.selected < listCount() &&
      mappedInput.wasLongPressed(MappedInputManager::Button::Confirm, LONG_PRESS_MS)) {
    app.clearTapFlash();
    const auto& book = books[static_cast<size_t>(nav.selected)];
    promptRemoveBook(book.path, book.title);
    return true;
  }
  return UiListActivity::handleButtons();
}

void BookLogActivity::onRowLongPress(const int index) {
  if (index < 0 || index >= listCount()) return;
  nav.selected = index;
  app.clearTapFlash();
  const auto& book = books[static_cast<size_t>(index)];
  promptRemoveBook(book.path, book.title);
}

void BookLogActivity::buildScreen(UiScreen& screen) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const Rect safe = UITheme::getInstance().getScreenSafeArea(renderer, true, false);
  screen.setContentMarginFromScreen(
      fui::Insets{static_cast<int16_t>(safe.y + metrics.topPadding + metrics.headerHeight),
                  static_cast<int16_t>(renderer.getScreenWidth() - (safe.x + safe.width)),
                  static_cast<int16_t>(renderer.getScreenHeight() - (safe.y + safe.height) + metrics.buttonHintsHeight),
                  static_cast<int16_t>(safe.x)});
  screen.spacer(static_cast<int16_t>(metrics.verticalSpacing));

  if (books.empty()) {
    screen.centeredText(tr(STR_NO_BOOK_LOG_ENTRIES), screen.theme().bodyText);
    return;
  }

  fui::ListProps props;
  props.items = rowItems.data();
  props.count = static_cast<uint16_t>(rowItems.size());
  props.action = ACTION_ROW;
  props.inputMask = fui::InputTouch | fui::InputLongPress;
  props.subtitleText = screen.theme().smallText;
  props.subtitleText.maxLines = 2;
  syncListViewport(screen, props);
  screen.list(props);
}

void BookLogActivity::render(RenderLock&& lock) {
  if (!showingUnavailableMessage) {
    UiListActivity::render(std::move(lock));
    return;
  }

  renderer.clearScreen();

  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();
  const auto& metrics = UITheme::getInstance().getMetrics();

  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_BOOK_LOG));

  renderer.drawCenteredText(UI_12_FONT_ID, pageHeight / 2 - 20, tr(STR_BOOK_UNAVAILABLE), true, EpdFontFamily::BOLD);
  const std::string safeTitle =
      renderer.truncatedText(UI_10_FONT_ID, unavailableTitle.c_str(), pageWidth - metrics.contentSidePadding * 2);
  renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2 + 15, safeTitle.c_str());

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), "", "", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}
