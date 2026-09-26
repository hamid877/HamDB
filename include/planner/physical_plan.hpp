#pragma once

#include "catalog/schema.hpp"
#include "executor/expression.hpp"
#include "executor/sort_executor.hpp" // for OrderByType
#include <memory>
#include <vector>
#include <string>

namespace hamdb::planner {

enum class PhysicalPlanType {
    SEQ_SCAN,
    FILTER,
    PROJECTION,
    SORT,
    LIMIT,
    VALUES,
    INSERT,
    UPDATE,
    DELETE
};

class AbstractPlanNode {
public:
    AbstractPlanNode(PhysicalPlanType type, Schema output_schema)
        : type_(type), output_schema_(std::move(output_schema)) {}
    virtual ~AbstractPlanNode() = default;

    PhysicalPlanType getType() const { return type_; }
    const Schema& getOutputSchema() const { return output_schema_; }

    void addChild(std::unique_ptr<AbstractPlanNode> child) {
        children_.push_back(std::move(child));
    }
    std::vector<std::unique_ptr<AbstractPlanNode>>& getChildren() {
        return children_;
    }
    const std::vector<std::unique_ptr<AbstractPlanNode>>& getChildren() const {
        return children_;
    }

protected:
    PhysicalPlanType type_;
    Schema output_schema_;
    std::vector<std::unique_ptr<AbstractPlanNode>> children_;
};

class SeqScanPlan : public AbstractPlanNode {
public:
    SeqScanPlan(Schema output_schema, std::string table_name, std::string table_alias, std::unique_ptr<hamdb::Expression> predicate = nullptr)
        : AbstractPlanNode(PhysicalPlanType::SEQ_SCAN, std::move(output_schema)),
          table_name_(std::move(table_name)), table_alias_(std::move(table_alias)), predicate_(std::move(predicate)) {}
    
    const std::string& getTableName() const { return table_name_; }
    const std::string& getTableAlias() const { return table_alias_; }
    std::unique_ptr<hamdb::Expression>& getPredicate() { return predicate_; }
    const std::unique_ptr<hamdb::Expression>& getPredicate() const { return predicate_; }

private:
    std::string table_name_;
    std::string table_alias_;
    std::unique_ptr<hamdb::Expression> predicate_;
};

class FilterPlan : public AbstractPlanNode {
public:
    FilterPlan(Schema output_schema, std::unique_ptr<hamdb::Expression> predicate)
        : AbstractPlanNode(PhysicalPlanType::FILTER, std::move(output_schema)),
          predicate_(std::move(predicate)) {}
    
    std::unique_ptr<hamdb::Expression>& getPredicate() { return predicate_; }
    const std::unique_ptr<hamdb::Expression>& getPredicate() const { return predicate_; }
private:
    std::unique_ptr<hamdb::Expression> predicate_;
};

class ProjectionPlan : public AbstractPlanNode {
public:
    ProjectionPlan(Schema output_schema, std::vector<std::unique_ptr<hamdb::Expression>> expressions)
        : AbstractPlanNode(PhysicalPlanType::PROJECTION, std::move(output_schema)),
          expressions_(std::move(expressions)) {}
          
    std::vector<std::unique_ptr<hamdb::Expression>>& getExpressions() { return expressions_; }
    const std::vector<std::unique_ptr<hamdb::Expression>>& getExpressions() const { return expressions_; }
private:
    std::vector<std::unique_ptr<hamdb::Expression>> expressions_;
};

class SortPlan : public AbstractPlanNode {
public:
    SortPlan(Schema output_schema, std::vector<std::pair<hamdb::OrderByType, std::unique_ptr<hamdb::Expression>>> order_by)
        : AbstractPlanNode(PhysicalPlanType::SORT, std::move(output_schema)),
          order_by_(std::move(order_by)) {}
          
    std::vector<std::pair<hamdb::OrderByType, std::unique_ptr<hamdb::Expression>>>& getOrderBy() { return order_by_; }
    const std::vector<std::pair<hamdb::OrderByType, std::unique_ptr<hamdb::Expression>>>& getOrderBy() const { return order_by_; }
private:
    std::vector<std::pair<hamdb::OrderByType, std::unique_ptr<hamdb::Expression>>> order_by_;
};

class LimitPlan : public AbstractPlanNode {
public:
    LimitPlan(Schema output_schema, std::size_t limit, std::size_t offset)
        : AbstractPlanNode(PhysicalPlanType::LIMIT, std::move(output_schema)),
          limit_(limit), offset_(offset) {}
          
    std::size_t getLimit() const { return limit_; }
    std::size_t getOffset() const { return offset_; }
private:
    std::size_t limit_;
    std::size_t offset_;
};

class ValuesPlan : public AbstractPlanNode {
public:
    ValuesPlan(Schema output_schema, std::vector<std::vector<std::unique_ptr<hamdb::Expression>>> values)
        : AbstractPlanNode(PhysicalPlanType::VALUES, std::move(output_schema)),
          values_(std::move(values)) {}
          
    std::vector<std::vector<std::unique_ptr<hamdb::Expression>>>& getValues() { return values_; }
    const std::vector<std::vector<std::unique_ptr<hamdb::Expression>>>& getValues() const { return values_; }
private:
    std::vector<std::vector<std::unique_ptr<hamdb::Expression>>> values_;
};

class InsertPlan : public AbstractPlanNode {
public:
    InsertPlan(Schema output_schema, std::string table_name)
        : AbstractPlanNode(PhysicalPlanType::INSERT, std::move(output_schema)),
          table_name_(std::move(table_name)) {}
          
    const std::string& getTableName() const { return table_name_; }
private:
    std::string table_name_;
};

class UpdatePlan : public AbstractPlanNode {
public:
    UpdatePlan(Schema output_schema, std::string table_name, std::vector<std::unique_ptr<hamdb::Expression>> target_expressions)
        : AbstractPlanNode(PhysicalPlanType::UPDATE, std::move(output_schema)),
          table_name_(std::move(table_name)), target_expressions_(std::move(target_expressions)) {}
          
    const std::string& getTableName() const { return table_name_; }
    std::vector<std::unique_ptr<hamdb::Expression>>& getTargetExpressions() { return target_expressions_; }
    const std::vector<std::unique_ptr<hamdb::Expression>>& getTargetExpressions() const { return target_expressions_; }
private:
    std::string table_name_;
    std::vector<std::unique_ptr<hamdb::Expression>> target_expressions_;
};

class DeletePlan : public AbstractPlanNode {
public:
    DeletePlan(Schema output_schema, std::string table_name)
        : AbstractPlanNode(PhysicalPlanType::DELETE, std::move(output_schema)),
          table_name_(std::move(table_name)) {}
          
    const std::string& getTableName() const { return table_name_; }
private:
    std::string table_name_;
};

} // namespace hamdb::planner
