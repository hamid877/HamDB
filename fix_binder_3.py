import sys
import re

def rewrite(path, cb):
    with open(path, 'r') as f: c = f.read()
    new_c = cb(c)
    if new_c != c:
        with open(path, 'w') as f: f.write(new_c)
        print(f"Updated {path}")

# binder.hpp
def upd_binder_hpp(c):
    c = c.replace("""    struct Context {
        std::string table_name;
        std::string table_alias;
        const Schema* schema{nullptr};
    };""", """    struct TableContext {
        std::string table_name;
        std::string table_alias;
        const Schema* schema;
        size_t column_offset;
    };
    struct Context {
        std::vector<TableContext> tables;
        std::unique_ptr<Schema> combined_schema;
    };""")
    
    if "bindTableReference" not in c:
        c = c.replace("std::unique_ptr<BoundSelectStatement> bindSelect(const ast::SelectStatement& stmt);", "std::unique_ptr<BoundTableReference> bindTableReference(const ast::TableReference& ref, std::vector<Column>& combined_columns);\n    std::unique_ptr<BoundSelectStatement> bindSelect(const ast::SelectStatement& stmt);")
    return c

# binder.cpp
def upd_binder_cpp(c):
    # AST BaseTableReference has table_name_ and table_alias_
    # AST JoinTableReference has left_, right_, condition_
    
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
"""
    if "bindTableReference" not in c:
        c = c.replace("std::unique_ptr<BoundSelectStatement> Binder::bindSelect(const ast::SelectStatement& stmt) {", bind_table_ref + "\nstd::unique_ptr<BoundSelectStatement> Binder::bindSelect(const ast::SelectStatement& stmt) {")
    
    select_rep = """
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
    }
"""
    c = re.sub(r'auto bound = std::make_unique<BoundSelectStatement>\(\);[\s\S]*?\} else \{\n            bound->select_list_\.push_back\(bindExpression\(\*expr\)\);\n        \}\n    \}', select_rep, c)
    
    bind_col = """
std::unique_ptr<BoundExpression> Binder::bindColumnValue(const ast::ColumnValueExpression& expr) {
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
}
"""
    c = re.sub(r'std::unique_ptr<BoundExpression> Binder::bindColumnValue\(const ast::ColumnValueExpression& expr\) \{[\s\S]*?return std::make_unique<BoundColumnRef>\(std::move\(executor_expr\), type, current_context_\.table_name, expr\.column_name\);\n\}', bind_col, c)

    update_ctx = """
    current_context_.tables.clear();
    current_context_.tables.push_back({stmt.table_name, "", &table_info->getSchema(), 0});
    std::vector<Column> cols;
    for (uint32_t i = 0; i < table_info->getSchema().getColumnCount(); ++i) {
        cols.push_back(table_info->getSchema().getColumn(i));
    }
    current_context_.combined_schema = std::make_unique<Schema>(cols);
"""
    c = c.replace('current_context_.table_name = stmt.table_name;\n    current_context_.table_alias = "";\n    current_context_.schema = &table_info->getSchema();', update_ctx)
    return c

rewrite("include/binder/binder.hpp", upd_binder_hpp)
rewrite("src/binder/binder.cpp", upd_binder_cpp)
