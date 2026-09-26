#pragma once

#include "binder/bound_expression.hpp"
#include <memory>
#include <vector>
#include <string>
#include <utility>

namespace hamdb::binder {

enum class BoundStatementType {
    SELECT,
    INSERT,
    UPDATE,
    DELETE,
    VALUES
};

class BoundStatement {
public:
    virtual ~BoundStatement() = default;
    virtual BoundStatementType getType() const = 0;
};

class BoundSelectStatement : public BoundStatement {
public:
    BoundStatementType getType() const override { return BoundStatementType::SELECT; }

    std::string table_name_;
    std::string table_alias_;
    std::vector<std::unique_ptr<BoundExpression>> select_list_;
    std::unique_ptr<BoundExpression> where_clause_;
    std::vector<std::pair<std::unique_ptr<BoundExpression>, bool>> order_by_;
    std::unique_ptr<BoundExpression> limit_;
    std::unique_ptr<BoundExpression> offset_;
};

class BoundInsertStatement : public BoundStatement {
public:
    BoundStatementType getType() const override { return BoundStatementType::INSERT; }

    std::string table_name_;
    std::vector<std::vector<std::unique_ptr<BoundExpression>>> values_;
};

class BoundUpdateStatement : public BoundStatement {
public:
    BoundStatementType getType() const override { return BoundStatementType::UPDATE; }

    std::string table_name_;
    std::vector<std::pair<std::string, std::unique_ptr<BoundExpression>>> set_clauses_;
    std::unique_ptr<BoundExpression> where_clause_;
};

class BoundDeleteStatement : public BoundStatement {
public:
    BoundStatementType getType() const override { return BoundStatementType::DELETE; }

    std::string table_name_;
    std::unique_ptr<BoundExpression> where_clause_;
};

class BoundValuesStatement : public BoundStatement {
public:
    BoundStatementType getType() const override { return BoundStatementType::VALUES; }

    std::vector<std::vector<std::unique_ptr<BoundExpression>>> values_;
};

} // namespace hamdb::binder
