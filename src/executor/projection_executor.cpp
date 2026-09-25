#include "executor/projection_executor.hpp"
#include "utils/serializer.hpp"

namespace hamdb {

ProjectionExecutor::ProjectionExecutor(std::unique_ptr<AbstractExecutor> child,
                                       std::vector<std::unique_ptr<Expression>> expressions,
                                       Schema output_schema)
    : child_(std::move(child)),
      expressions_(std::move(expressions)),
      output_schema_(std::move(output_schema)) {}

void ProjectionExecutor::init() {
    child_->init();
}

bool ProjectionExecutor::next(Tuple* tuple, RID* rid) {
    Tuple child_tuple;
    RID child_rid;
    
    if (!child_->next(&child_tuple, &child_rid)) {
        return false;
    }

    // Evaluate all expressions
    std::vector<Value> projected_values;
    projected_values.reserve(expressions_.size());
    for (const auto& expr : expressions_) {
        projected_values.push_back(expr->evaluate(child_tuple, child_->outputSchema()));
    }

    // Build the new tuple
    std::vector<std::byte> buf(1024);
    Serializer ser(buf);
    for (size_t i = 0; i < projected_values.size(); ++i) {
        auto type = output_schema_.getColumn(i).getType();
        if (type == ColumnType::Integer) {
            (void)ser.writeInt32(projected_values[i].getAsInteger());
        } else if (type == ColumnType::Boolean) {
            (void)ser.writeBool(projected_values[i].getAsBoolean());
        } else if (type == ColumnType::Varchar) {
            (void)ser.writeString(projected_values[i].getAsVarchar());
        }
    }
    
    *tuple = Tuple(std::span<const std::byte>(buf.data(), ser.position()));
    *rid = child_rid; // Preserve RID propagation
    
    return true;
}

const Schema& ProjectionExecutor::outputSchema() const {
    return output_schema_;
}

} // namespace hamdb
