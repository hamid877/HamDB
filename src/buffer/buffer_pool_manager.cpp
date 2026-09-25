#include "buffer/buffer_pool_manager.hpp"

namespace hamdb
{

    BufferPoolManager::BufferPoolManager(std::size_t pool_size, DiskManager& disk_manager)
        : pool_size_(pool_size), disk_manager_(disk_manager),
          frames_(std::make_unique<BufferFrame[]>(pool_size)),
          replacer_(std::make_unique<LRUKReplacer>(pool_size, 2))
    {
        for (std::size_t i = 0; i < pool_size_; ++i)
        {
            frames_[i].setFrameId(static_cast<FrameId>(i));
        }
    }

    BufferPoolManager::~BufferPoolManager()
    {
        (void)flushAllPages();
    }

    Status BufferPoolManager::findFreeFrame(FrameId& out_frame_id)
    {
        for (std::size_t i = 0; i < pool_size_; ++i)
        {
            if (!frames_[i].isValid())
            {
                out_frame_id = static_cast<FrameId>(i);
                return Status::Ok;
            }
        }

        FrameId victim_fid;
        if (replacer_->evict(victim_fid))
        {
            BufferFrame& victim = frames_[victim_fid];
            if (victim.isDirty())
            {
                if (Status s = flushPage(victim.pageId()); s != Status::Ok)
                {
                    return s;
                }
            }
            page_table_.erase(victim.pageId());
            victim.invalidate();
            replacer_->remove(victim_fid);
            out_frame_id = victim_fid;
            return Status::Ok;
        }

        return Status::BufferPoolFull;
    }

    Status BufferPoolManager::fetchPage(PageId page_id, BufferFrame*& out_frame)
    {
        out_frame = nullptr;

        auto it = page_table_.find(page_id);
        if (it != page_table_.end())
        {
            FrameId frame_id = it->second;
            frames_[frame_id].pin();
            replacer_->recordAccess(frame_id);
            replacer_->setEvictable(frame_id, false);
            out_frame = &frames_[frame_id];
            return Status::Ok;
        }

        FrameId free_frame_id = 0;
        if (Status s = findFreeFrame(free_frame_id); s != Status::Ok)
        {
            return s;
        }

        BufferFrame& frame = frames_[free_frame_id];
        frame.reset(page_id);

        if (Status s = disk_manager_.readPage(page_id, frame.page()); s != Status::Ok)
        {
            frame.invalidate();
            return s;
        }

        page_table_[page_id] = free_frame_id;

        replacer_->recordAccess(free_frame_id);
        replacer_->setEvictable(free_frame_id, false);

        out_frame = &frame;

        return Status::Ok;
    }

    Status BufferPoolManager::newPage(PageId& out_page_id, BufferFrame*& out_frame)
    {
        out_frame = nullptr;
        out_page_id = kInvalidPageId;

        FrameId free_frame_id = 0;
        if (Status s = findFreeFrame(free_frame_id); s != Status::Ok)
        {
            return s;
        }

        PageId new_page_id = kInvalidPageId;
        if (Status s = disk_manager_.allocatePage(new_page_id); s != Status::Ok)
        {
            return s;
        }

        BufferFrame& frame = frames_[free_frame_id];
        frame.reset(new_page_id);
        page_table_[new_page_id] = free_frame_id;

        replacer_->recordAccess(free_frame_id);
        replacer_->setEvictable(free_frame_id, false);

        out_page_id = new_page_id;
        out_frame = &frame;

        return Status::Ok;
    }

    Status BufferPoolManager::unpinPage(PageId page_id, bool is_dirty)
    {
        auto it = page_table_.find(page_id);
        if (it == page_table_.end())
        {
            return Status::NotFound;
        }

        FrameId frame_id = it->second;
        BufferFrame& frame = frames_[frame_id];

        if (frame.pinCount() <= 0)
        {
            return Status::InvalidArg;
        }

        frame.unpin(is_dirty);

        if (frame.pinCount() == 0)
        {
            replacer_->setEvictable(frame_id, true);
        }

        return Status::Ok;
    }

    Status BufferPoolManager::flushPage(PageId page_id)
    {
        auto it = page_table_.find(page_id);
        if (it == page_table_.end())
        {
            return Status::NotFound;
        }

        FrameId frame_id = it->second;
        BufferFrame& frame = frames_[frame_id];

        if (!frame.isValid())
        {
            return Status::InvalidArg;
        }

        if (!frame.isDirty())
        {
            return Status::Ok;
        }

        if (Status s = disk_manager_.writePage(page_id, frame.page()); s != Status::Ok)
        {
            return s;
        }

        frame.markClean();
        return Status::Ok;
    }

    Status BufferPoolManager::flushAllPages()
    {
        for (std::size_t i = 0; i < pool_size_; ++i)
        {
            if (frames_[i].isValid() && frames_[i].isDirty())
            {
                if (Status s = flushPage(frames_[i].pageId()); s != Status::Ok)
                {
                    return s;
                }
            }
        }
        return Status::Ok;
    }

    Status BufferPoolManager::fetchPageRead(PageId page_id, ReadPageGuard& out_guard)
    {
        out_guard.drop();
        BufferFrame* frame = nullptr;
        if (Status s = fetchPage(page_id, frame); s != Status::Ok)
        {
            return s;
        }
        out_guard = ReadPageGuard(BasicPageGuard(this, frame, false));
        return Status::Ok;
    }

    Status BufferPoolManager::fetchPageWrite(PageId page_id, WritePageGuard& out_guard)
    {
        out_guard.drop();
        BufferFrame* frame = nullptr;
        if (Status s = fetchPage(page_id, frame); s != Status::Ok)
        {
            return s;
        }
        out_guard = WritePageGuard(BasicPageGuard(this, frame, false));
        return Status::Ok;
    }

    Status BufferPoolManager::newPageGuard(PageId& out_page_id, WritePageGuard& out_guard)
    {
        out_guard.drop();
        BufferFrame* frame = nullptr;
        PageId page_id = kInvalidPageId;
        if (Status s = newPage(page_id, frame); s != Status::Ok)
        {
            return s;
        }
        out_page_id = page_id;
        out_guard = WritePageGuard(BasicPageGuard(this, frame, false));
        return Status::Ok;
    }

} // namespace hamdb
