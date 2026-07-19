#pragma once

#include <cstddef>
#include <memory>
#include <new>
#include <type_traits>
#include <utility>

#include "network/WebDAVTypes.h"

/**
 * Reusable WebDAV directory storage for the constrained target heap.
 *
 * std::vector growth uses throwing operator new, and an uncaught allocation
 * failure resets the firmware. This buffer allocates with nothrow before
 * HTTP/TLS starts and falls back to a smaller listing when the heap is
 * fragmented instead of resetting the device.
 */
class WebDAVItemList final {
 public:
  static constexpr size_t MAX_ITEMS = 200;
  using StoragePtr = std::unique_ptr<WebDAVItem[]>;
  using StorageFactory = StoragePtr (*)(size_t count) noexcept;

  explicit WebDAVItemList(StorageFactory storageFactory = &allocateStorage) noexcept
      : storageFactory(storageFactory) {}

  WebDAVItemList(const WebDAVItemList&) = delete;
  WebDAVItemList& operator=(const WebDAVItemList&) = delete;
  WebDAVItemList(WebDAVItemList&&) = delete;
  WebDAVItemList& operator=(WebDAVItemList&&) = delete;

  bool ensureStorage() noexcept {
    if (storage) return true;
    if (!storageFactory) return false;

    // Prefer the full listing, but remain usable when only a smaller
    // contiguous heap block is available.
    static constexpr size_t capacities[] = {MAX_ITEMS, 128, 64, 32, 16};
    for (const size_t candidateCapacity : capacities) {
      auto candidate = storageFactory(candidateCapacity);
      if (candidate) {
        storage = std::move(candidate);
        storageCapacity = candidateCapacity;
        return true;
      }
    }
    return false;
  }

  void clear() noexcept {
    // Release href/name allocations while retaining the reusable item array.
    for (size_t i = 0; i < itemCount; ++i) {
      storage[i] = WebDAVItem{};
    }
    itemCount = 0;
  }

  bool push_back(WebDAVItem&& item) noexcept {
    if ((!storage && !ensureStorage()) || itemCount >= storageCapacity) return false;
    storage[itemCount++] = std::move(item);
    return true;
  }

  void shrinkTo(const size_t newSize) noexcept {
    if (newSize >= itemCount) return;
    for (size_t i = newSize; i < itemCount; ++i) {
      storage[i] = WebDAVItem{};
    }
    itemCount = newSize;
  }

  [[nodiscard]] size_t size() const noexcept { return itemCount; }
  [[nodiscard]] size_t capacity() const noexcept { return storageCapacity; }
  [[nodiscard]] bool empty() const noexcept { return itemCount == 0; }
  [[nodiscard]] bool full() const noexcept { return itemCount >= storageCapacity; }

  WebDAVItem* data() noexcept { return storage.get(); }
  const WebDAVItem* data() const noexcept { return storage.get(); }

  WebDAVItem& operator[](const size_t index) noexcept { return storage[index]; }
  const WebDAVItem& operator[](const size_t index) const noexcept { return storage[index]; }

 private:
  static StoragePtr allocateStorage(const size_t count) noexcept {
    return StoragePtr{new (std::nothrow) WebDAVItem[count]};
  }

  static_assert(std::is_nothrow_move_assignable_v<WebDAVItem>);

  StoragePtr storage;
  size_t itemCount = 0;
  size_t storageCapacity = 0;
  StorageFactory storageFactory;
};
