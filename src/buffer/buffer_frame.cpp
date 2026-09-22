#include "buffer/buffer_frame.hpp"

namespace hamdb
{

    BufferFrame::BufferFrame() = default;

    FrameId BufferFrame::frameId() const
    {
        return frame_id_;
    }

    void BufferFrame::setFrameId(FrameId frame_id)
    {
        frame_id_ = frame_id;
    }

    PageId BufferFrame::pageId() const
    {
        return page_id_;
    }

    void BufferFrame::setPageId(PageId page_id)
    {
        page_id_ = page_id;
    }

    Page& BufferFrame::page()
    {
        return page_;
    }

    const Page& BufferFrame::page() const
    {
        return page_;
    }

    int BufferFrame::pinCount() const
    {
        return pin_count_;
    }

    bool BufferFrame::isDirty() const
    {
        return is_dirty_;
    }

    bool BufferFrame::isValid() const
    {
        return is_valid_;
    }

    void BufferFrame::pin()
    {
        pin_count_++;
    }

    void BufferFrame::unpin(bool dirty)
    {
        if (pin_count_ > 0)
        {
            pin_count_--;
        }
        if (dirty)
        {
            is_dirty_ = true;
        }
    }

    void BufferFrame::markClean()
    {
        is_dirty_ = false;
    }

    void BufferFrame::invalidate()
    {
        is_valid_ = false;
        pin_count_ = 0;
        is_dirty_ = false;
        page_id_ = kInvalidPageId;
        page_.clear();
    }

    void BufferFrame::reset(PageId new_page_id)
    {
        page_id_ = new_page_id;
        pin_count_ = 1;
        is_dirty_ = false;
        is_valid_ = true;
        page_.clear();
    }

} // namespace hamdb
