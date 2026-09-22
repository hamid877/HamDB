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
| Current Version  | v0.1.0-dev                  |
| Overall Progress | **31%**                     |

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
| M1.7 | Buffer Pool Manager Skeleton | ✅ Complete | **131** |
| M1.8 | LRU-K Replacement Policy     | ⏳ Next     | —       |
| M1.9 | Page Guards (RAII)           | ⬜ Planned  | —       |

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

## M1.7 — Buffer Pool Manager Skeleton

**Status:** ✅ Complete

### Implemented

* BufferFrame abstraction.
* BufferPoolManager.
* Fixed-size frame pool.
* Page table (`PageId → FrameId`).
* Pin / unpin.
* Dirty page tracking.
* Flush page.
* Flush all pages.
* Pool full detection.

### Verification

* Tests passing: **131 / 131**
* Build: ✅
* Lint: ✅
* Test: ✅

### Git Commit

```text
feat(buffer): implement buffer pool manager skeleton
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

## M1.8 — LRU-K Replacement Policy

### Goal

Replace "first free frame only" behavior with an LRU-K page replacement algorithm.

### Deliverables

* LRUKReplacer.
* Access history timestamps.
* Backward K-distance calculation.
* Evictable frame tracking.
* Victim selection.
* BufferPoolManager integration.

### Expected Tests

Approximately **150+ total tests** after completion.

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
| M1.7      | **131**       |

---

# Version History

| Version    | Milestone                            |
| ---------- | ------------------------------------ |
| v0.1.0-dev | M1.1–M1.7                            |
| v0.2.0-dev | After Buffer Pool & LRU-K            |
| v0.3.0-dev | After B+ Tree                        |
| v0.4.0-dev | After SQL Parser                     |
| v1.0.0     | Basic SQL database with transactions |
