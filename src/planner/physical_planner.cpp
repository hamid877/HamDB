#include "planner/physical_planner.hpp"
#include "executor/column_value_expression.hpp"
#include <stdexcept>

namespace hamdb::planner {

std::unique_ptr<AbstractPlanNode> PhysicalPlanner::plan(std::unique_ptr<LogicalPlanNode> logical_plan) {
    return planNode(std::move(logical_plan));
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
std::unique_ptr<AbstractPlanNode> PhysicalPlanner::planNode(std::unique_ptr<LogicalPlanNode> logical_node) {
    if (!logical_node) return nullptr;

    std::unique_ptr<AbstractPlanNode> physical_node;

    switch (logical_node->getType()) {
        case LogicalPlanType::SEQ_SCAN: {
            auto* seq_scan = dynamic_cast<SeqScanPlanNode*>(logical_node.get());
            physical_node = std::make_unique<SeqScanPlan>(
                seq_scan->getOutputSchema(), seq_scan->table_name_, seq_scan->table_alias_, std::move(seq_scan->predicate_));
            break;
        }
        case LogicalPlanType::FILTER: {
            auto* filter = dynamic_cast<FilterPlanNode*>(logical_node.get());
            physical_node = std::make_unique<FilterPlan>(
                filter->getOutputSchema(), std::move(filter->predicate_));
            break;
        }
        case LogicalPlanType::PROJECTION: {
            auto* proj = dynamic_cast<ProjectionPlanNode*>(logical_node.get());
            physical_node = std::make_unique<ProjectionPlan>(
                proj->getOutputSchema(), std::move(proj->expressions_));
            break;
        }
        case LogicalPlanType::SORT: {
            auto* sort = dynamic_cast<SortPlanNode*>(logical_node.get());
            std::vector<std::pair<hamdb::OrderByType, std::unique_ptr<hamdb::Expression>>> order_bys;
            for (auto& pair : sort->order_by_) {
                hamdb::OrderByType type = pair.second ? hamdb::OrderByType::ASC : hamdb::OrderByType::DESC;
                order_bys.emplace_back(type, std::move(pair.first));
            }
            physical_node = std::make_unique<SortPlan>(
                sort->getOutputSchema(), std::move(order_bys));
            break;
        }
        case LogicalPlanType::LIMIT: {
            auto* limit_node = dynamic_cast<LimitPlanNode*>(logical_node.get());
            std::size_t limit = 0;
            std::size_t offset = 0;
            if (limit_node->limit_) {
                auto val = limit_node->limit_->evaluate(Tuple{}, Schema(std::vector<Column>{}));
                limit = val.getAsInteger();
            }
            if (limit_node->offset_) {
                auto val = limit_node->offset_->evaluate(Tuple{}, Schema(std::vector<Column>{}));
                offset = val.getAsInteger();
            }
            physical_node = std::make_unique<LimitPlan>(
                limit_node->getOutputSchema(), limit, offset);
            break;
        }
        case LogicalPlanType::VALUES: {
            auto* values_node = dynamic_cast<ValuesPlanNode*>(logical_node.get());
            physical_node = std::make_unique<ValuesPlan>(
                values_node->getOutputSchema(), std::move(values_node->values_));
            break;
        }
        case LogicalPlanType::INSERT: {
            auto* insert_node = dynamic_cast<InsertPlanNode*>(logical_node.get());
            physical_node = std::make_unique<InsertPlan>(
                insert_node->getOutputSchema(), insert_node->table_name_);
            break;
        }
        case LogicalPlanType::UPDATE: {
            auto* update_node = dynamic_cast<UpdatePlanNode*>(logical_node.get());
            TableInfo* table_info = nullptr;
            if (catalog_->getTable(update_node->table_name_, table_info) != Status::Ok) {
                throw std::runtime_error("Table not found: " + update_node->table_name_);
            }
            
            std::vector<std::unique_ptr<hamdb::Expression>> target_exprs;
            const auto& schema = table_info->getSchema();
            
            for (uint32_t i = 0; i < schema.getColumnCount(); ++i) {
                const auto& col = schema.getColumn(i);
                bool found = false;
                for (auto& pair : update_node->set_clauses_) {
                    if (pair.first == col.getName()) {
                        target_exprs.push_back(std::move(pair.second));
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    target_exprs.push_back(std::make_unique<ColumnValueExpression>(i));
                }
            }
            physical_node = std::make_unique<UpdatePlan>(
                update_node->getOutputSchema(), update_node->table_name_, std::move(target_exprs));
            break;
        }
        case LogicalPlanType::DELETE: {
            auto* delete_node = dynamic_cast<DeletePlanNode*>(logical_node.get());
            physical_node = std::make_unique<DeletePlan>(
                delete_node->getOutputSchema(), delete_node->table_name_);
            break;
        }
        default:
            throw std::runtime_error("Unsupported logical plan node type");
    }

    for (auto& child : logical_node->children_) {
        physical_node->addChild(planNode(std::move(child)));
    }

    return physical_node;
}

} // namespace hamdb::planner
