#include "buffer/page_guard.hpp"

#include "buffer/buffer_pool_manager.hpp"

namespace hamdb
{

    // =========================================================================
    // BasicPageGuard
    // =========================================================================

    BasicPageGuard::BasicPageGuard(BufferPoolManager* bpm, BufferFrame* frame,
                                   bool dirty) noexcept
        : bpm_(bpm),
          frame_(frame),
          page_id_(frame != nullptr ? frame->pageId() : kInvalidPageId),
          dirty_(dirty)
    {
    }

    BasicPageGuard::BasicPageGuard(BasicPageGuard&& other) noexcept
        : bpm_(other.bpm_),
          frame_(other.frame_),
          page_id_(other.page_id_),
          dirty_(other.dirty_)
    {
        other.bpm_     = nullptr;
        other.frame_   = nullptr;
        other.page_id_ = kInvalidPageId;
        other.dirty_   = false;
    }

    BasicPageGuard& BasicPageGuard::operator=(BasicPageGuard&& other) noexcept
    {
        if (this != &other)
        {
            // Release our current ownership before taking the new one.
            drop();

            bpm_     = other.bpm_;
            frame_   = other.frame_;
            page_id_ = other.page_id_;
            dirty_   = other.dirty_;

            other.bpm_     = nullptr;
            other.frame_   = nullptr;
            other.page_id_ = kInvalidPageId;
            other.dirty_   = false;
        }
        return *this;
    }

    BasicPageGuard::~BasicPageGuard()
    {
        drop();
    }

    bool BasicPageGuard::isValid() const noexcept
    {
        return bpm_ != nullptr && frame_ != nullptr &&
               page_id_ != kInvalidPageId;
    }

    PageId BasicPageGuard::pageId() const noexcept
    {
        return page_id_;
    }

    const Page& BasicPageGuard::page() const noexcept
    {
        return frame_->page();
    }

    Page& BasicPageGuard::pageMut() noexcept
    {
        return frame_->page();
    }

    void BasicPageGuard::markDirty() noexcept
    {
        dirty_ = true;
    }

    void BasicPageGuard::drop()
    {
        if (!isValid())
        {
            return;
        }

        (void)bpm_->unpinPage(page_id_, dirty_);

        bpm_     = nullptr;
        frame_   = nullptr;
        page_id_ = kInvalidPageId;
        dirty_   = false;
    }

    // =========================================================================
    // ReadPageGuard
    // =========================================================================

    ReadPageGuard::ReadPageGuard(BasicPageGuard guard) noexcept
        : guard_(std::move(guard))
    {
    }

    ReadPageGuard::ReadPageGuard(ReadPageGuard&& other) noexcept
        : guard_(std::move(other.guard_))
    {
    }

    ReadPageGuard& ReadPageGuard::operator=(ReadPageGuard&& other) noexcept
    {
        if (this != &other)
        {
            guard_ = std::move(other.guard_);
        }
        return *this;
    }

    bool ReadPageGuard::isValid() const noexcept
    {
        return guard_.isValid();
    }

    PageId ReadPageGuard::pageId() const noexcept
    {
        return guard_.pageId();
    }

    const Page& ReadPageGuard::page() const noexcept
    {
        return guard_.page();
    }

    void ReadPageGuard::drop()
    {
        guard_.drop();
    }

    // =========================================================================
    // WritePageGuard
    // =========================================================================

    WritePageGuard::WritePageGuard(BasicPageGuard guard) noexcept
        : guard_(std::move(guard))
    {
    }

    WritePageGuard::WritePageGuard(WritePageGuard&& other) noexcept
        : guard_(std::move(other.guard_))
    {
    }

    WritePageGuard& WritePageGuard::operator=(WritePageGuard&& other) noexcept
    {
        if (this != &other)
        {
            guard_ = std::move(other.guard_);
        }
        return *this;
    }

    bool WritePageGuard::isValid() const noexcept
    {
        return guard_.isValid();
    }

    PageId WritePageGuard::pageId() const noexcept
    {
        return guard_.pageId();
    }

    const Page& WritePageGuard::page() const noexcept
    {
        return guard_.page();
    }

    Page& WritePageGuard::pageMut() noexcept
    {
        return guard_.pageMut();
    }

    void WritePageGuard::markDirty() noexcept
    {
        guard_.markDirty();
    }

    void WritePageGuard::drop()
    {
        guard_.drop();
    }

} // namespace hamdb
