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
| Overall Progress | **59%**                     |

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
| M4.7 | SQL Lexer  | ⬜      |
| M4.8 | SQL Parser | ⬜      |
| M4.9 | AST        | ⬜      |
| M4.10 | Planner    | ⬜      |
| M4.11 | Executor   | ⬜      |

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

# Completed Milestones

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

## M4.7 — SQL Lexer

### Goal

Implement lexical analysis for SQL statements.

### Deliverables

* Token types.
* Lexer class.
* Keyword recognition.

### Expected Tests

Approximately **370+ total tests** after completion.

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
