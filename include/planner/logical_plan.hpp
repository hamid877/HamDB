#pragma once

#include "catalog/schema.hpp"
#include "executor/expression.hpp"
#include <memory>
#include <vector>
#include <string>

namespace hamdb::planner {

enum class LogicalPlanType {
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

class LogicalPlanNode {
public:
    LogicalPlanNode(LogicalPlanType type, Schema output_schema)
        : type_(type), output_schema_(std::move(output_schema)) {}
    virtual ~LogicalPlanNode() = default;

    LogicalPlanType getType() const { return type_; }
    const Schema& getOutputSchema() const { return output_schema_; }
    
    void addChild(std::unique_ptr<LogicalPlanNode> child) {
        children_.push_back(std::move(child));
    }
    const std::vector<std::unique_ptr<LogicalPlanNode>>& getChildren() const {
        return children_;
    }

protected:
    LogicalPlanType type_;
    Schema output_schema_;
    std::vector<std::unique_ptr<LogicalPlanNode>> children_;
    friend class PhysicalPlanner;
};

class SeqScanPlanNode : public LogicalPlanNode {
public:
    SeqScanPlanNode(Schema output_schema, std::string table_name, std::string table_alias)
        : LogicalPlanNode(LogicalPlanType::SEQ_SCAN, std::move(output_schema)),
          table_name_(std::move(table_name)), table_alias_(std::move(table_alias)) {}
    
    const std::string& getTableName() const { return table_name_; }
    const std::string& getTableAlias() const { return table_alias_; }

private:
    std::string table_name_;
    std::string table_alias_;
    friend class PhysicalPlanner;
};

class FilterPlanNode : public LogicalPlanNode {
public:
    FilterPlanNode(Schema output_schema, std::unique_ptr<hamdb::Expression> predicate)
        : LogicalPlanNode(LogicalPlanType::FILTER, std::move(output_schema)),
          predicate_(std::move(predicate)) {}
    
    const hamdb::Expression* getPredicate() const { return predicate_.get(); }
private:
    std::unique_ptr<hamdb::Expression> predicate_;
    friend class PhysicalPlanner;
};

class ProjectionPlanNode : public LogicalPlanNode {
public:
    ProjectionPlanNode(Schema output_schema, std::vector<std::unique_ptr<hamdb::Expression>> expressions)
        : LogicalPlanNode(LogicalPlanType::PROJECTION, std::move(output_schema)),
          expressions_(std::move(expressions)) {}
          
    const std::vector<std::unique_ptr<hamdb::Expression>>& getExpressions() const { return expressions_; }
private:
    std::vector<std::unique_ptr<hamdb::Expression>> expressions_;
    friend class PhysicalPlanner;
};

class SortPlanNode : public LogicalPlanNode {
public:
    SortPlanNode(Schema output_schema, std::vector<std::pair<std::unique_ptr<hamdb::Expression>, bool>> order_by)
        : LogicalPlanNode(LogicalPlanType::SORT, std::move(output_schema)),
          order_by_(std::move(order_by)) {}
          
    const std::vector<std::pair<std::unique_ptr<hamdb::Expression>, bool>>& getOrderBy() const { return order_by_; }
private:
    std::vector<std::pair<std::unique_ptr<hamdb::Expression>, bool>> order_by_;
    friend class PhysicalPlanner;
};

class LimitPlanNode : public LogicalPlanNode {
public:
    LimitPlanNode(Schema output_schema, std::unique_ptr<hamdb::Expression> limit, std::unique_ptr<hamdb::Expression> offset)
        : LogicalPlanNode(LogicalPlanType::LIMIT, std::move(output_schema)),
          limit_(std::move(limit)), offset_(std::move(offset)) {}
          
    const hamdb::Expression* getLimit() const { return limit_.get(); }
    const hamdb::Expression* getOffset() const { return offset_.get(); }
private:
    std::unique_ptr<hamdb::Expression> limit_;
    std::unique_ptr<hamdb::Expression> offset_;
    friend class PhysicalPlanner;
};

class ValuesPlanNode : public LogicalPlanNode {
public:
    ValuesPlanNode(Schema output_schema, std::vector<std::vector<std::unique_ptr<hamdb::Expression>>> values)
        : LogicalPlanNode(LogicalPlanType::VALUES, std::move(output_schema)),
          values_(std::move(values)) {}
          
    const std::vector<std::vector<std::unique_ptr<hamdb::Expression>>>& getValues() const { return values_; }
private:
    std::vector<std::vector<std::unique_ptr<hamdb::Expression>>> values_;
    friend class PhysicalPlanner;
};

class InsertPlanNode : public LogicalPlanNode {
public:
    InsertPlanNode(Schema output_schema, std::string table_name)
        : LogicalPlanNode(LogicalPlanType::INSERT, std::move(output_schema)),
          table_name_(std::move(table_name)) {}
          
    const std::string& getTableName() const { return table_name_; }
private:
    std::string table_name_;
    friend class PhysicalPlanner;
};

class UpdatePlanNode : public LogicalPlanNode {
public:
    UpdatePlanNode(Schema output_schema, std::string table_name, std::vector<std::pair<std::string, std::unique_ptr<hamdb::Expression>>> set_clauses)
        : LogicalPlanNode(LogicalPlanType::UPDATE, std::move(output_schema)),
          table_name_(std::move(table_name)), set_clauses_(std::move(set_clauses)) {}
          
    const std::string& getTableName() const { return table_name_; }
    const std::vector<std::pair<std::string, std::unique_ptr<hamdb::Expression>>>& getSetClauses() const { return set_clauses_; }
private:
    std::string table_name_;
    std::vector<std::pair<std::string, std::unique_ptr<hamdb::Expression>>> set_clauses_;
    friend class PhysicalPlanner;
};

class DeletePlanNode : public LogicalPlanNode {
public:
    DeletePlanNode(Schema output_schema, std::string table_name)
        : LogicalPlanNode(LogicalPlanType::DELETE, std::move(output_schema)),
          table_name_(std::move(table_name)) {}
          
    const std::string& getTableName() const { return table_name_; }
private:
    std::string table_name_;
    friend class PhysicalPlanner;
};

} // namespace hamdb::planner
