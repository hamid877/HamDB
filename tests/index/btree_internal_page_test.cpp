#include "index/btree_internal_page.hpp"
#include <gtest/gtest.h>
#include <array>

namespace hamdb
{

    class BTreeInternalPageTest : public ::testing::Test
    {
    protected:
        std::unique_ptr<BTreeInternalPage> page_ptr_{new BTreeInternalPage()};
        BTreeInternalPage* page_{page_ptr_.get()};

        void SetUp() override
        {
            page_->init(100, 50);
        }
    };

    TEST_F(BTreeInternalPageTest, Initialization)
    {
        EXPECT_EQ(page_->pageId(), 100);
        EXPECT_EQ(page_->parentPageId(), 50);
        EXPECT_EQ(page_->size(), 0);
        EXPECT_TRUE(page_->isEmpty());
        EXPECT_FALSE(page_->isFull());
        EXPECT_EQ(page_->maxSize(), BTreeInternalPage::kMaxEntries);
    }

    TEST_F(BTreeInternalPageTest, PopulateNewRoot)
    {
        page_->populateNewRoot(10, 500, 20);
        EXPECT_EQ(page_->size(), 2);
        EXPECT_EQ(page_->childAt(0), 10);
        EXPECT_EQ(page_->keyAt(1), 500);
        EXPECT_EQ(page_->childAt(1), 20);
    }

    TEST_F(BTreeInternalPageTest, LookupRouting)
    {
        page_->populateNewRoot(10, 500, 20);
        EXPECT_EQ(page_->insert(1000, 30), Status::Ok);
        EXPECT_EQ(page_->insert(2000, 40), Status::Ok);

        // Keys: [500, 1000, 2000]
        // Children: [10, 20, 30, 40]
        EXPECT_EQ(page_->size(), 4);

        EXPECT_EQ(page_->lookup(100), 10);
        EXPECT_EQ(page_->lookup(499), 10);
        EXPECT_EQ(page_->lookup(500), 20);
        EXPECT_EQ(page_->lookup(750), 20);
        EXPECT_EQ(page_->lookup(1000), 30);
        EXPECT_EQ(page_->lookup(1500), 30);
        EXPECT_EQ(page_->lookup(2000), 40);
        EXPECT_EQ(page_->lookup(5000), 40);
    }

    TEST_F(BTreeInternalPageTest, LookupEmpty)
    {
        EXPECT_EQ(page_->lookup(100), kInvalidPageId);
    }

    TEST_F(BTreeInternalPageTest, InsertionOrder)
    {
        page_->populateNewRoot(10, 500, 20);
        // Insert out of order
        EXPECT_EQ(page_->insert(250, 15), Status::Ok);
        EXPECT_EQ(page_->insert(750, 25), Status::Ok);
        EXPECT_EQ(page_->insert(100, 12), Status::Ok);

        // Keys should be: 100, 250, 500, 750
        // Children: 10, 12, 15, 20, 25
        EXPECT_EQ(page_->size(), 5);

        EXPECT_EQ(page_->childAt(0), 10);
        EXPECT_EQ(page_->keyAt(1), 100);
        EXPECT_EQ(page_->childAt(1), 12);
        EXPECT_EQ(page_->keyAt(2), 250);
        EXPECT_EQ(page_->childAt(2), 15);
        EXPECT_EQ(page_->keyAt(3), 500);
        EXPECT_EQ(page_->childAt(3), 20);
        EXPECT_EQ(page_->keyAt(4), 750);
        EXPECT_EQ(page_->childAt(4), 25);
    }

    TEST_F(BTreeInternalPageTest, InsertDuplicateReject)
    {
        page_->populateNewRoot(10, 500, 20);
        EXPECT_EQ(page_->insert(500, 30), Status::AlreadyExists);
        EXPECT_EQ(page_->size(), 2);
    }

    TEST_F(BTreeInternalPageTest, Deletion)
    {
        page_->populateNewRoot(10, 500, 20);
        EXPECT_EQ(page_->insert(1000, 30), Status::Ok);
        EXPECT_EQ(page_->insert(2000, 40), Status::Ok);

        // Keys: [500, 1000, 2000]
        // Remove 1000
        EXPECT_EQ(page_->remove(1000), Status::Ok);
        EXPECT_EQ(page_->size(), 3);

        EXPECT_EQ(page_->keyAt(1), 500);
        EXPECT_EQ(page_->childAt(1), 20);
        EXPECT_EQ(page_->keyAt(2), 2000);
        EXPECT_EQ(page_->childAt(2), 40);
        
        EXPECT_EQ(page_->lookup(1000), 20); // 1000 < 2000, routes to child 1 which is 20
    }

    TEST_F(BTreeInternalPageTest, DeleteNotFound)
    {
        page_->populateNewRoot(10, 500, 20);
        EXPECT_EQ(page_->remove(999), Status::NotFound);
        EXPECT_EQ(page_->size(), 2);
    }
    
    TEST_F(BTreeInternalPageTest, FullPageBehavior)
    {
        page_->populateNewRoot(0, 1, 1);
        
        uint16_t capacity = page_->maxSize();
        
        for (int64_t i = 2; i < capacity; ++i)
        {
            EXPECT_EQ(page_->insert(i, static_cast<PageId>(i)), Status::Ok);
        }
        
        EXPECT_TRUE(page_->isFull());
        EXPECT_EQ(page_->size(), capacity);
        
        EXPECT_EQ(page_->insert(9999, 99), Status::InvalidArg);
    }
    
    TEST_F(BTreeInternalPageTest, SerializationRoundTrip)
    {
        page_->populateNewRoot(10, 500, 20);
        EXPECT_EQ(page_->insert(1000, 30), Status::Ok);
        EXPECT_EQ(page_->insert(2000, 40), Status::Ok);

        std::array<std::byte, kPageSize> dest_buffer{};
        EXPECT_EQ(page_->serialize(dest_buffer), Status::Ok);

        std::array<std::byte, kPageSize> read_buffer{};
        BTreeInternalPage* read_page = reinterpret_cast<BTreeInternalPage*>(read_buffer.data());
        
        EXPECT_EQ(read_page->deserialize(dest_buffer), Status::Ok);

        EXPECT_EQ(read_page->pageId(), 100);
        EXPECT_EQ(read_page->parentPageId(), 50);
        EXPECT_EQ(read_page->size(), 4);
        
        EXPECT_EQ(read_page->childAt(0), 10);
        EXPECT_EQ(read_page->keyAt(1), 500);
        EXPECT_EQ(read_page->childAt(1), 20);
        EXPECT_EQ(read_page->keyAt(2), 1000);
        EXPECT_EQ(read_page->childAt(2), 30);
        EXPECT_EQ(read_page->keyAt(3), 2000);
        EXPECT_EQ(read_page->childAt(3), 40);
    }

    TEST_F(BTreeInternalPageTest, SerializationErrors)
    {
        page_->populateNewRoot(10, 500, 20);
        std::array<std::byte, 10> small_buffer{};
        EXPECT_EQ(page_->serialize(small_buffer), Status::IoError);
        EXPECT_EQ(page_->deserialize(small_buffer), Status::IoError);
    }

} // namespace hamdb
