#pragma once

#include "optimizer/rule.hpp"
#include "catalog/catalog_manager.hpp"

namespace hamdb::optimizer {

class IndexScanRule : public Rule {
public:
    explicit IndexScanRule(CatalogManager* catalog) : catalog_(catalog) {}
    
    std::unique_ptr<planner::LogicalPlanNode> apply(std::unique_ptr<planner::LogicalPlanNode> plan) override;
    std::string name() const override { return "IndexScanRule"; }
    
private:
    CatalogManager* catalog_;
};

} // namespace hamdb::optimizer
