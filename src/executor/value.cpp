#include "executor/value.hpp"

namespace hamdb
{

    Value::Value(int32_t val) : value_(val) {}
    Value::Value(bool val) : value_(val) {}
    Value::Value(std::string val) : value_(std::move(val)) {}
    Value::Value(const char* val) : value_(std::string(val)) {}

    TypeId Value::getType() const
    {
        if (std::holds_alternative<int32_t>(value_))
            return TypeId::Integer;
        if (std::holds_alternative<bool>(value_))
            return TypeId::Boolean;
        if (std::holds_alternative<std::string>(value_))
            return TypeId::Varchar;
        return TypeId::Null;
    }

    bool Value::isNull() const
    {
        return getType() == TypeId::Null;
    }

    int32_t Value::getAsInteger() const
    {
        return std::get<int32_t>(value_);
    }

    bool Value::getAsBoolean() const
    {
        return std::get<bool>(value_);
    }

    const std::string& Value::getAsVarchar() const
    {
        return std::get<std::string>(value_);
    }

    Value Value::add(const Value& other) const
    {
        if (isNull() || other.isNull())
            return Value();
        if (getType() == TypeId::Integer && other.getType() == TypeId::Integer)
        {
            return Value(getAsInteger() + other.getAsInteger());
        }
        throw std::runtime_error("Unsupported type for addition");
    }

    Value Value::subtract(const Value& other) const
    {
        if (isNull() || other.isNull())
            return Value();
        if (getType() == TypeId::Integer && other.getType() == TypeId::Integer)
        {
            return Value(getAsInteger() - other.getAsInteger());
        }
        throw std::runtime_error("Unsupported type for subtraction");
    }

    Value Value::multiply(const Value& other) const
    {
        if (isNull() || other.isNull())
            return Value();
        if (getType() == TypeId::Integer && other.getType() == TypeId::Integer)
        {
            return Value(getAsInteger() * other.getAsInteger());
        }
        throw std::runtime_error("Unsupported type for multiplication");
    }

    Value Value::divide(const Value& other) const
    {
        if (isNull() || other.isNull())
            return Value();
        if (getType() == TypeId::Integer && other.getType() == TypeId::Integer)
        {
            int32_t denom = other.getAsInteger();
            if (denom == 0)
                throw std::runtime_error("Division by zero");
            return Value(getAsInteger() / denom);
        }
        throw std::runtime_error("Unsupported type for division");
    }

    Value Value::compareEquals(const Value& other) const
    {
        if (isNull() || other.isNull())
            return Value();
        if (getType() != other.getType())
            return Value(false);
        return Value(value_ == other.value_);
    }

    Value Value::compareNotEquals(const Value& other) const
    {
        if (isNull() || other.isNull())
            return Value();
        if (getType() != other.getType())
            return Value(true);
        return Value(value_ != other.value_);
    }

    Value Value::compareLessThan(const Value& other) const
    {
        if (isNull() || other.isNull())
            return Value();
        if (getType() != other.getType())
            throw std::runtime_error("Type mismatch in comparison");
        return Value(value_ < other.value_);
    }

    Value Value::compareLessThanEquals(const Value& other) const
    {
        if (isNull() || other.isNull())
            return Value();
        if (getType() != other.getType())
            throw std::runtime_error("Type mismatch in comparison");
        return Value(value_ <= other.value_);
    }

    Value Value::compareGreaterThan(const Value& other) const
    {
        if (isNull() || other.isNull())
            return Value();
        if (getType() != other.getType())
            throw std::runtime_error("Type mismatch in comparison");
        return Value(value_ > other.value_);
    }

    Value Value::compareGreaterThanEquals(const Value& other) const
    {
        if (isNull() || other.isNull())
            return Value();
        if (getType() != other.getType())
            throw std::runtime_error("Type mismatch in comparison");
        return Value(value_ >= other.value_);
    }

} // namespace hamdb
