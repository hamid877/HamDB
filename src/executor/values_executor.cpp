#include "executor/values_executor.hpp"
#include "utils/serializer.hpp"

namespace hamdb
{

    ValuesExecutor::ValuesExecutor(std::vector<std::vector<std::unique_ptr<Expression>>> values,
                                   Schema output_schema)
        : values_(std::move(values)), output_schema_(std::move(output_schema))
    {
    }

    void ValuesExecutor::init()
    {
        cursor_ = 0;
    }

    bool ValuesExecutor::next(Tuple* tuple, RID* rid)
    {
        if (cursor_ >= values_.size())
        {
            return false;
        }

        const auto& row_exprs = values_[cursor_];

        // Evaluate with dummy tuple and schema since values are constants
        Tuple dummy_tuple;
        Schema dummy_schema;

        std::vector<std::byte> buf(1024);
        Serializer ser(buf);

        for (std::size_t i = 0; i < row_exprs.size(); ++i)
        {
            Value val = row_exprs[i]->evaluate(dummy_tuple, dummy_schema);
            auto type = output_schema_.getColumn(i).getType();

            if (type == ColumnType::Integer)
            {
                (void)ser.writeInt32(val.getAsInteger());
            }
            else if (type == ColumnType::Boolean)
            {
                (void)ser.writeBool(val.getAsBoolean());
            }
            else if (type == ColumnType::Varchar)
            {
                (void)ser.writeString(val.getAsVarchar());
            }
        }

        *tuple = Tuple(std::span<const std::byte>(buf.data(), ser.position()));
        *rid = RID{};

        cursor_++;
        return true;
    }

    const Schema& ValuesExecutor::outputSchema() const
    {
        return output_schema_;
    }

} // namespace hamdb
