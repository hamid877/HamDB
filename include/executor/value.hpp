#pragma once

#include <cstdint>
#include <stdexcept>
#include <string>
#include <variant>

namespace hamdb
{

    enum class TypeId
    {
        Null,
        Integer,
        Boolean,
        Varchar
    };

    class Value
    {
    public:
        Value() = default;
        explicit Value(int32_t val);
        explicit Value(bool val);
        explicit Value(std::string val);
        explicit Value(const char* val);

        [[nodiscard]] TypeId getType() const;
        [[nodiscard]] bool isNull() const;

        [[nodiscard]] int32_t getAsInteger() const;
        [[nodiscard]] bool getAsBoolean() const;
        [[nodiscard]] const std::string& getAsVarchar() const;

        Value add(const Value& other) const;
        Value subtract(const Value& other) const;
        Value multiply(const Value& other) const;
        Value divide(const Value& other) const;

        Value compareEquals(const Value& other) const;
        Value compareNotEquals(const Value& other) const;
        Value compareLessThan(const Value& other) const;
        Value compareLessThanEquals(const Value& other) const;
        Value compareGreaterThan(const Value& other) const;
        Value compareGreaterThanEquals(const Value& other) const;

    private:
        std::variant<std::monostate, int32_t, bool, std::string> value_;
    };

} // namespace hamdb
