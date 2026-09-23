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
| Overall Progress | **54%**                     |

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
| M2.8 | Delete & Merge              | ⬜          |

---

## Phase 3 — Catalog

| ID   | Milestone                  | Status |
| ---- | -------------------------- | ------ |
| M3.0 | Schema Representation      | ⬜      |
| M3.1 | Catalog Manager            | ⬜      |
| M3.2 | Table Metadata Persistence | ⬜      |

---

## Phase 4 — SQL Engine

| ID   | Milestone  | Status |
| ---- | ---------- | ------ |
| M4.0 | SQL Lexer  | ⬜      |
| M4.1 | SQL Parser | ⬜      |
| M4.2 | AST        | ⬜      |
| M4.3 | Planner    | ⬜      |
| M4.4 | Executor   | ⬜      |

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

## M2.8 — Delete & Merge

### Goal

Implement B+ Tree deletion and page merging logic.

### Deliverables

* `remove(key)` logic.
* Leaf and internal node merging.
* Sibling borrowing (redistribution).

### Expected Tests

Approximately **335+ total tests** after completion.

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
| M2.7      | **322**       |

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
