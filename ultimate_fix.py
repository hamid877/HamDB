import sys
import re

def rewrite(path, cb):
    with open(path, 'r') as f: c = f.read()
    new_c = cb(c)
    if new_c != c:
        with open(path, 'w') as f: f.write(new_c)
        print(f"Updated {path}")

def fix_binder_hpp(c):
    ctx_rep = """struct TableContext {
        std::string table_name;
        std::string table_alias;
        const Schema* schema;
        size_t column_offset;
    };
    struct Context {
        std::vector<TableContext> tables;
        std::unique_ptr<Schema> combined_schema;
    };"""
    c = re.sub(r'struct Context \{[\s\S]*?\};', ctx_rep, c)
    c = c.replace("std::unique_ptr<BoundSelectStatement> bindSelect(const ast::SelectStatement& stmt);", "std::unique_ptr<BoundTableReference> bindTableReference(const ast::TableReference& ref, std::vector<Column>& combined_columns);\n    std::unique_ptr<BoundSelectStatement> bindSelect(const ast::SelectStatement& stmt);")
    return c

def fix_binder_cpp(c):
    # This must replace bindSelect and bindColumnValue completely.
    # The original binder.cpp has exactly this structure.
    
    bind_table_ref = """
std::unique_ptr<BoundTableReference> Binder::bindTableReference(const ast::TableReference& ref, std::vector<Column>& combined_columns) {
    if (auto base = dynamic_cast<const ast::BaseTableReference*>(&ref)) {
        auto bound_base = std::make_unique<BoundBaseTableReference>();
        TableInfo* table_info = nullptr;
        if (catalog_->getTable(base->table_name_, table_info) != Status::Ok) {
            throw BinderError("Unknown table: " + base->table_name_);
        }
        bound_base->table_name_ = base->table_name_;
        bound_base->table_alias_ = base->table_alias_;
        bound_base->schema_ = &table_info->getSchema();
        
        TableContext tc;
        tc.table_name = base->table_name_;
        tc.table_alias = base->table_alias_;
        tc.schema = bound_base->schema_;
        tc.column_offset = combined_columns.size();
        current_context_.tables.push_back(tc);
        
        for (uint32_t i = 0; i < tc.schema->getColumnCount(); ++i) {
            combined_columns.push_back(tc.schema->getColumn(i));
        }
        return bound_base;
    } else if (auto join = dynamic_cast<const ast::JoinTableReference*>(&ref)) {
        auto bound_join = std::make_unique<BoundJoinTable>();
        bound_join->left_ = bindTableReference(*join->left_, combined_columns);
        bound_join->right_ = bindTableReference(*join->right_, combined_columns);
        
        current_context_.combined_schema = std::make_unique<Schema>(combined_columns);
        if (join->condition_) {
            bound_join->condition_ = bindExpression(*join->condition_);
            if (bound_join->condition_->getType() != TypeId::Boolean) {
                throw BinderError("JOIN condition must be of boolean type");
            }
        }
        return bound_join;
    }
    throw BinderError("Unknown TableReference type");
}

std::unique_ptr<BoundSelectStatement> Binder::bindSelect(const ast::SelectStatement& stmt) {
    auto bound = std::make_unique<BoundSelectStatement>();
    current_context_.tables.clear();
    
    if (stmt.table) {
        std::vector<Column> combined_columns;
        bound->table_ = bindTableReference(*stmt.table, combined_columns);
        current_context_.combined_schema = std::make_unique<Schema>(combined_columns);
    } else {
        current_context_.combined_schema = nullptr;
    }

    for (const auto& expr : stmt.select_list) {
        if (dynamic_cast<ast::StarExpression*>(expr.get())) {
            if (!current_context_.combined_schema) {
                throw BinderError("SELECT * with no table specified");
            }
            const auto& cols = current_context_.combined_schema->getColumns();
            for (size_t i = 0; i < cols.size(); ++i) {
                TypeId type = columnTypeToTypeId(cols[i].getType());
                auto col_val = std::make_unique<hamdb::ColumnValueExpression>(i);
                bound->select_list_.push_back(std::make_unique<BoundColumnRef>(std::move(col_val), type, "", cols[i].getName()));
            }
        } else {
            bound->select_list_.push_back(bindExpression(*expr));
        }
    }"""
    
    # replace bindSelect completely
    c = re.sub(r'std::unique_ptr<BoundSelectStatement> Binder::bindSelect.*?\}\n\n    if \(stmt\.where_clause\)', bind_table_ref + '\n\n    if (stmt.where_clause)', c, flags=re.DOTALL)
    
    # replace bindColumnValue completely
    bind_col = """std::unique_ptr<BoundExpression> Binder::bindColumnValue(const ast::ColumnValueExpression& expr) {
    if (!current_context_.combined_schema) {
        throw BinderError("Column reference without a table");
    }

    int found_idx = -1;
    std::string found_table_name;
    TypeId type;

    if (!expr.table_name.empty()) {
        bool table_found = false;
        for (const auto& tc : current_context_.tables) {
            if (tc.table_name == expr.table_name || tc.table_alias == expr.table_name) {
                table_found = true;
                const auto& cols = tc.schema->getColumns();
                for (size_t i = 0; i < cols.size(); ++i) {
                    if (cols[i].getName() == expr.column_name) {
                        if (found_idx != -1) {
                            throw BinderError("Ambiguous column: " + expr.column_name);
                        }
                        found_idx = tc.column_offset + i;
                        found_table_name = tc.table_name;
                        type = columnTypeToTypeId(cols[i].getType());
                    }
                }
            }
        }
        if (!table_found) {
            throw BinderError("Unknown table or alias: " + expr.table_name);
        }
    } else {
        const auto& cols = current_context_.combined_schema->getColumns();
        for (size_t i = 0; i < cols.size(); ++i) {
            if (cols[i].getName() == expr.column_name) {
                if (found_idx != -1) {
                    throw BinderError("Ambiguous column: " + expr.column_name);
                }
                found_idx = i;
                type = columnTypeToTypeId(cols[i].getType());
            }
        }
    }

    if (found_idx == -1) {
        throw BinderError("Unknown column: " + expr.column_name);
    }

    auto executor_expr = std::make_unique<hamdb::ColumnValueExpression>(found_idx);
    return std::make_unique<BoundColumnRef>(std::move(executor_expr), type, found_table_name, expr.column_name);
}"""
    c = re.sub(r'std::unique_ptr<BoundExpression> Binder::bindColumnValue.*?\}\n', bind_col + '\n', c, flags=re.DOTALL)
    
    update_ctx = """    current_context_.tables.clear();
    current_context_.tables.push_back({stmt.table_name_, "", &table_info->getSchema(), 0});
    std::vector<Column> cols;
    for (uint32_t i = 0; i < table_info->getSchema().getColumnCount(); ++i) {
        cols.push_back(table_info->getSchema().getColumn(i));
    }
    current_context_.combined_schema = std::make_unique<Schema>(cols);"""
    
    # Fix bindUpdate and bindDelete context assignments
    c = re.sub(r'current_context_\.table_name = stmt\.table_name;.*?current_context_\.schema = &table_info->getSchema\(\);', update_ctx, c, flags=re.DOTALL)
    
    # Fix the remaining `stmt.table_name` which was renamed to `stmt.table_name_` in AST for Insert/Update/Delete!
    # Wait, did we rename `table_name` to `table_name_` in AST? YES, in fix_stmt.py, we renamed it in all Statements!
    c = re.sub(r'stmt\.table_name(?!_)', 'stmt.table_name_', c)
    return c

def fix_planner_tests(c):
    c = c.replace('std::filesystem::unique_path("test_planner_%%%%.hamdb")', '("test_planner_" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count()) + ".hamdb")')
    c = c.replace('std::filesystem::unique_path("test_physical_planner_%%%%.hamdb")', '("test_physical_planner_" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count()) + ".hamdb")')
    
    # We replace `stmt->table_name_ = "test_table";` for SELECT statements ONLY!
    # A SelectStatement is created with std::make_unique<hamdb::binder::BoundSelectStatement>()
    c = re.sub(r'(auto stmt = std::make_unique<hamdb::binder::BoundSelectStatement>\(\);.*?)(stmt->table_name_ = "test_table";)', 
               r'\1auto base = std::make_unique<hamdb::binder::BoundBaseTableReference>();\n    base->table_name_ = "test_table";\n    stmt->table_ = std::move(base);', 
               c, flags=re.DOTALL)
    return c

rewrite("include/binder/binder.hpp", fix_binder_hpp)
rewrite("src/binder/binder.cpp", fix_binder_cpp)
rewrite("tests/planner/planner_test.cpp", fix_planner_tests)
rewrite("tests/planner/physical_planner_test.cpp", fix_planner_tests)
