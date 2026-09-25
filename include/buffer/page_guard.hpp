#pragma once

/// @file page_guard.hpp
/// @brief Move-only RAII wrappers that automate pin/unpin around buffer pages.

#include "buffer/buffer_frame.hpp"
#include "common/constants.hpp"
#include "storage/page.hpp"

namespace hamdb
{

    class BufferPoolManager; // forward declaration

    // =========================================================================
    // BasicPageGuard
    // =========================================================================

    /**
     * @brief Move-only RAII guard that pins a page for the duration of its
     * lifetime and automatically calls BufferPoolManager::unpinPage on
     * destruction.
     *
     * Callers should prefer ReadPageGuard or WritePageGuard for typed access.
     * BasicPageGuard is the common base used internally by the higher-level
     * guards, but it can also be used directly when neither read nor write
     * semantics are needed at the type level.
     */
    class BasicPageGuard
    {
    public:
        // ── Construction ─────────────────────────────────────────────────────

        /// Construct an invalid (unowned) guard.
        BasicPageGuard() noexcept = default;

        /**
         * @brief Construct a guard that owns the given frame.
         *
         * @param bpm   The buffer pool manager that produced the frame.
         * @param frame The pinned frame to guard.
         * @param dirty If true, the page will be unpinned as dirty.
         */
        BasicPageGuard(BufferPoolManager* bpm, BufferFrame* frame, bool dirty = false) noexcept;

        // Non-copyable
        BasicPageGuard(const BasicPageGuard&) = delete;
        BasicPageGuard& operator=(const BasicPageGuard&) = delete;

        // Movable
        BasicPageGuard(BasicPageGuard&& other) noexcept;
        BasicPageGuard& operator=(BasicPageGuard&& other) noexcept;

        /// Destructor: automatically unpins the page if still valid.
        ~BasicPageGuard();

        // ── Observers ────────────────────────────────────────────────────────

        /// Returns true when this guard owns a valid pinned page.
        [[nodiscard]] bool isValid() const noexcept;

        /// Returns the page id owned by this guard, or kInvalidPageId.
        [[nodiscard]] PageId pageId() const noexcept;

        /// Read-only access to the underlying Page.
        [[nodiscard]] const Page& page() const noexcept;

        /// Mutable access to the underlying Page (internal use).
        [[nodiscard]] Page& pageMut() noexcept;

        // ── Modifiers ────────────────────────────────────────────────────────

        /**
         * @brief Mark the page as dirty so it will be written back on unpin.
         *
         * Safe to call multiple times; the dirty flag is sticky.
         */
        void markDirty() noexcept;

        /**
         * @brief Release ownership early, unpinning the page immediately.
         *
         * After drop() the guard is in an invalid state; subsequent calls to
         * drop() or the destructor are harmless no-ops.
         */
        void drop();

    private:
        BufferPoolManager* bpm_{nullptr}; ///< Non-owning pointer to the BPM.
        BufferFrame* frame_{nullptr};     ///< Non-owning pointer to the frame.
        PageId page_id_{kInvalidPageId};  ///< Cached page ID.
        bool dirty_{false};               ///< Accumulated dirty flag.
    };

    // =========================================================================
    // ReadPageGuard
    // =========================================================================

    /**
     * @brief RAII guard offering read-only access to a pinned page.
     *
     * Wraps BasicPageGuard and exposes only const Page&.
     * The underlying unpin call uses dirty = false.
     */
    class ReadPageGuard
    {
    public:
        // ── Construction ─────────────────────────────────────────────────────

        ReadPageGuard() noexcept = default;

        /// Construct from a pre-built BasicPageGuard (transfers ownership).
        explicit ReadPageGuard(BasicPageGuard guard) noexcept;

        // Non-copyable
        ReadPageGuard(const ReadPageGuard&) = delete;
        ReadPageGuard& operator=(const ReadPageGuard&) = delete;

        // Movable
        ReadPageGuard(ReadPageGuard&& other) noexcept;
        ReadPageGuard& operator=(ReadPageGuard&& other) noexcept;

        ~ReadPageGuard() = default;

        // ── Observers ────────────────────────────────────────────────────────

        [[nodiscard]] bool isValid() const noexcept;

        [[nodiscard]] PageId pageId() const noexcept;

        /// Read-only access to the page data.
        [[nodiscard]] const Page& page() const noexcept;

        // ── Modifiers ────────────────────────────────────────────────────────

        /// Release ownership early.
        void drop();

    private:
        BasicPageGuard guard_;
    };

    // =========================================================================
    // WritePageGuard
    // =========================================================================

    /**
     * @brief RAII guard offering mutable access to a pinned page.
     *
     * Wraps BasicPageGuard and exposes mutable Page&.
     * Calling markDirty() causes the page to be unpinned as dirty.
     */
    class WritePageGuard
    {
    public:
        // ── Construction ─────────────────────────────────────────────────────

        WritePageGuard() noexcept = default;

        /// Construct from a pre-built BasicPageGuard (transfers ownership).
        explicit WritePageGuard(BasicPageGuard guard) noexcept;

        // Non-copyable
        WritePageGuard(const WritePageGuard&) = delete;
        WritePageGuard& operator=(const WritePageGuard&) = delete;

        // Movable
        WritePageGuard(WritePageGuard&& other) noexcept;
        WritePageGuard& operator=(WritePageGuard&& other) noexcept;

        ~WritePageGuard() = default;

        // ── Observers ────────────────────────────────────────────────────────

        [[nodiscard]] bool isValid() const noexcept;

        [[nodiscard]] PageId pageId() const noexcept;

        /// Read-only view.
        [[nodiscard]] const Page& page() const noexcept;

        /// Mutable view.
        [[nodiscard]] Page& pageMut() noexcept;

        // ── Modifiers ────────────────────────────────────────────────────────

        /// Mark the page dirty so it is written back on drop/destruction.
        void markDirty() noexcept;

        /// Release ownership early, propagating dirty flag.
        void drop();

    private:
        BasicPageGuard guard_;
    };

} // namespace hamdb
