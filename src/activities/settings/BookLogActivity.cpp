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
#include "fontIds.h"

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
}

void BookLogActivity::onEnter() {
  Activity::onEnter();
  loadBookLog();
  selectorIndex = 0;
  longPressFired = false;
  showingUnavailableMessage = false;
  unavailableTitle.clear();
  requestUpdate();
}

void BookLogActivity::onExit() {
  Activity::onExit();
  books.clear();
  unavailable.clear();
}

void BookLogActivity::loop() {
  if (showingUnavailableMessage) {
    if (mappedInput.wasReleased(MappedInputManager::Button::Back) ||
        mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
      showingUnavailableMessage = false;
      requestUpdate();
    }
    return;
  }

  const int pageItems = UITheme::getInstance().getNumberOfItemsPerPage(renderer, true, false, true, true);

  // A fired long press owns the whole gesture; wait for release so Confirm
  // cannot also open the selected book.
  if (longPressFired) {
    if (!mappedInput.isPressed(MappedInputManager::Button::Confirm)) {
      longPressFired = false;
    }
    return;
  }

  if (!books.empty() && selectorIndex < books.size() &&
      mappedInput.isPressed(MappedInputManager::Button::Confirm) && mappedInput.getHeldTime() >= LONG_PRESS_MS) {
    longPressFired = true;
    promptRemoveBook(books[selectorIndex].path, books[selectorIndex].title);
    return;
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    if (!books.empty() && selectorIndex < books.size()) {
      const auto& book = books[selectorIndex];
      if (!Storage.exists(book.path.c_str())) {
        unavailable[selectorIndex] = 1;
        unavailableTitle = book.title;
        showingUnavailableMessage = true;
        requestUpdate();
      } else {
        LOG_DBG("BLOG", "Opening logged book: %s", book.path.c_str());
        onSelectBook(book.path);
      }
    }
    return;
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    finish();
    return;
  }

  const int listSize = static_cast<int>(books.size());
  if (listSize == 0) return;

  buttonNavigator.onNextRelease([this, listSize] {
    selectorIndex = ButtonNavigator::nextIndex(static_cast<int>(selectorIndex), listSize);
    requestUpdate();
  });

  buttonNavigator.onPreviousRelease([this, listSize] {
    selectorIndex = ButtonNavigator::previousIndex(static_cast<int>(selectorIndex), listSize);
    requestUpdate();
  });

  buttonNavigator.onNextContinuous([this, listSize, pageItems] {
    selectorIndex = ButtonNavigator::nextPageIndex(static_cast<int>(selectorIndex), listSize, pageItems);
    requestUpdate();
  });

  buttonNavigator.onPreviousContinuous([this, listSize, pageItems] {
    selectorIndex = ButtonNavigator::previousPageIndex(static_cast<int>(selectorIndex), listSize, pageItems);
    requestUpdate();
  });
}

void BookLogActivity::promptRemoveBook(const std::string& path, const std::string& title) {
  auto handler = [this, path](const ActivityResult& result) {
    if (result.isCancelled) return;

    if (BOOK_LOG.removeByPath(path)) {
      loadBookLog();
      if (books.empty()) {
        selectorIndex = 0;
      } else if (selectorIndex >= books.size()) {
        selectorIndex = books.size() - 1;
      }
      requestUpdate(true);
    }
  };

  startActivityForResult(
      std::make_unique<ConfirmationActivity>(renderer, mappedInput, tr(STR_REMOVE_FROM_BOOK_LOG), title),
      std::move(handler));
}

void BookLogActivity::render(RenderLock&&) {
  renderer.clearScreen();

  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();
  const auto& metrics = UITheme::getInstance().getMetrics();

  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_BOOK_LOG));

  const int contentTop = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
  const int contentHeight = pageHeight - contentTop - metrics.buttonHintsHeight - metrics.verticalSpacing;

  if (showingUnavailableMessage) {
    renderer.drawCenteredText(UI_12_FONT_ID, pageHeight / 2 - 20, tr(STR_BOOK_UNAVAILABLE), true,
                              EpdFontFamily::BOLD);
    const std::string safeTitle =
        renderer.truncatedText(UI_10_FONT_ID, unavailableTitle.c_str(), pageWidth - metrics.contentSidePadding * 2);
    renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2 + 15, safeTitle.c_str());

    const auto labels = mappedInput.mapLabels(tr(STR_BACK), "", "", "");
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
    renderer.displayBuffer();
    return;
  }

  if (books.empty()) {
    renderer.drawText(UI_10_FONT_ID, metrics.contentSidePadding, contentTop + 20, tr(STR_NO_BOOK_LOG_ENTRIES));
  } else {
    GUI.drawList(
        renderer, Rect{0, contentTop, pageWidth, contentHeight}, books.size(), selectorIndex,
        [this](int index) { return books[index].title; },
        [this](int index) {
          if (!unavailable[index]) return books[index].author;
          std::string description = tr(STR_BOOK_UNAVAILABLE);
          if (!books[index].author.empty()) description += ": " + books[index].author;
          return description;
        },
        [this](int index) { return UITheme::getFileIcon(books[index].path); });
  }

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_OPEN), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  renderer.displayBuffer();
}
