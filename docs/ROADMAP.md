# HamDB Roadmap

> Living development roadmap for HamDB.
>
> This document is updated after every completed milestone.

---

# Project Information

| Field            | Value                       |
| ---------------- | --------------------------- |
| Project          | HamDB                       |
| Language         | C++20                       |
| Build            | CMake + Ninja               |
| Testing          | GoogleTest                  |
| Platform         | Linux (Ubuntu / Linux Mint) |
| Current Version  | v0.3.0-dev                  |
| Overall Progress | **66%**                     |

---

# Development Principles

Every milestone must satisfy:

* [ ] `make build` passes.
* [ ] `make lint` passes with no project warnings.
* [ ] `make test` passes.
* [ ] Existing tests continue passing.
* [ ] Documentation updated.
* [ ] One Git commit per milestone.

---

# Milestone Progress

## Phase 1 — Storage Engine Foundations

| ID   | Milestone                    | Status     | Tests   |
| ---- | ---------------------------- | ---------- | ------- |
| M1.1 | Project Architecture         | ✅ Complete | 19      |
| M1.2 | Page & PageHeader            | ✅ Complete | 30      |
| M1.3 | Database Metadata Page       | ✅ Complete | 44      |
| M1.4 | Serializer / Deserializer    | ✅ Complete | 67      |
| M1.5 | Slotted Pages                | ✅ Complete | 96      |
| M1.6 | Table Heap + RID + Iterator  | ✅ Complete | 125     |
| M1.7 | Buffer Pool Manager Skeleton | ✅ Complete | 131     |
| M1.8 | LRU-K Replacement Policy     | ✅ Complete | **135** |
| M1.9 | Page Guards (RAII)           | ✅ Complete | **161** |

---

## Phase 2 — Index Engine

| ID   | Milestone          | Status |
| ---- | ------------------ | ------ |
| M2.0 | B+ Tree Page Infrastructure | ✅ Complete |
| M2.1 | Leaf Pages                  | ✅ Complete |
| M2.2 | Internal Pages              | ✅ Complete |
| M2.3 | Search Algorithm            | ✅ Complete |
| M2.4 | B+ Tree Leaf Insert         | ✅ Complete |
| M2.5 | B+ Tree Splits              | ✅ Complete |
| M2.6 | Recursive B+ Tree Insertion | ✅ Complete |
| M2.7 | B+ Tree Iterator & Range Scan | ✅ Complete |
| M2.8 | Delete & Merge              | ✅ Complete |

---

## Phase 3 — Catalog

| ID   | Milestone                  | Status |
| ---- | -------------------------- | ------ |
| M3.0 | Transaction Manager Skeleton | ✅ Complete |
| M3.1 | Lock Manager               | ✅ Complete |
| M3.2 | MVCC (Snapshot Isolation)  | ✅ Complete |
| M3.3 | Write-Ahead Logging (WAL)  | ✅ Complete |
| M3.4 | Crash Recovery             | ✅ Complete |

---

## Phase 4 — SQL Engine

| ID   | Milestone  | Status |
| ---- | ---------- | ------ |
| M4.0 | Catalog Manager | ✅ Complete |
| M4.1 | Expression System | ✅ Complete |
| M4.2 | Sequential Scan Executor | ✅ Complete |
| M4.3 | Index Scan Executor | ✅ Complete |
| M4.4 | Insert & Delete Executor | ✅ Complete |
| M4.5 | Update Executor | ✅ Complete |
| M4.6 | Filter Executor | ✅ Complete |
| M4.7 | Projection Executor | ✅ Complete |
| M8.1 | Nested Loop Join Executor | ✅ Complete |
| M8.2 | Hash Join Executor | ✅ Complete |
| M8.3 | Aggregation Executor | ✅ Complete |
| M8.4 | Having Executor | ✅ Complete |
| M8.5 | ORDER BY Executor | ✅ Complete |
| M8.6 | LIMIT/OFFSET Executor | ✅ Complete |
| M4.10 | Sort Executor | ✅ Complete |
| M4.11 | Limit Executor | ✅ Complete |
| M4.12 | Values Executor | ✅ Complete |
| M4.13 | SQL Parser | ⬜      |
| M4.14 | AST        | ⬜      |
| M4.15 | Planner    | ✅ Complete |
| M4.16 | Executor   | ⬜      |

---

## Phase 5 — Transactions & Recovery

| ID   | Milestone    | Status |
| ---- | ------------ | ------ |
| M5.0 | WAL Records  | ⬜      |
| M5.1 | Log Manager  | ⬜      |
| M5.2 | Recovery     | ⬜      |
| M5.3 | MVCC         | ⬜      |
| M5.4 | Lock Manager | ⬜      |

---

## Phase 6 — Query Optimizer

| ID   | Milestone    | Status |
| ---- | ------------ | ------ |
| M6.1 | Predicate Pushdown | ✅ Complete |
| M6.2 | Projection Pruning | ✅ Complete |
| M6.3 | Constant Folding   | ✅ Complete |
| M6.4 | Index Scan Selection Optimizer | ✅ Complete |
| M6.5 | Sort Elimination & Limit Pushdown | ✅ Complete |

---

## Phase 7 — Client Interfaces & Tooling

| ID   | Milestone    | Status |
| ---- | ------------ | ------ |
| M7.1 | Interactive SQL REPL | ✅ Complete |
| M7.2 | EXPLAIN & EXPLAIN ANALYZE | ✅ Complete |
| M7.3 | Prepared Statements & Parameter Binding | ✅ Complete |
| M7.4 | CLI Client   | ✅ Complete |
| M7.5 | CLI Utilities & Database Introspection | ✅ Complete |

---

## Phase 9 — Advanced Query Optimizer

| ID   | Milestone    | Status |
| ---- | ------------ | ------ |
| M9.1 | Optimizer Infrastructure | ✅ Complete |
| M9.2 | Filter/Predicate Pushdown | ✅ Complete |
| M9.3 | Projection Pruning | ✅ Complete |
| M9.4 | Constant Folding (Advanced) | ✅ Complete |

---

# Completed Milestones

## M9.4 — Constant Folding (Advanced)

**Status:** ✅ Complete

### Implemented

* Rewrote `ConstantFoldingRule` to be recursively evaluating, semantic-preserving, and exception-safe.
* Wrapped `evaluate()` calls in a `try-catch` block to gracefully handle runtime errors (e.g., division by zero) by folding to `NULL` rather than crashing the optimizer.
* Fixed SQL 3-valued boolean logic for `AND`/`OR` to properly short-circuit with `NULL` constants (e.g., `false AND NULL` evaluates to `false`).
* Ensured non-constant subexpressions are preserved and unaffected unless a valid fold can occur.
* Added comprehensive query-level semantic equivalence tests and expression-level unit tests covering arithmetic, comparison, logic, exception safety, and NULL propagation.
* Integrated without creating any new expression abstractions.

### Verification

* Tests passing: **361 / 361** (CTest)
* Build: ✅
* Lint: ✅
* Test: ✅ (10/10 deterministic test loops)

### Git Commit

```text
feat(optimizer): implement advanced constant folding rule (M9.4)
```

---

## M9.3 — Projection Pruning

**Status:** ✅ Complete

### Implemented

* Implemented semantic-preserving projection pruning by determining which columns are required by the final projection and all ancestor operators, and propagating those requirements toward child plans.
* Correctly retain columns required by filters, join predicates, GROUP BY, aggregate expressions, HAVING, ORDER BY, and computed expressions.
* Propagated requirements down through `HAVING`, `AGGREGATION`, and `JOIN` (`NESTED_LOOP_JOIN`, `HASH_JOIN`) nodes.
* Added `JoinPruningTest` to verify that required columns are properly propagated to both sides of a join in `ProjectionPruningRule`.

### Verification

* Tests passing: **361 / 361** (CTest)
* Build: ✅
* Lint: ✅
* Test: ✅

### Git Commit

```text
feat(optimizer): implement join/aggregation projection pruning rule (M9.3)
```

---

## M9.2 — Filter/Predicate Pushdown

**Status:** ✅ Complete

### Implemented

* Implemented semantic-preserving filter pushdown rule into join children (Left and Right sides).
* Recursively push filters through SeqScan and Hash/NestedLoop Joins.
* Implemented splitting of AND conjuncts, dynamically shifting column dependencies when pushing to right join children.
* Preserved all existing expression and plan node semantics without inventing new abstractions.
* Added `PushdownFilterToJoinLeft`, `PushdownFilterToJoinRight`, and `ExecutionResultsIdenticalJoin` tests in `tests/optimizer/predicate_pushdown_test.cpp`.

### Verification

* Tests passing: **361 / 361** (CTest)
* Build: ✅
* Lint: ✅
* Test: ✅

### Git Commit

```text
feat(optimizer): implement join filter pushdown rule (M9.2)
```

---

## M9.1 — Optimizer Infrastructure

**Status:** ✅ Complete

### Implemented

* Created `IOptimizer` interface in `include/optimizer/optimizer.hpp` to decouple calling code from rule-application mechanisms.
* Implemented `HamDBOptimizer` in `src/optimizer/optimizer.cpp`, wrapping `RuleExecutor` and encapsulating standard rule registrations.
* Updated `include/shell/shell.hpp` and `src/shell/shell.cpp` to use `IOptimizer` instead of depending on `RuleExecutor` and individual rule headers.
* Removed manual rule registration from `Shell`, moving responsibility to `HamDBOptimizer`.
* Built comprehensive, standalone tests for the optimizer in `tests/optimizer/optimizer_test.cpp`.
* Verified CI hardening standards: no global mutable state, deterministic tests, respectful of `[[nodiscard]]`, and Clang 18 + -Werror clean.

### Verification

* Build: ✅
* Lint: ✅
* Test: ✅ (361 CTest suites, 10/10 deterministic runs)
* CI Hardening: ✅

### Git Commit

```text
feat(optimizer): implement optimizer infrastructure (M9.1)
```

---



## M8.6 — LIMIT/OFFSET Executor

**Status:** ✅ Complete

### Implemented

* Reused existing `LimitExecutor` (M4.11) — fully wired into `LogicalPlanType::LIMIT` / `PhysicalPlanType::LIMIT` / `ExecutorFactory`.
* `init()` calls child `init()` then skips `OFFSET` tuples using the child iterator.
* `next()` returns at most `LIMIT` tuples; returns `false` once the budget is exhausted.
* `LIMIT 0` returns EOF immediately; `OFFSET` beyond child size returns EOF.
* Child tuple order is always preserved (no reordering inside `LimitExecutor`).
* Added `tests/planner/limit_plan_test.cpp` — 7 structural tests for `LimitPlanNode` and `LimitPlan`.
* Added `tests/executor/limit_integration_test.cpp` — 14 integration tests verifying LIMIT/OFFSET after:
  * `OrderByExecutor` (ascending, descending, with offset)
  * Mock aggregation output via `ValuesExecutor`
  * `HavingExecutor` (LIMIT 0 case included)
  * `NestedLoopJoinExecutor` (cross join, offset-beyond-size EOF, order preservation)
  * Re-init safety (double `init()` resets state correctly)
  * Output schema passthrough
* Both new test executables registered as CTest targets (`LimitPlanTest`, added to `ExecutorTest`).
* CI hardening: unique temp DB paths, no shared global state, `[[nodiscard]]` respected, `-Werror` clean.

### Verification

* Build: ✅
* Lint: ✅
* Test: ✅ (360 CTest suites, 10/10 deterministic runs)
* CI Hardening: ✅

### Git Commit

```text
feat(executor): implement LIMIT/OFFSET executor (M8.6)
```

---

## M8.5 — ORDER BY Executor

**Status:** ✅ Complete

### Implemented

* `OrderByExecutor` implementing the executor lifecycle (`init()`, `next()`, `outputSchema()`).
* Support for one or multiple sort keys with `ASC`/`DESC` directions.
* Materialize child tuples into an in-memory vector.
* Evaluate ORDER BY expressions and stable-sort tuples lexicographically.
* Integration with the planner, physical planner, and executor factory.
* Validation rules for sort parameters in binder and optimizer.
* Added `order_by_plan_test` and `order_by_executor_test`.

### Verification

* Build: ✅
* Lint: ✅
* Test: ✅
* CI Hardening: ✅

### Git Commit

```text
feat(executor): implement ORDER BY executor (M8.5)
```

## M8.4 — Having Executor

**Status:** ✅ Complete

### Implemented

* `HavingExecutor` implementing the executor lifecycle (`init()`, `next()`, `outputSchema()`).
* Consume aggregated tuples from `AggregationExecutor` (or any child).
* Evaluate HAVING predicate dynamically using the `Expression` system on emitted tuples.
* Included physical and logical planner changes with `HavingPlan` and `LogicalHavingNode`.
* Validation rules in `Binder` strictly permitting only `GROUP BY` column references and aggregate functions.
* Added `having_plan_test` and `having_executor_test`.

### Verification

* Build: ✅
* Lint: ✅
* Test: ✅

### Git Commit

```text
feat(executor): implement having executor (M8.4)
```

## M8.3 — Aggregation Executor

**Status:** ✅ Complete

### Implemented

* `AggregationExecutor` implementing the executor lifecycle (`init()`, `next()`, `outputSchema()`).
* Support for `GROUP BY` using `AggregateKey` and `AggregateValue`.
* Support for hash aggregation functions `COUNT(*)`, `COUNT(col)`, `SUM(col)`, `AVG(col)`, `MIN(col)`, and `MAX(col)`.
* Unordered map for aggregate hash table state built during `init()`.
* Iterating through the hash table and emitting one tuple per group in `next()`.
* Planner and Binder updates for `LogicalAggregationNode` and `AggregationPlan`.

### Verification

* Build: ✅
* Lint: ✅
* Test: ✅

### Git Commit

```text
feat(executor): implement aggregation executor (M8.3)
```

## M8.2 — Hash Join Executor

**Status:** ✅ Complete

### Implemented

* `HashJoinExecutor` implementing the executor lifecycle (`init()`, `next()`, `outputSchema()`).
* Hash table built from right child tuples during `init()`.
* Probe phase performed continuously while scanning the left child in `next()`.
* Support for duplicate join keys using vector buckets inside the hash table map.
* Logical and physical planner support for equality predicates, compiling directly into `HashJoinPlan`.
* Planners updated to compile non-equality predicates to `NestedLoopJoinPlan`.
* Properly preserved and evaluated expressions for hashing and comparison.
* Full integration into physical execution and optimizer rules.

### Verification

* Build: ✅
* Lint: ✅
* Test: ✅

### Git Commit

```text
feat(executor): implement hash join executor (M8.2)
```

## M7.5 — CLI Utilities & Database Introspection

**Status:** ✅ Complete

### Implemented

* Created `include/shell/meta_commands.hpp` and `src/shell/meta_commands.cpp`.
* Implemented meta commands `.help`, `.tables`, `.schema <table>`, `.describe <table>`, `.indexes <table>`, `.stats`, `.quit`, and `.exit`.
* Reused `TablePrinter` for reusable ASCII formatting.
* Gathered metadata from `CatalogManager`, `BufferPoolManager`, `DiskManager`, `TransactionManager`, and `PreparedStatementManager` for `.stats`.
* Integrated `MetaCommands` execution before SQL parsing in `Shell`.
* Updated `CMakeLists.txt` and `app/hamdb.cpp`.
* Wrote integration tests in `tests/shell/meta_command_test.cpp` verifying execution correctly using temporary database paths.

### Verification

* Build: ✅
* Lint: ✅
* Test: ✅

### Git Commit

```text
feat(shell): implement CLI utilities and database introspection (M7.5)
```

## M7.4 — CLI Client & SQL Script Execution

**Status:** ✅ Complete

### Implemented

* Created `StatementSplitter` to separate SQL scripts into distinct statements correctly supporting comments and strings.
* Created `ScriptExecutor` to run `.sql` script files.
* Updated `hamdb` executable to accept `.sql` scripts as command-line arguments.
* Implemented `.read` meta command in the REPL.
* Re-used existing physical planning and execution pipeline.
* Stops on first error and prints summary of statement execution count.

### Verification

* Build: ✅
* Lint: ✅
* Test: ✅

### Git Commit

```text
feat(shell): implement sql script execution and cli client (M7.4)
```

## M7.3 — Prepared Statements & Parameter Binding

**Status:** ✅ Complete

### Implemented

* Implemented `PREPARE`, `EXECUTE`, and `DEALLOCATE` statements in parser.
* Updated `Binder` to infer parameter types and bind parameter values.
* Created `PreparedStatementManager` to store optimized physical plans.
* Updated `Expression` hierarchy to support `clone()` and `bindParameters()`.
* Integrated prepared statement cache and execution flow into `Shell`.
* Implemented parameter substitution at execution time to skip planning/optimization.

### Verification

* Build: ✅
* Lint: ✅
* Test: ✅

### Git Commit

```text
feat(prepared): implement prepared statements and parameter binding (M7.3)
```

## M7.2 — EXPLAIN & EXPLAIN ANALYZE

**Status:** ✅ Complete

### Implemented

* Created `include/planner/explain_plan.hpp` and `include/planner/plan_formatter.hpp`.
* Created `include/executor/execution_stats.hpp` to track `rows_in`, `rows_out`, and `execution_time`.
* Updated `ExecutorFactory` to conditionally wrap executors in `AnalyzeExecutor` to collect execution statistics during `EXPLAIN ANALYZE`.
* Updated `RuleExecutor` to track applied rules.
* Updated `Lexer` and `Parser` to support `EXPLAIN` and `ANALYZE` keywords and `ExplainStatement`.
* Modified `Shell::executeSQL` to format logical plan, optimized logical plan, physical plan, output schema, and optimizer rule trace using Unicode tree characters.
* Preserved normal query execution semantics.

### Verification

* Tests passing: All
* Build: ✅
* Lint: ✅
* Test: ✅

### Git Commit

```text
feat(planner): implement EXPLAIN and EXPLAIN ANALYZE (M7.2)
```

## M7.1 — Interactive SQL REPL

**Status:** ✅ Complete

### Implemented

* Created `include/shell/` and `src/shell/` for `Repl`, `Shell`, and `TablePrinter`.
* Added ASCII `TablePrinter` for formatting SQL output with `(N rows)` counters and `NULL` handling.
* Integrated `Shell` with HamDB's optimizer and executor pipelines (`executeSQL`).
* Added meta commands (`.help`, `.exit`, `.quit`, `.tables`, `.schema <table>`, `.indexes <table>`).
* Support for multiline SQL statements buffered until `;` is typed (`hamdb>` and `....>` prompts).
* Added comprehensive integration tests (`tests/shell/shell_test.cpp`, `tests/shell/table_printer_test.cpp`).
* Added `app/hamdb.cpp` building to executable `build/app/hamdb`.

### Verification

* Tests passing: **349 / 349** (CTest)
* Build: ✅
* Lint: ✅
* Test: ✅

### Git Commit

```text
feat(shell): implement interactive SQL REPL and table printer (M7.1)
```

## M6.5 — Sort Elimination & Limit Pushdown

**Status:** ✅ Complete

### Implemented

* Created `include/optimizer/sort_limit_rule.hpp` and `src/optimizer/sort_limit_rule.cpp`.
* Implemented `SortLimitRule` to eliminate `Sort` node if `IndexScan` guarantees identical ordering.
* Implemented `Limit` pushdown through `Projection` and `Filter` nodes into `SeqScan` and `IndexScan`.
* Updated `LogicalPlanNode`, `SeqScanPlanNode`, and `LogicalIndexScanNode` to support ordering and limit metadata.
* Updated `SeqScanExecutor` and `IndexScanExecutor` to accept `limit` and `offset` and stop after emitting N tuples.
* Implemented corresponding physical plan nodes and updated `PhysicalPlanner` and `ExecutorFactory`.
* Wrote integration tests in `tests/optimizer/sort_limit_rule_test.cpp`.

### Verification

* Tests passing: **348 / 348**
* Build: ✅
* Lint: ✅
* Test: ✅

### Git Commit

```text
feat(optimizer): implement sort elimination and limit pushdown (M6.5)
```

---

## M6.4 — Index Scan Selection Optimizer

**Status:** ✅ Complete

### Implemented

* Created `LogicalIndexScanNode` and `IndexScanRule`.
* `IndexScanRule` rewrites `SeqScanPlanNode` with an index-eligible predicate into a `LogicalIndexScanNode`.
* Extended `PhysicalPlanner` to support translating `LogicalIndexScanNode` to `IndexScanPlan`.
* Extended `ExecutorFactory` to instantiate `IndexScanExecutor`.
* Updated `IndexScanExecutor` to accept an `Expression` predicate.
* `IndexScanExecutor` iterates `BPlusTree` via `BPlusTreeIterator`, looking up the RID in `TableHeap` and applying the predicate.
* Handled fallback to `SeqScan` for unsupported queries.
* Included fallback rules in `ConstantFoldingRule` and `ProjectionPruningRule`.

### Verification

* Build: ✅
* Lint: ✅
* Test: ✅

### Git Commit

```text
feat(optimizer): implement index scan selection optimizer (M6.4)
```

---

## M6.3 — Constant Folding Optimizer

**Status:** ✅ Complete

### Implemented

* Implemented `ConstantFoldingRule`.
* Recursively traverses and folds constant subtrees in `Expression` trees.
* Supports folding Arithmetic (`+`, `-`, `*`, `/`, `%`, unary minus), Comparison, and Boolean (`AND`, `OR`, `NOT`) expressions.
* Handles boolean identities (`expr AND true → expr`, `expr OR false → expr`, etc.).
* Modifies expressions in place for `SeqScan`, `Filter`, `Projection`, `Sort`, `Limit`, `Values`, and `Update` nodes.
* Added support for `Modulo` to the expression system and SQL binder.
* Preserves output schemas and semantics.

### Verification

* Tests passing: **346 / 346** (CTest)
* Build: ✅
* Lint: ✅
* Test: ✅

### Git Commit

```text
feat(optimizer): implement constant folding rule (M6.3)
```

---

## M6.2 — Projection Pruning Optimizer

**Status:** ✅ Complete

### Implemented

* Implemented `ProjectionPruningRule`.
* Recursively traverses logical plan top-down to collect required columns.
* Implemented recursive expression column collector.
* Preserves schemas for all operators except `SeqScanPlanNode`, which rewrites output schema.
* Projection, Filter, Sort, Limit contribute their respective required columns to the child.
* Disabled pruning for wildcard SELECT * and modifications (INSERT/UPDATE/DELETE/VALUES).
* Added unit test to verify correctly rewritten logical plans, output schemas, and preserved optimized execution.

### Verification

* Build: ✅
* Lint: ✅
* Test: ✅

### Git Commit

```text
feat(optimizer): implement projection pruning rule (M6.2)
```

---

## M6.1 — Predicate Pushdown Optimizer

**Status:** ✅ Complete

### Implemented

* Created optimizer directory structure (`include/optimizer`, `src/optimizer`, `tests/optimizer`).
* Defined `Rule` base class for logical plan rewriting.
* Implemented `RuleExecutor` for bottom-up rule application across the logical plan tree.
* Implemented `PredicatePushdownRule` which pushes `FilterPlanNode` predicates into underlying `SeqScanPlanNode`s and removes the filter if eligible.
* Updated `SeqScanPlanNode` and `SeqScanExecutor` to accept and evaluate predicates dynamically during scans.
* Full integration test demonstrating identical tuple output between optimized and unoptimized plans while validating structural plan differences.

### Verification

* Tests passing: **344 / 344** (CTest)
* Build: ✅
* Lint: ✅
* Test: ✅

### Git Commit

```text
feat(optimizer): implement rule-based predicate pushdown (M6.1)
```

---

## M5.5 — Physical Planner & Executor Factory

**Status:** ✅ Complete

### Implemented

* Defined `PhysicalPlanNode` tree representing executor physical operations (`SeqScan`, `Filter`, `Projection`, `Sort`, `Limit`, `Values`, `Insert`, `Update`, `Delete`).
* Implemented `PhysicalPlanner` to translate `LogicalPlanNode` to `PhysicalPlanNode`.
* Implemented `ExecutorFactory` to construct the execution pipeline recursively.
* Passes `ExecutorContext` through the tree to all executors.
* Full integration tests running end-to-end SQL query pipelines.

### Verification

* Tests passing: **343 / 343** (CTest)
* Build: ✅
* Lint: ✅
* Test: ✅

### Git Commit

```text
feat(planner): implement physical planner and executor factory (M5.5)
```

---

## M5.4 — Logical Query Planner

**Status:** ✅ Complete

### Implemented

* Defined `LogicalPlanNode` tree representing relational algebra operations (`SeqScan`, `Filter`, `Projection`, `Sort`, `Limit`, `Values`, `Insert`, `Update`, `Delete`).
* Implemented `Planner` to convert `BoundStatement` AST nodes into logical plan trees.
* Handled target lists, predicates, ordering, limits, and set clauses recursively.
* Automatically resolved and derived `Schema` outputs for every logical plan operator.
* Unit tests validating tree construction (e.g., `LIMIT -> PROJECTION -> FILTER -> SEQ_SCAN`) and proper values wrapping.

### Verification

* Tests passing: **341 / 341** (CTest)
* Build: ✅
* Lint: ✅
* Test: ✅

### Git Commit

```text
feat(planner): implement logical query planner (M5.4)
```

---

## M5.3 — Binder (Semantic Analysis)

**Status:** ✅ Complete

### Implemented

* `include/binder/*` and `src/binder/*` defining `BoundExpression` and `BoundStatement` hierarchies.
* `Binder` class that recursively binds `ast::Statement` nodes to bound equivalents.
* Resolution of table names using `CatalogManager`.
* Resolution of column names, including schema lookup and alias support.
* Support for `SELECT *` expansion into all table columns.
* Strict type checking (unknown/ambiguous columns, missing tables, mismatched expressions).
* Binder unit tests covering `SELECT`, `UPDATE`, `INSERT`, aliases, and error conditions.

### Verification

* Tests passing: **346 / 346** (CTest)
* Build: ✅
* Lint: ✅
* Test: ✅

### Git Commit

```text
feat(binder): implement semantic binder (M5.3)
```

---

## M5.2 — Recursive Descent SQL Parser

**Status:** ✅ Complete

### Implemented

* AST node definitions (`ASTNode`, `Expression`, `Statement` hierarchies).
* Expression parsing with precedence (Parentheses, Unary, `*`/`/`/`%`, `+`/`-`, Comparison, `AND`, `OR`).
* Statement parsing (`SELECT`, `INSERT`, `UPDATE`, `DELETE`, `VALUES`).
* `Parser` class with recursive descent strategy over lexer tokens.
* Graceful error handling using `ParserError`.

### Verification

* Tests passing: **339 / 339** (CTest)
* Build: ✅
* Lint: ✅
* Test: ✅

### Git Commit

```text
feat(parser): implement recursive descent SQL parser (M5.2)
```

---
## M5.1 — SQL Lexer / Tokenizer

**Status:** ✅ Complete

### Implemented

* `Token` structures for types, lexemes, lines, and columns.
* `Lexer` implementation to tokenize SQL strings.
* Case-insensitive keyword parsing, identifiers, literals, operators, and delimiters.
* Error handling for invalid characters and unterminated strings.
* Single-line (`--`) and multi-line (`/* */`) comment skipping.

### Verification

* Tests passing: **339 / 339** (CTest)
* Build: ✅
* Lint: ✅
* Test: ✅

### Git Commit

```text
feat(parser): implement SQL lexer / tokenizer (M5.1)
```

---

## M4.12 — Values Executor

**Status:** ✅ Complete

### Implemented

* `ValuesExecutor` implementing the executor lifecycle (`init()`, `next()`, `outputSchema()`).
* Production of tuples from constant expressions without a child executor.
* Emitting tuples sequentially.
* Evaluation of values using the `Expression` system dynamically.
* Dynamically build output schema.

### Verification

* Tests passing: **338 / 338** (CTest)
* Build: ✅
* Lint: ✅
* Test: ✅

### Git Commit

```text
feat(executor): implement values executor (M4.12)
```

---

## M4.11 — Limit Executor

**Status:** ✅ Complete

### Implemented

* `LimitExecutor` implementing the executor lifecycle (`init()`, `next()`, `outputSchema()`).
* Wrapping of any child executor.
* Emitting at most `LIMIT` tuples and skipping `OFFSET` tuples.
* Returns `false` immediately in `next()` once `LIMIT` tuples have been emitted.
* Preserves `Tuple` and `RID` propagation from the child tuple.
* Uses O(1) extra memory by simply keeping track of the tuple count.
* Correctly handles limits and offsets of 0.

### Verification

* Tests passing: **343 / 343** (CTest)
* Build: ✅
* Lint: ✅
* Test: ✅

### Git Commit

```text
feat(executor): implement limit executor (M4.11)
```

---

## M4.10 — Sort Executor

**Status:** ✅ Complete

### Implemented

* `SortExecutor` implementing the executor lifecycle (`init()`, `next()`, `outputSchema()`).
* Wrapping of any child executor.
* `OrderByType` enum with `ASC` and `DESC`.
* Materialization of all tuples from the child executor inside `init()`.
* In-memory sorting using `std::sort` based on multiple target keys.
* Independent sort direction support (`ASC` and `DESC`).
* Reusing the `Expression` system (e.g., `ColumnValueExpression`) to evaluate values dynamically.
* Emitting tuples sequentially after sorting via `next()`.
* Preservation of `RID` propagation from the child tuple.

### Verification

* Tests passing: **338 / 338** (CTest)
* Build: ✅
* Lint: ✅
* Test: ✅

### Git Commit

```text
feat(executor): implement sort executor (M4.10)
```

---

## M8.1 — Nested Loop Join Executor

**Status:** ✅ Complete

### Implemented

* `NestedLoopJoinExecutor` implementing the executor lifecycle (`init()`, `next()`, `outputSchema()`).
* Wrap two child executors (left and right).
* Iterate left tuples, restarting the right executor for each left tuple.
* Evaluate join predicate using the `Expression` system, passing both left and right tuples via `evaluateJoin()`.
* Concatenate left and right tuple data to produce combined output tuples.
* Dynamically build combined output schema.
* Preserve RID propagation from the left child.
* Updated `Expression` tree to support dual-tuple evaluation (`evaluateJoin()`), adding `TupleSource` to `ColumnValueExpression`.

### Verification

* Tests passing: **338 / 338** (CTest)
* Build: ✅
* Lint: ✅
* Test: ✅

### Git Commit

```text
feat(executor): implement nested loop join executor (M8.1)
```

---

## M4.7 — Projection Executor

**Status:** ✅ Complete

### Implemented

* `ProjectionExecutor` implementing the executor lifecycle (`init()`, `next()`, `outputSchema()`).
* Wrapping of any child executor.
* Evaluation of a list of target expressions for every input tuple using the `Expression` system.
* Construction of a new output `Tuple` based on the evaluated values.
* Preservation of `RID` propagation from the child tuple.

### Verification

* Tests passing: **338 / 338** (CTest)
* Build: ✅
* Lint: ✅
* Test: ✅

### Git Commit

```text
feat(executor): implement projection executor (M4.7)
```

---

## M4.6 — Filter Executor

**Status:** ✅ Complete

### Implemented

* `FilterExecutor` implementing the executor lifecycle (`init()`, `next()`, `outputSchema()`).
* Wrapping of any child executor.
* Evaluation of predicates using `Expression` system.
* Support for logical operators (`LogicalExpression`) handling `AND`, `OR`, `NOT`.
* Integration with `Value` type boolean logic.

### Verification

* Tests passing: **338 / 338** (CTest)
* Build: ✅
* Lint: ✅
* Test: ✅

### Git Commit

```text
feat(executor): implement filter executor (M4.6)
```

---

## M4.5 — Update Executor

**Status:** ✅ Complete

### Implemented

* `UpdateExecutor` implementing the executor lifecycle (`init()`, `next()`, `outputSchema()`).
* Consumer of child executor output (`Tuple`, `RID`).
* Exclusive tuple lock acquisition via `LockManager`.
* Evaluation of target expressions using the `Expression` system.
* Tombstoning the old MVCC version and inserting a new version.
* Primary B+ Tree index updates (`remove` old key and `insert` new key) if indexed column changed.
* WAL `LogRecordType::UPDATE` logging.
* Returns affected row count.

### Verification

* Tests passing: **338 / 338** (CTest)
* Build: ✅
* Lint: ✅
* Test: ✅

### Git Commit

```text
feat(executor): implement update executor (M4.5)
```

---

## M4.4 — Insert & Delete Executor

**Status:** ✅ Complete

### Implemented

* `InsertExecutor` and `DeleteExecutor` implementing the executor lifecycle (`init()`, `next()`, `outputSchema()`).
* Exclusive locking of RIDs using `LockManager`.
* Tombstoning and inserting tuples via `MvccManager`.
* WAL logging for `LogRecordType::INSERT` and `LogRecordType::DELETE`.
* Primary B+ Tree index updates (`insert` and `remove`).
* Both executors return a single tuple containing the number of affected rows.
* Updates to `ExecutorContext` to inject `LockManager` and `LogManager`.

### Verification

* Tests passing: **340 / 340**
* Build: ✅
* Lint: ✅
* Test: ✅

### Git Commit

```text
feat(executor): implement insert and delete executors (M4.4)
```

---

## M4.3 — Index Scan Executor

**Status:** ✅ Complete

### Implemented

* `IndexScanExecutor` implementing the executor lifecycle (`init()`, `next()`, `outputSchema()`).
* Equality lookups on `int64_t` keys using `BPlusTree::getValue()`.
* Reading tuples from `TableHeap` via RID.
* Applying MVCC visibility rules inside `next()` to return the correct tuple version or skip deleted ones.
* Only returns the matching tuple once, returning exhaustion on subsequent calls.

### Verification

* Tests passing: **338 / 338**
* Build: ✅
* Lint: ✅
* Test: ✅

### Git Commit

```text
feat(executor): implement index scan executor (M4.3)
```

---

## M4.2 — Sequential Scan Executor

**Status:** ✅ Complete

### Implemented

* `ExecutorContext` holding references to `CatalogManager`, `Transaction`, `MvccManager`, and `BufferPoolManager`.
* `AbstractExecutor` interface definition.
* `SeqScanExecutor` implementing the executor lifecycle (`init()`, `next()`, `outputSchema()`).
* Iteration over `TableHeap` using `HeapIterator`.
* MVCC visibility checks integrated inside `next()` to skip deleted or invisible tuple versions.

### Verification

* Tests passing: **338 / 338**
* Build: ✅
* Lint: ✅
* Test: ✅

### Git Commit

```text
feat(executor): implement sequential scan executor (M4.2)
```

---

## M4.1 — Expression System

**Status:** ✅ Complete

### Implemented

* `Value` class supporting `INTEGER`, `BOOLEAN`, `VARCHAR`, and `NULL`.
* Comparison operators (`=`, `!=`, `<`, `<=`, `>`, `>=`).
* Arithmetic operators (`+`, `-`, `*`, `/`).
* `Expression` base class with `evaluate` API.
* `ConstantExpression` returning fixed values.
* `ColumnValueExpression` extracting column values dynamically from `Tuple` based on `Schema`.
* `ComparisonExpression` and `ArithmeticExpression` allowing nested evaluation trees.
* Integrated gracefully with `Tuple` and `Schema` classes.

### Verification

* Tests passing: **338 / 338**
* Build: ✅
* Lint: ✅
* Test: ✅

### Git Commit

```text
feat(executor): implement expression system (M4.1)
```

---

## M4.0 — Catalog Manager

**Status:** ✅ Complete

### Implemented

* `Schema` with column name/type serialization.
* `TableInfo` storing `table_id`, `table_name`, `heap_root_page`, `index_root_page`, and `schema`.
* `CatalogManager` class handling `createTable`, `getTable`, `dropTable`, and `listTables`.
* Persistent catalog page stored in `.hamdb` (metadata page / page 0).
* Metadata survives database reopen operations.

### Verification

* Tests passing: **337 / 337**
* Build: ✅
* Lint: ✅
* Test: ✅

### Git Commit

```text
feat(catalog): implement catalog manager (M4.0)
```

---

## M3.4 — Crash Recovery

**Status:** ✅ Complete

### Implemented

* `RecoveryManager` class handling ARIES-like crash recovery.
* Analyzed WAL to reconstruct active transactions.
* Redo for committed and uncommitted mutation records (`INSERT`, `UPDATE`, `DELETE`).
* Undo for incomplete transactions using before-images.
* Idempotent Redo using `PageLSN`.
* Modified `PageHeader` size to 24 bytes to accommodate `page_lsn`.
* Modified `SlottedPage` to expose `insertTupleAtSlot` and `updateTuple` for targeted tuple operations during recovery.

### Verification

* Tests passing: **334 / 334**
* Build: ✅
* Lint: ✅ (`clang-tidy passed`)
* Test: ✅

### Git Commit

```text
feat(wal): implement crash recovery (M3.4)
```

---

## M3.3 — Write-Ahead Logging (WAL)

**Status:** ✅ Complete

### Implemented

* `LogRecordType` enum (`BEGIN`, `INSERT`, `UPDATE`, `DELETE`, `COMMIT`, `ABORT`).
* Binary `LogRecord` serialization and deserialization.
* `LogManager` with `append`, `flush`, `flushAll`, `persistentLSN`, `nextLSN`.
* Monotonically increasing LSNs via `std::atomic<uint64_t>`.
* Buffered WAL writes into memory.
* BEGIN/COMMIT/ABORT logging hooks support.
* INSERT/UPDATE/DELETE record payload support with `RID` and `Tuple`.

### Verification

* Tests passing: **333 / 333**
* Build: ✅
* Lint: ✅ (`clang-tidy passed`)
* Test: ✅

### Git Commit

```text
feat(wal): implement write-ahead logging (M3.3)
```

---

## M3.0 — Transaction Manager Skeleton

**Status:** ✅ Complete

### Implemented

* `TransactionState` enum (`ACTIVE`, `COMMITTED`, `ABORTED`).
* `Transaction` class with `txn_id`, state, and timestamps.
* `TransactionManager` with `begin()`, `commit()`, `abort()`, and `getTransaction()`.
* Monotonically increasing transaction IDs via `std::atomic`.
* In-memory tracking of active transactions and RAII memory cleanup.
* Tests passing for begin, commit, abort, and ID incrementation.

### Verification

* Tests passing: **330 / 330**
* Build: ✅
* Lint: ✅
* Test: ✅

### Git Commit

```text
feat(transaction): implement transaction manager skeleton (M3.0)
```

---

## M2.8 — B+ Tree Delete & Rebalancing

**Status:** ✅ Complete

### Implemented

* `remove(key)` logic.
* Leaf and internal node merging.
* Sibling borrowing (redistribution).
* Root collapse when tree height shrinks.
* Tests passing for leaf redistribution, leaf merge, internal redistribution, internal merge, and root collapse.

### Verification

* Tests passing: **329 / 329**
* Build: ✅
* Lint: ✅ (`clang-tidy passed`)
* Test: ✅

### Git Commit

```text
feat(index): implement B+ tree delete & rebalancing (M2.8)
```

---

## M2.7 — B+ Tree Iterator & Range Scan

**Status:** ✅ Complete

### Implemented

* `BPlusTreeIterator` class for forward iteration over leaf pages.
* `begin()`, `begin(int64_t key)`, and `end()` methods in `BPlusTree`.
* Support for exact matches, lower bounds, and multi-node range scans.
* Automatic `ReadPageGuard` management within iterator to prevent pin leaks.
* Overloaded iterator operators (`++`, `*`, `==`, `!=`).
* Tests passing for empty tree, single node, multi-node, and range scans.

### Verification

* Tests passing: **322 / 322**
* Build: ✅
* Lint: ✅ (`clang-tidy passed`)
* Test: ✅

### Git Commit

```text
feat(index): implement B+ tree iterator and range scan (M2.7)
```

---

## M2.6 — Recursive B+ Tree Insertion

**Status:** ✅ Complete

### Implemented

* Internal page split API (`moveHalfTo`).
* Promotion of median key (removed from both children).
* Updating parent pointers of moved children.
* Recursive `insertIntoParent()` in `BPlusTree`.
* New root creation when the old root splits.
* RAII page guards used extensively to avoid pin leaks.
* Tests passing for internal node splits.

### Verification

* Tests passing: **318 / 318**
* Build: ✅
* Lint: ✅ (`clang-tidy passed`)
* Test: ✅

### Git Commit

```text
feat(index): implement recursive B+ tree insertion (M2.6)
```

---

## M2.5 — B+ Tree Splits

**Status:** ✅ Complete

### Implemented

* Leaf page splitting using `moveHalfTo` during `BPlusTree::insert`.
* Sibling leaf allocation via `BufferPoolManager` and link updates (`nextPageId`, `prevPageId`).
* `setParentPageId` added to leaf node.
* Root creation into a new internal page when the root splits.
* 3 new tests covering even/odd split logic, insertion triggering leaf splits, sibling links validation, and pin leak safety.

### Verification

* Tests passing: **317 / 317**
* Build: ✅
* Lint: ✅ (`clang-tidy passed`)
* Test: ✅

### Git Commit

```text
feat(index): implement B+ tree leaf split (M2.5)
```

---

## M2.4 — B+ Tree Leaf Insert

**Status:** ✅ Complete

### Implemented

* `BPlusTree::insert(int64_t key, RID rid)` public API.
* Empty tree creates a root leaf page via `BufferPoolManager`.
* Tree traversal to target leaf using `ReadPageGuard`.
* Insertion into leaf page with `WritePageGuard`.
* Duplicate key rejection (`Status::AlreadyExists`).
* Full leaf rejection (`Status::PageFull`).
* Proper dirty page propagation and pin leak prevention via RAII guards.
* Added `PageFull` to `Status` enum.
* 7 new tests covering empty tree insertion, ordered/random inserts, duplicates, full page behaviour, and pin leak validation.

### Verification

* Tests passing: **314 / 314**
* Build: ✅
* Lint: ✅ (`clang-tidy passed`)
* Test: ✅

### Git Commit

```text
feat(index): implement B+ tree leaf insert (M2.4)
```

---

## M2.3 — B+ Tree Search

**Status:** ✅ Complete

### Implemented

* `BPlusTree` — Read-only search using `BufferPoolManager` and `ReadPageGuard`.
* `create()` initializes an empty tree.
* `open()` opens an existing root.
* `getValue(int64_t key)` traverses from root to leaf to return `std::optional<RID>`.
* Automatic `ReadPageGuard` release at each step.
* 4 new tests covering empty tree, single-leaf lookup, multi-level routing, missing keys, boundary conditions, and guard release validation.

### Verification

* Tests passing: **307 / 307**
* Build: ✅
* Lint: ✅ (`clang-tidy passed`)
* Test: ✅

### Git Commit

```text
feat(index): implement B+ tree search (M2.3)
```

---

## M2.2 — B+ Tree Internal Pages

**Status:** ✅ Complete

### Implemented

* `BTreeInternalPage` — sorted array of separator keys + child `PageId` pointers.
* `lookup(key)` — return child page ID for a given search key.
* `insert(key, right_child)` — insert separator key and right-child pointer in sorted order.
* `keyAt()`, `childAt()`, `size()`, `maxSize()`, `isFull()` accessors.
* `serialize()` / `deserialize()` using project `Serializer` / `Deserializer` utilities.
* 11 new tests covering default init, population, sibling routing, sorted insertion,
  duplicate rejection, deletion, full-page behaviour, and round-trip serialisation.

### Verification

* Tests passing: **303 / 303**
* Build: ✅
* Lint: ✅ (`clang-tidy passed`)
* Test: ✅

### Git Commit

```text
feat(index): implement B+ Tree internal page (M2.2)
```

---

## M2.1 — B+ Tree Leaf Pages

**Status:** ✅ Complete

### Implemented

* `BTreeLeafPage` — concrete leaf node built on top of `BTreePage`.
* Concrete key type: `int64_t`; value type: `RID` (heap record identifier).
* Fixed on-disk leaf header: 16 B `BTreePage` + 4 B `prev_page_id` +
  4 B `next_page_id` = **24 bytes**.
* Entry size: 8 B key + 4 B `page_id` + 2 B `slot_id` = **14 bytes**.
* Maximum entries per page: `(kPageBodySize - 24) / 14 = 289`.
* Sorted insert by binary lower-bound + right-shift.
* Remove by binary search + left-shift.
* Binary search `lookup()` returning `std::optional<RID>`.
* `keyAt()`, `valueAt()`, `size()`, `maxSize()`, `isEmpty()`, `isFull()`.
* `prevPageId()` / `nextPageId()` sibling link getters and setters.
* `serialize()` / `deserialize()` field-by-field using project utilities.
* Duplicate key rejection (`Status::AlreadyExists`).
* Full-page insert rejection (`Status::InvalidArg`).
* 80 new tests: layout constants, init, sibling links, sorted insertion,
  duplicate rejection, full-page behaviour, slot accessors, binary search,
  deletion, round-trip serialisation, error handling, and boundary conditions.
* `hamdb_index` now links `hamdb_storage` for `RID` symbol resolution.

### Verification

* Tests passing: **292 / 292**
* Build: ✅
* Lint: ✅ (`clang-tidy passed`)
* Test: ✅

### Git Commit

```text
feat(index): implement B+ Tree leaf page (M2.1)
```

---

## M2.0 — B+ Tree Page Infrastructure

**Status:** ✅ Complete

### Implemented

* `PageType::BTreeInternal` and `PageType::BTreeLeaf` added to `enums.hpp`.
* `BTreePage` — shared 16-byte header for all B+ Tree node pages.
* Fixed header layout: `page_type` (1 B) + `current_size` (2 B) + `max_size` (2 B) +
  `parent_page_id` (4 B) + `page_id` (4 B) + reserved (3 B) = **16 bytes**.
* `serialize()` / `deserialize()` using project `Serializer` / `Deserializer` utilities.
* `isFull()` and `isRoot()` convenience predicates.
* Full getter/setter API with `[[nodiscard]]` and `noexcept`.
* `hamdb_index` static library (`src/index/`).
* 51 new tests covering default init, parameterised construction, getters/setters,
  serialised byte layout, header size constant, round-trip serialisation,
  error handling (buffer-too-small), equality, parent metadata, and larger-buffer
  compatibility.

### Verification

* Tests passing: **212 / 212**
* Build: ✅
* Lint: ✅ (`clang-tidy passed`)
* Test: ✅

### Git Commit

```text
feat(index): implement B+ Tree page infrastructure (M2.0)
```

---

## M1.9 — Page Guards (RAII)

**Status:** ✅ Complete

### Implemented

* `BasicPageGuard` — move-only RAII owner of a pinned `BufferFrame`.
* `ReadPageGuard` — const-only page access; unpins with dirty=false.
* `WritePageGuard` — mutable page access; propagates `markDirty()` on unpin.
* `drop()` for early release; destructor auto-unpins if still valid.
* `isValid()` / `pageId()` / `page()` / `pageMut()` observers.
* `BufferPoolManager::fetchPageRead()`, `fetchPageWrite()`, `newPageGuard()` factory methods.
* 26 new tests covering auto-unpin, move semantics, drop(), dirty propagation,
  moved-from safety, nested scopes, pin-count correctness, and round-trip persistence.

### Verification

* Tests passing: **161 / 161**
* Build: ✅
* Lint: ✅ (`clang-tidy passed`)
* Test: ✅

### Git Commit

```text
feat(buffer): implement page guards (RAII)
```

---

## M1.8 — LRU-K Replacement Policy

**Status:** ✅ Complete

### Implemented

* LRUKReplacer algorithm.
* Access history timestamps (tracking up to K accesses).
* Backward K-distance calculation for +inf and finite distances.
* Tie-breaking logic (oldest timestamp, smaller FrameId).
* BufferPoolManager integration (cache miss eviction, dirty page flushing).
* Pinned frame tracking (evictability).

### Verification

* Tests passing: **135 / 135**
* Build: ✅
* Lint: ✅
* Test: ✅

### Git Commit

```text
feat(buffer): implement LRU-K replacement policy
```

---

# Current Architecture Snapshot

```text
SQL Layer (future)
        │
Executor (future)
        │
TableHeap
        │
BufferPoolManager ←── BPlusTree → BTreePage (index layer)
        │
DiskManager
        │
users.hamdb
```

---

# Upcoming Milestone

## M4.9 — SQL Lexer

### Goal

Implement lexical analysis for SQL statements.

### Deliverables

* Token types.
* Lexer class.
* Keyword recognition.

### Expected Tests

Approximately **372+ total tests** after completion.

---

# Testing History

| Milestone | Passing Tests |
| --------- | ------------- |
| M1.1      | 19            |
| M1.2      | 30            |
| M1.3      | 44            |
| M1.4      | 67            |
| M1.5      | 96            |
| M1.6      | 125           |
| M1.7      | 131           |
| M1.8      | 135           |
| M1.9      | 161           |
| M2.0      | 212           |
| M2.1      | 292           |
| M2.2      | 303           |
| M2.3      | 307           |
| M2.4      | 314           |
| M2.5      | 317           |
| M2.6      | 318           |
| M2.7      | 322           |
| M2.8      | 329           |
| M3.0      | 330           |
| M3.1      | 331           |
| M3.2      | 332           |
| M3.3      | 333           |
| M3.4      | 334           |
| M4.0      | 337           |
| M4.1      | 338           |
| M4.2      | 338           |
| M4.3      | 338           |
| M4.4      | 340           |
| M4.5      | 338           |
| M4.6      | **338**       |

---

# Version History

| Version    | Milestone                            |
| ---------- | ------------------------------------ |
| v0.1.0-dev | M1.1–M1.7                            |
| v0.2.0-dev | After Buffer Pool & LRU-K            |
| v0.3.0-dev | After B+ Tree Page Infrastructure    |
| v0.4.0-dev | After B+ Tree full implementation    |
| v0.5.0-dev | After SQL Parser                     |
| v1.0.0     | Basic SQL database with transactions |

## M1.8 — LRU-K Replacement Policy

**Status:** ✅ Complete

### Implemented

* LRUKReplacer (K = 2).
* Access history tracking.
* Backward K-distance calculation.
* Infinite-distance handling for pages with fewer than K accesses.
* Deterministic victim selection.
* Evictable frame tracking.
* Integration with BufferPoolManager.
* Dirty-page flushing before eviction.

### Verification

* Build: ✅
* Lint: ✅
* Tests: ✅ (all tests passing)

### Git Commit

feat(buffer): implement LRU-K replacement policy

---

## M3.2 — MVCC (Snapshot Isolation)

**Status:** ✅ Complete

### Implemented

* `TupleVersion` struct: `begin_txn_id`, `end_txn_id`, `is_committed`, `is_deleted`, `data`, `prev` version chain pointer.
* `isVisibleTo(snapshot_ts)` visibility predicate (snapshot isolation: committed + in timestamp range).
* Version chains per RID stored in `MvccManager::chains_`.
* `insert` — creates a new uncommitted version head; rejects duplicates and write-write conflicts.
* `update` — stamps the current head's `end_txn_id`, inserts new uncommitted head with old head as `prev`.
* `remove` — stamps the current head's `end_txn_id`, inserts a tombstone version.
* `commit` — stamps `is_committed = true` on all versions owned by the committing txn.
* `abort` — rebuilds each affected chain, dropping all versions owned by the aborted txn and restoring `end_txn_id` on the new head.
* `read` — walks chain newest-to-oldest; returns own uncommitted writes or committed versions within snapshot.
* `exists` / `versionCount` helpers for testing and diagnostics.
* Write-write conflict detection: rejects writes when another uncommitted txn owns the current head.
* `mvcc_manager_test.cpp` with 27 tests covering all lifecycle paths.

### Verification

* Tests passing: **332 / 332**
* Build: ✅
* Lint: ✅ (`clang-tidy passed`)
* Test: ✅

### Git Commit

```text
feat(transaction): implement MVCC snapshot isolation (M3.2)
```

---

## M3.1 — Lock Manager (Shared / Exclusive)

**Status:** ✅ Complete

### Implemented

* `LockMode` (SHARED, EXCLUSIVE).
* `LockManager` with `lockShared`, `lockExclusive`, `lockUpgrade`, `unlock`, and `releaseAll`.
* `LockRequestQueue` using `std::condition_variable` and `std::deque`.
* Thread-safety via a global `std::mutex`.
* Update to `Transaction` to store `shared_lock_set_` and `exclusive_lock_set_`.
* Added `RIDHash`.
* Wait-only condition variable handling.

### Verification

* Build: ✅
* Lint: ✅
* Tests: ✅ (331 / 331 passing)

### Git Commit

```text
feat(transaction): implement lock manager (shared / exclusive) (M3.1)
```
