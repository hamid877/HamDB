#pragma once

#include "planner/logical_plan.hpp"
#include "executor/expression.hpp"
#include <string>
#include <memory>

namespace hamdb::planner {

class LogicalIndexScanNode : public LogicalPlanNode {
public:
    LogicalIndexScanNode(Schema output_schema, std::string table_name, std::string table_alias, std::unique_ptr<hamdb::Expression> predicate)
        : LogicalPlanNode(LogicalPlanType::INDEX_SCAN, std::move(output_schema)),
          table_name_(std::move(table_name)), table_alias_(std::move(table_alias)), predicate_(std::move(predicate)) {}
    
    const std::string& getTableName() const { return table_name_; }
    const std::string& getTableAlias() const { return table_alias_; }
    const hamdb::Expression* getPredicate() const { return predicate_.get(); }
    std::unique_ptr<hamdb::Expression> takePredicate() { return std::move(predicate_); }

    std::string table_name_;
    std::string table_alias_;
    std::unique_ptr<hamdb::Expression> predicate_;
};

} // namespace hamdb::planner
