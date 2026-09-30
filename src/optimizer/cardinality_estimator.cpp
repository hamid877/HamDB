#include "optimizer/cardinality_estimator.hpp"
#include "executor/column_value_expression.hpp"
#include "executor/comparison_expression.hpp"
#include "executor/constant_expression.hpp"
#include "executor/logical_expression.hpp"
#include <algorithm>

namespace hamdb
{

    double CardinalityEstimator::estimateSelectivity(const Expression* predicate,
                                                     const TableStatistics& stats,
                                                     const Schema& /*schema*/) const
    {
        if (!predicate)
        {
            return 1.0;
        }

        if (auto logical_expr = dynamic_cast<const LogicalExpression*>(predicate))
        {
            if (logical_expr->getLogicType() == LogicalType::And)
            {
                double left = estimateSelectivity(logical_expr->getChildren()[0].get(), stats, Schema());
                double right = estimateSelectivity(logical_expr->getChildren()[1].get(), stats, Schema());
                return left * right;
            }
            if (logical_expr->getLogicType() == LogicalType::Or)
            {
                double left = estimateSelectivity(logical_expr->getChildren()[0].get(), stats, Schema());
                double right = estimateSelectivity(logical_expr->getChildren()[1].get(), stats, Schema());
                return left + right - (left * right);
            }
            if (logical_expr->getLogicType() == LogicalType::Not)
            {
                double child = estimateSelectivity(logical_expr->getChildren()[0].get(), stats, Schema());
                return std::max(0.0, 1.0 - child);
            }
        }
        else if (auto comp_expr = dynamic_cast<const ComparisonExpression*>(predicate))
        {
            auto col_expr = dynamic_cast<const ColumnValueExpression*>(comp_expr->getChildren()[0].get());
            auto const_expr = dynamic_cast<const ConstantExpression*>(comp_expr->getChildren()[1].get());
            
            bool swapped = false;
            if (!col_expr || !const_expr)
            {
                col_expr = dynamic_cast<const ColumnValueExpression*>(comp_expr->getChildren()[1].get());
                const_expr = dynamic_cast<const ConstantExpression*>(comp_expr->getChildren()[0].get());
                swapped = true;
            }

            if (col_expr && const_expr)
            {
                uint32_t col_idx = col_expr->getColIdx();
                if (col_idx < stats.column_stats.size())
                {
                    const auto& col_stat = stats.column_stats[col_idx];
                    auto comp_type = comp_expr->getComparisonType();

                    if (comp_type == ComparisonType::Equal)
                    {
                        return col_stat.distinct_count > 0 ? 1.0 / static_cast<double>(col_stat.distinct_count) : 1.0;
                    }
                    if (comp_type == ComparisonType::NotEqual)
                    {
                        return col_stat.distinct_count > 0 ? 1.0 - (1.0 / static_cast<double>(col_stat.distinct_count)) : 1.0;
                    }

                    if (col_stat.min_value && col_stat.max_value)
                    {
                        Tuple empty_tuple;
                        Schema empty_schema;
                        Value const_val = const_expr->evaluate(empty_tuple, empty_schema);

                        if (const_val.getType() == TypeId::Integer && col_stat.min_value->getType() == TypeId::Integer)
                        {
                            double min_v = static_cast<double>(col_stat.min_value->getAsInteger());
                            double max_v = static_cast<double>(col_stat.max_value->getAsInteger());
                            double val_v = static_cast<double>(const_val.getAsInteger());

                            if (max_v == min_v)
                                return 1.0;

                            if (comp_type == ComparisonType::LessThan || comp_type == ComparisonType::LessThanOrEqual)
                            {
                                if (swapped) {
                                    if (val_v >= max_v) return 0.0;
                                    if (val_v <= min_v) return 1.0;
                                    return (max_v - val_v) / (max_v - min_v);
                                } else {
                                    if (val_v <= min_v) return 0.0;
                                    if (val_v >= max_v) return 1.0;
                                    return (val_v - min_v) / (max_v - min_v);
                                }
                            }
                            else if (comp_type == ComparisonType::GreaterThan || comp_type == ComparisonType::GreaterThanOrEqual)
                            {
                                if (swapped) {
                                    if (val_v <= min_v) return 0.0;
                                    if (val_v >= max_v) return 1.0;
                                    return (val_v - min_v) / (max_v - min_v);
                                } else {
                                    if (val_v >= max_v) return 0.0;
                                    if (val_v <= min_v) return 1.0;
                                    return (max_v - val_v) / (max_v - min_v);
                                }
                            }
                        }
                    }
                    return 0.33; // Default for range predicates without stats or for non-integers
                }
            }
        }
        
        return 1.0;
    }

    double CardinalityEstimator::estimateJoinSelectivity(const Expression* predicate,
                                                         const TableStatistics& left_stats,
                                                         const Schema& left_schema,
                                                         const TableStatistics& right_stats,
                                                         const Schema& right_schema) const
    {
        if (!predicate)
        {
            return 1.0;
        }

        if (auto logical_expr = dynamic_cast<const LogicalExpression*>(predicate))
        {
            if (logical_expr->getLogicType() == LogicalType::And)
            {
                double left = estimateJoinSelectivity(logical_expr->getChildren()[0].get(), left_stats, left_schema, right_stats, right_schema);
                double right = estimateJoinSelectivity(logical_expr->getChildren()[1].get(), left_stats, left_schema, right_stats, right_schema);
                return left * right;
            }
        }
        else if (auto comp_expr = dynamic_cast<const ComparisonExpression*>(predicate))
        {
            if (comp_expr->getComparisonType() == ComparisonType::Equal)
            {
                auto left_col_expr = dynamic_cast<const ColumnValueExpression*>(comp_expr->getChildren()[0].get());
                auto right_col_expr = dynamic_cast<const ColumnValueExpression*>(comp_expr->getChildren()[1].get());
                
                if (left_col_expr && right_col_expr) {
                    uint32_t l_idx = left_col_expr->getColIdx();
                    uint32_t r_idx = right_col_expr->getColIdx();
                    
                    if (l_idx < left_stats.column_stats.size() && r_idx < right_stats.column_stats.size()) {
                        std::size_t l_distinct = left_stats.column_stats[l_idx].distinct_count;
                        std::size_t r_distinct = right_stats.column_stats[r_idx].distinct_count;
                        
                        std::size_t max_distinct = std::max(std::max(l_distinct, r_distinct), static_cast<std::size_t>(1));
                        return 1.0 / static_cast<double>(max_distinct);
                    }
                }
            }
        }

        return 1.0;
    }

} // namespace hamdb
