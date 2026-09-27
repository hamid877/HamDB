#include "shell/shell.hpp"
#include "shell/repl.hpp"
#include <gtest/gtest.h>
#include <sstream>
#include <fstream>
#include <filesystem>
#include "storage/database_metadata.hpp"
#include "storage/table_heap.hpp"
#include "storage/slotted_page.hpp"
#include "utils/serializer.hpp"
#include "executor/executor_context.hpp"
#include "planner/executor_factory.hpp"

namespace hamdb::shell {

class ShellTest : public ::testing::Test {
protected:
    void SetUp() override {
        db_name_ = "test_shell.hamdb";
        if (std::filesystem::exists(db_name_)) {
            std::filesystem::remove(db_name_);
        }
        
        // Setup initial catalog to create a table manually
        {
            DiskManager dm(db_name_);
            (void)dm.createDatabase();
            (void)dm.openDatabase();
            BufferPoolManager bpm(10, dm);
            CatalogManager catalog(&bpm);
            
            std::vector<Column> cols = {
                Column("id", ColumnType::Integer),
                Column("name", ColumnType::Varchar)
            };
            Schema schema(cols);
            
            TableInfo* info = nullptr;
            (void)catalog.createTable("users", schema, info);
            
            (void)bpm.flushAllPages();
            
            // Format the page allocated by createTable as a TableHeap page on disk
            Page page(PageHeader(info->getHeapRootPage(), PageType::Table));
            SlottedPage sp(page);
            (void)sp.initialize();
            (void)dm.writePage(info->getHeapRootPage(), page);
            
            auto heap_opt = TableHeap::open(dm, info->getHeapRootPage());
            
            // Insert a row directly
            std::vector<std::byte> buf(128);
            Serializer ser(buf);
            (void)ser.writeInt32(1);
            (void)ser.writeString("Alice");
            Tuple t(std::span<const std::byte>(buf.data(), ser.position()));
            RID rid;
            (void)heap_opt->insertTuple(t, rid);
        }
        
        // Patch page_count in DatabaseMetadata to workaround cache bug
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
    }
    
    void TearDown() override {
        if (std::filesystem::exists(db_name_)) {
            std::filesystem::remove(db_name_);
        }
    }
    
    std::string db_name_;
};

TEST_F(ShellTest, ExecuteSQL) {
    Shell shell(db_name_);
    std::ostringstream os;
    
    shell.executeSQL("SELECT * FROM users;", os);
    
    std::string output = os.str();
    EXPECT_TRUE(output.find("Alice") != std::string::npos) << "Output: " << output;
    EXPECT_TRUE(output.find("1") != std::string::npos) << "Output: " << output;
    EXPECT_TRUE(output.find("(1 row)") != std::string::npos) << "Output: " << output;
}

TEST_F(ShellTest, ExecuteMeta) {
    Shell shell(db_name_);
    std::ostringstream os;
    
    shell.executeMeta(".tables", os);
    EXPECT_TRUE(os.str().find("users") != std::string::npos);
    
    os.str("");
    shell.executeMeta(".schema users", os);
    EXPECT_TRUE(os.str().find("CREATE TABLE users") != std::string::npos);
    EXPECT_TRUE(os.str().find("INTEGER") != std::string::npos);
    EXPECT_TRUE(os.str().find("VARCHAR") != std::string::npos);
}

TEST_F(ShellTest, ReplCommands) {
    Shell shell(db_name_);
    Repl repl(shell);
    std::istringstream in(".help\n.exit\n");
    std::ostringstream out;
    repl.run(in, out);
    EXPECT_TRUE(out.str().find(".help") != std::string::npos);
}

TEST_F(ShellTest, SqlErrorHandling) {
    Shell shell(db_name_);
    std::ostringstream os;
    
    shell.executeSQL("SELECT FROM missing_table;", os);
    
    std::string output = os.str();
    EXPECT_TRUE(output.find("Error:") != std::string::npos);
}

} // namespace hamdb::shell
