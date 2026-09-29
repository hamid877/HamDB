#include "planner/plan_formatter.hpp"
#include "planner/nested_loop_join_plan.hpp"
#include "planner/logical_index_scan.hpp"
#include <sstream>

namespace hamdb::planner {

FormattedPlanNode PlanFormatter::buildFormattedTree(const LogicalPlanNode* plan) {
    FormattedPlanNode node;
    if (!plan) return node;

    switch (plan->getType()) {
        case LogicalPlanType::SEQ_SCAN: {
            auto* p = static_cast<const SeqScanPlanNode*>(plan);
            node.name = "SEQ_SCAN";
            node.details = "table: " + p->getTableName();
            break;
        }
        case LogicalPlanType::NESTED_LOOP_JOIN: {
            node.name = "NESTED_LOOP_JOIN";
            break;
        }
        case LogicalPlanType::HASH_JOIN: {
            node.name = "HASH_JOIN";
            break;
        }
        case LogicalPlanType::INDEX_SCAN: {
            auto* p = static_cast<const LogicalIndexScanNode*>(plan);
            node.name = "INDEX_SCAN";
            node.details = "table: " + p->getTableName();
            break;
        }
        case LogicalPlanType::FILTER: node.name = "FILTER"; break;
        case LogicalPlanType::PROJECTION: node.name = "PROJECTION"; break;
        case LogicalPlanType::SORT: node.name = "SORT"; break;
        case LogicalPlanType::ORDER_BY: node.name = "ORDER_BY"; break;
        case LogicalPlanType::LIMIT: node.name = "LIMIT"; break;
        case LogicalPlanType::VALUES: node.name = "VALUES"; break;
        case LogicalPlanType::INSERT: {
            auto* p = static_cast<const InsertPlanNode*>(plan);
            node.name = "INSERT";
            node.details = "table: " + p->getTableName();
            break;
        }
        case LogicalPlanType::UPDATE: {
            auto* p = static_cast<const UpdatePlanNode*>(plan);
            node.name = "UPDATE";
            node.details = "table: " + p->getTableName();
            break;
        }
        case LogicalPlanType::DELETE: {
            auto* p = static_cast<const DeletePlanNode*>(plan);
            node.name = "DELETE";
            node.details = "table: " + p->getTableName();
            break;
        }
        case LogicalPlanType::AGGREGATION: node.name = "AGGREGATION"; break;
        case LogicalPlanType::HAVING: node.name = "HAVING"; break;
        default: node.name = "UNKNOWN"; break;
    }

    for (const auto& child : plan->getChildren()) {
        node.children.push_back(buildFormattedTree(child.get()));
    }
    return node;
}

FormattedPlanNode PlanFormatter::buildFormattedTree(const AbstractPlanNode* plan) {
    FormattedPlanNode node;
    if (!plan) return node;
    
    node.stats = plan->getStats();

    switch (plan->getType()) {
        case PhysicalPlanType::SEQ_SCAN: {
            auto* p = static_cast<const SeqScanPlan*>(plan);
            node.name = "SEQ_SCAN";
            node.details = "table: " + p->getTableName();
            break;
        }
        case PhysicalPlanType::NESTED_LOOP_JOIN: {
            node.name = "NESTED_LOOP_JOIN";
            break;
        }
        case PhysicalPlanType::HASH_JOIN: {
            node.name = "HASH_JOIN";
            break;
        }
        case PhysicalPlanType::INDEX_SCAN: {
            auto* p = static_cast<const IndexScanPlan*>(plan);
            node.name = "INDEX_SCAN";
            node.details = "table: " + p->getTableName();
            break;
        }
        case PhysicalPlanType::FILTER: node.name = "FILTER"; break;
        case PhysicalPlanType::PROJECTION: node.name = "PROJECTION"; break;
        case PhysicalPlanType::SORT: node.name = "SORT"; break;
        case PhysicalPlanType::ORDER_BY: node.name = "ORDER_BY"; break;
        case PhysicalPlanType::LIMIT: node.name = "LIMIT"; break;
        case PhysicalPlanType::VALUES: node.name = "VALUES"; break;
        case PhysicalPlanType::INSERT: {
            auto* p = static_cast<const InsertPlan*>(plan);
            node.name = "INSERT";
            node.details = "table: " + p->getTableName();
            break;
        }
        case PhysicalPlanType::UPDATE: {
            auto* p = static_cast<const UpdatePlan*>(plan);
            node.name = "UPDATE";
            node.details = "table: " + p->getTableName();
            break;
        }
        case PhysicalPlanType::DELETE: {
            auto* p = static_cast<const DeletePlan*>(plan);
            node.name = "DELETE";
            node.details = "table: " + p->getTableName();
            break;
        }
        case PhysicalPlanType::AGGREGATION: node.name = "AGGREGATION"; break;
        case PhysicalPlanType::HAVING: node.name = "HAVING"; break;
        default: node.name = "UNKNOWN"; break;
    }

    for (const auto& child : plan->getChildren()) {
        node.children.push_back(buildFormattedTree(child.get()));
    }
    return node;
}

void PlanFormatter::computeRowsIn(FormattedPlanNode& node) {
    for (auto& child : node.children) {
        computeRowsIn(child);
        if (node.stats && child.stats) {
            node.stats->rows_in += child.stats->rows_out;
        }
    }
}

std::string PlanFormatter::renderTree(const FormattedPlanNode& node, bool analyze, const std::string& prefix, bool is_last) {
    std::string out = prefix;
    out += (is_last ? "└── " : "├── ");
    out += node.name;
    if (!node.details.empty()) {
        out += " (" + node.details + ")";
    }

    if (analyze && node.stats) {
        std::stringstream ss;
        ss << " (rows_in=" << node.stats->rows_in
           << ", rows_out=" << node.stats->rows_out
           << ", time=" << node.stats->execution_time.count() << "us)";
        out += ss.str();
    }
    out += "\n";

    std::string child_prefix = prefix + (is_last ? "    " : "│   ");
    for (size_t i = 0; i < node.children.size(); ++i) {
        out += renderTree(node.children[i], analyze, child_prefix, i == node.children.size() - 1);
    }
    return out;
}

} // namespace hamdb::planner
