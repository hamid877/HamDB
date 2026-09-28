#include <gtest/gtest.h>
#include "executor/parameter_expression.hpp"
#include "executor/value.hpp"
#include "storage/tuple.hpp"
#include "catalog/schema.hpp"
#include <vector>

using namespace hamdb;

TEST(ParameterBindingTest, BasicTest) {
    ParameterExpression param(0);

    Tuple t;
    Schema s(std::vector<Column>{});

    EXPECT_THROW({
        (void) param.evaluate(t, s);
    }, std::runtime_error);


    std::vector<Value> params;
    params.push_back(Value(42));

    param.bindParameters(params);

    Value val = param.evaluate(t, s);
    EXPECT_EQ(val.getType(), TypeId::Integer);
    EXPECT_EQ(val.getAsInteger(), 42);
}
