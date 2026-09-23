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
| Current Version  | v0.2.0-dev                  |
| Overall Progress | **40%**                     |

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
| M2.0 | B+ Tree Leaf Pages | ⬜      |
| M2.1 | Internal Pages     | ⬜      |
| M2.2 | Search Algorithm   | ⬜      |
| M2.3 | Insert & Split     | ⬜      |
| M2.4 | Delete & Merge     | ⬜      |
| M2.5 | Index Iterator     | ⬜      |

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
BufferPoolManager
        │
DiskManager
        │
users.hamdb
```

---

# Upcoming Milestone

## M2.0 — B+ Tree Leaf Pages

### Goal

Implement B+ Tree leaf pages as the foundation for the index engine.

### Deliverables

* `BPlusTreeLeafPage` with key/value slot array.
* Insert, search, and delete on a single leaf.
* Overflow detection (page full).
* Leaf page serialization/deserialization.
* Unit tests for all leaf-page operations.

### Expected Tests

Approximately **185+ total tests** after completion.

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
| M1.9      | **161**       |

---

# Version History

| Version    | Milestone                            |
| ---------- | ------------------------------------ |
| v0.1.0-dev | M1.1–M1.7                            |
| v0.2.0-dev | After Buffer Pool & LRU-K            |
| v0.3.0-dev | After B+ Tree                        |
| v0.4.0-dev | After SQL Parser                     |
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
