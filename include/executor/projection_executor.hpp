#pragma once

#include "executor/abstract_executor.hpp"
#include "executor/expression.hpp"
#include <memory>
#include <vector>

namespace hamdb {

class ProjectionExecutor : public AbstractExecutor {
public:
    ProjectionExecutor(std::unique_ptr<AbstractExecutor> child, 
                       std::vector<std::unique_ptr<Expression>> expressions,
                       Schema output_schema);

    void init() override;
    bool next(Tuple* tuple, RID* rid) override;
    const Schema& outputSchema() const override;

private:
    std::unique_ptr<AbstractExecutor> child_;
    std::vector<std::unique_ptr<Expression>> expressions_;
    Schema output_schema_;
};

} // namespace hamdb
