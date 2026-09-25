#include "executor/aggregation_executor.hpp"
#include "executor/column_value_expression.hpp"
#include "executor/constant_expression.hpp"
#include "utils/serializer.hpp"

#include <gtest/gtest.h>
#include <vector>

namespace hamdb
{
    namespace
    {

        class MockExecutor : public AbstractExecutor
        {
        public:
            MockExecutor(Schema schema, std::vector<Tuple> tuples)
                : schema_(std::move(schema)), tuples_(std::move(tuples))
            {
            }

            void init() override
            {
                idx_ = 0;
            }

            bool next(Tuple* tuple, RID* rid) override
            {
                if (idx_ < tuples_.size())
                {
                    *tuple = tuples_[idx_];
                    *rid = RID(0, idx_);
                    idx_++;
                    return true;
                }
                return false;
            }

            const Schema& outputSchema() const override
            {
                return schema_;
            }

        private:
            Schema schema_;
            std::vector<Tuple> tuples_;
            size_t idx_{0};
        };

        class AggregationExecutorTest : public ::testing::Test
        {
        protected:
            void SetUp() override
            {
                std::vector<Column> cols;
                cols.emplace_back("group_col", ColumnType::Integer);
                cols.emplace_back("val_col", ColumnType::Integer);
                child_schema_ = std::make_unique<Schema>(std::move(cols));
            }

            std::unique_ptr<Schema> child_schema_;
        };

        Tuple buildTuple(int32_t a, int32_t b)
        {
            std::vector<std::byte> buf(100);
            Serializer ser(buf);
            (void)ser.writeInt32(a);
            (void)ser.writeInt32(b);
            return Tuple(std::span<const std::byte>(buf.data(), ser.position()));
        }

        TEST_F(AggregationExecutorTest, BasicAggregation)
        {
            std::vector<Tuple> tuples;
            tuples.push_back(buildTuple(1, 10));
            tuples.push_back(buildTuple(1, 20));
            tuples.push_back(buildTuple(2, 50));
            tuples.push_back(buildTuple(2, 50));
            tuples.push_back(buildTuple(2, 20));

            auto mock_child = std::make_unique<MockExecutor>(*child_schema_, tuples);

            // Group By: group_col
            std::vector<std::unique_ptr<Expression>> group_bys;
            group_bys.push_back(std::make_unique<ColumnValueExpression>(0));

            // Aggregates: COUNT(*), SUM(val_col), MAX(val_col)
            std::vector<std::unique_ptr<Expression>> agg_exprs;
            agg_exprs.push_back(std::make_unique<ConstantExpression>(
                Value(1))); // COUNT(*) doesn't actually evaluate it this way, but we pass something
            agg_exprs.push_back(std::make_unique<ColumnValueExpression>(1));
            agg_exprs.push_back(std::make_unique<ColumnValueExpression>(1));

            std::vector<AggregateType> agg_types = {AggregateType::CountStar, AggregateType::Sum,
                                                    AggregateType::Max};

            std::vector<Column> out_cols;
            out_cols.emplace_back("group_col", ColumnType::Integer);
            out_cols.emplace_back("count_star", ColumnType::Integer);
            out_cols.emplace_back("sum_val", ColumnType::Integer);
            out_cols.emplace_back("max_val", ColumnType::Integer);
            Schema out_schema(std::move(out_cols));

            AggregationExecutor agg(std::move(mock_child), std::move(group_bys),
                                    std::move(agg_exprs), agg_types, out_schema);
            agg.init();

            Tuple tuple;
            RID rid;

            // Using simple vector to collect results since hash table order is non-deterministic
            std::vector<std::vector<int32_t>> results;

            ColumnValueExpression out_col0(0);
            ColumnValueExpression out_col1(1);
            ColumnValueExpression out_col2(2);
            ColumnValueExpression out_col3(3);

            while (agg.next(&tuple, &rid))
            {
                results.push_back({out_col0.evaluate(tuple, out_schema).getAsInteger(),
                                   out_col1.evaluate(tuple, out_schema).getAsInteger(),
                                   out_col2.evaluate(tuple, out_schema).getAsInteger(),
                                   out_col3.evaluate(tuple, out_schema).getAsInteger()});
            }

            EXPECT_EQ(results.size(), 2);

            bool found_group_1 = false;
            bool found_group_2 = false;

            for (const auto& row : results)
            {
                if (row[0] == 1)
                {
                    EXPECT_EQ(row[1], 2);  // COUNT(*)
                    EXPECT_EQ(row[2], 30); // SUM
                    EXPECT_EQ(row[3], 20); // MAX
                    found_group_1 = true;
                }
                else if (row[0] == 2)
                {
                    EXPECT_EQ(row[1], 3);   // COUNT(*)
                    EXPECT_EQ(row[2], 120); // SUM
                    EXPECT_EQ(row[3], 50);  // MAX
                    found_group_2 = true;
                }
            }

            EXPECT_TRUE(found_group_1);
            EXPECT_TRUE(found_group_2);
        }

        TEST_F(AggregationExecutorTest, EmptyTableWithoutGroupBy)
        {
            std::vector<Tuple> tuples;
            auto mock_child = std::make_unique<MockExecutor>(*child_schema_, tuples);

            std::vector<std::unique_ptr<Expression>> group_bys; // Empty
            std::vector<std::unique_ptr<Expression>> agg_exprs;
            agg_exprs.push_back(std::make_unique<ConstantExpression>(Value(1))); // COUNT(*)

            std::vector<AggregateType> agg_types = {AggregateType::CountStar};

            std::vector<Column> out_cols;
            out_cols.emplace_back("count_star", ColumnType::Integer);
            Schema out_schema(std::move(out_cols));

            AggregationExecutor agg(std::move(mock_child), std::move(group_bys),
                                    std::move(agg_exprs), agg_types, out_schema);
            agg.init();

            Tuple tuple;
            RID rid;

            ASSERT_TRUE(agg.next(&tuple, &rid));
            ColumnValueExpression out_col0(0);
            EXPECT_EQ(out_col0.evaluate(tuple, out_schema).getAsInteger(), 0);

            EXPECT_FALSE(agg.next(&tuple, &rid));
        }

        TEST_F(AggregationExecutorTest, EmptyTableWithGroupBy)
        {
            std::vector<Tuple> tuples;
            auto mock_child = std::make_unique<MockExecutor>(*child_schema_, tuples);

            std::vector<std::unique_ptr<Expression>> group_bys;
            group_bys.push_back(std::make_unique<ColumnValueExpression>(0));

            std::vector<std::unique_ptr<Expression>> agg_exprs;
            agg_exprs.push_back(std::make_unique<ConstantExpression>(Value(1)));

            std::vector<AggregateType> agg_types = {AggregateType::CountStar};

            std::vector<Column> out_cols;
            out_cols.emplace_back("group_col", ColumnType::Integer);
            out_cols.emplace_back("count_star", ColumnType::Integer);
            Schema out_schema(std::move(out_cols));

            AggregationExecutor agg(std::move(mock_child), std::move(group_bys),
                                    std::move(agg_exprs), agg_types, out_schema);
            agg.init();

            Tuple tuple;
            RID rid;

            // Empty table with GROUP BY should return 0 rows.
            EXPECT_FALSE(agg.next(&tuple, &rid));
        }

    } // namespace
} // namespace hamdb
