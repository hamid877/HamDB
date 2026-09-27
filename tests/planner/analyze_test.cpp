#include <gtest/gtest.h>
#include "shell/shell.hpp"
#include <sstream>
#include <fstream>
#include <filesystem>
#include "storage/database_metadata.hpp"
#include "storage/table_heap.hpp"
#include "storage/slotted_page.hpp"
#include "utils/serializer.hpp"

using namespace hamdb;
using namespace hamdb::shell;

class AnalyzeTest : public ::testing::Test {
protected:
    void SetUp() override {
        db_name_ = "test_analyze.hamdb";
        if (std::filesystem::exists(db_name_)) {
            std::filesystem::remove(db_name_);
        }
        
        {
            DiskManager dm(db_name_);
            (void)dm.createDatabase();
            (void)dm.openDatabase();
            BufferPoolManager bpm(10, dm);
            CatalogManager catalog(&bpm);
            
            std::vector<Column> cols = {
                Column("id", ColumnType::Integer),
                Column("name", ColumnType::Varchar),
                Column("age", ColumnType::Integer)
            };
            Schema schema(cols);
            
            TableInfo* info = nullptr;
            (void)catalog.createTable("users", schema, info);
            
            (void)bpm.flushAllPages();
            
            Page page(PageHeader(info->getHeapRootPage(), PageType::Table));
            SlottedPage sp(page);
            (void)sp.initialize();
            (void)dm.writePage(info->getHeapRootPage(), page);
            
            auto heap_opt = TableHeap::open(dm, info->getHeapRootPage());
            
            auto insert_row = [&](int id, const std::string& name, int age) {
                std::vector<std::byte> buf(128);
                Serializer ser(buf);
                (void)ser.writeInt32(id);
                (void)ser.writeString(name);
                (void)ser.writeInt32(age);
                Tuple t(std::span<const std::byte>(buf.data(), ser.position()));
                RID rid;
                (void)heap_opt->insertTuple(t, rid);
            };
            
            insert_row(1, "Alice", 30);
            insert_row(2, "Bob", 25);
            insert_row(3, "Charlie", 35);
        }
        
        {
            std::fstream stream(db_name_, std::ios::in | std::ios::out | std::ios::binary);
            std::array<std::uint8_t, DatabaseMetadata::kSize> meta_buf{};
            stream.read(reinterpret_cast<char*>(meta_buf.data()), DatabaseMetadata::kSize);
            DatabaseMetadata meta;
            meta.deserialize(meta_buf);
            if (meta.page_count < 3) {
                meta.page_count = 3;
                meta.serialize(meta_buf);
                stream.seekp(0);
                stream.write(reinterpret_cast<const char*>(meta_buf.data()), DatabaseMetadata::kSize);
            }
        }
        shell = std::make_unique<Shell>(db_name_);
    }
    
    void TearDown() override {
        if (std::filesystem::exists(db_name_)) {
            std::filesystem::remove(db_name_);
        }
    }
    
    std::string db_name_;
    std::unique_ptr<Shell> shell;
};

TEST_F(AnalyzeTest, AnalyzeSimpleSelect) {
    std::ostringstream out;
    shell->executeSQL("EXPLAIN ANALYZE SELECT id, name FROM users WHERE age > 25;", out);
    std::string result = out.str();
    
    EXPECT_TRUE(result.find("=== LOGICAL PLAN ===") != std::string::npos);
    EXPECT_TRUE(result.find("=== OPTIMIZED LOGICAL PLAN ===") != std::string::npos);
    EXPECT_TRUE(result.find("=== PHYSICAL PLAN ===") != std::string::npos);
    EXPECT_TRUE(result.find("=== OUTPUT SCHEMA ===") != std::string::npos);
    EXPECT_TRUE(result.find("=== OPTIMIZER RULES APPLIED ===") != std::string::npos);
    
    EXPECT_TRUE(result.find("SEQ_SCAN (table: users)") != std::string::npos);
    EXPECT_TRUE(result.find("FILTER") != std::string::npos);
    EXPECT_TRUE(result.find("PROJECTION") != std::string::npos);
    
    // Check if stats are printed
    EXPECT_TRUE(result.find("rows_in=") != std::string::npos);
    EXPECT_TRUE(result.find("rows_out=") != std::string::npos);
    EXPECT_TRUE(result.find("time=") != std::string::npos);
}
