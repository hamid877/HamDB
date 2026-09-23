# HamDB — AI Agent Master Instructions (Claude.md)

Version: 1.0
Project: HamDB
Language: C++20
Architecture Style: Production-quality educational relational database engine

---

# 1. Project Vision

HamDB is a lightweight relational database engine built completely from scratch in modern C++20.

The goal is **not** to clone SQLite or PostgreSQL, but to understand and implement the core internal components of a relational database while maintaining production-quality engineering practices.

The implementation should be modular, test-driven, deterministic, and easy to extend.

Target platform:

* Linux (Ubuntu / Linux Mint)
* GCC 13+
* CMake + Ninja
* GoogleTest
* clang-format
* clang-tidy

---

# 2. AI Agent Role

You are a senior C++ systems engineer working on HamDB.

Assume the repository already exists.

Do **not** regenerate the project structure.

Only implement the requested milestone while preserving existing APIs unless explicitly instructed.

Never rewrite completed modules unnecessarily.

Always keep backward compatibility with previous milestones and tests.

---

# 3. Technology Stack

| Component       | Choice       |
| --------------- | ------------ |
| Language        | C++20        |
| Compiler        | GCC 13+      |
| Build System    | CMake        |
| Generator       | Ninja        |
| Tests           | GoogleTest   |
| Formatting      | clang-format |
| Static Analysis | clang-tidy   |
| Documentation   | Doxygen      |
| Version Control | Git          |

No external database libraries.

---

# 4. Engineering Principles

## Mandatory Rules

* C++20 only.
* RAII everywhere.
* No global mutable state.
* No raw owning pointers.
* Smart pointers only for ownership.
* Stack allocation preferred where ownership is local.
* Deterministic memory management.
* One responsibility per class.
* Header/source separation.
* Every module independently testable.
* Keep functions small.
* Prefer composition over inheritance.
* Avoid unnecessary templates.

## Never Use

* SQLite
* RocksDB
* LevelDB
* LMDB
* PostgreSQL source code
* Boost
* ORM libraries
* Serialization libraries

Only C++ standard library unless explicitly approved.

---

# 5. Project Architecture

```
HamDB
│
├── include/
│   ├── common/
│   ├── storage/
│   ├── buffer/
│   ├── catalog/
│   ├── parser/
│   ├── executor/
│   ├── planner/
│   ├── index/
│   ├── transaction/
│   ├── wal/
│   └── network/
│
├── src/
│   ├── common/
│   ├── storage/
│   ├── buffer/
│   ├── catalog/
│   ├── parser/
│   ├── executor/
│   ├── planner/
│   ├── index/
│   ├── transaction/
│   ├── wal/
│   └── network/
│
├── tests/
│   ├── common/
│   ├── storage/
│   ├── buffer/
│   ├── index/
│   ├── executor/
│   └── parser/
│
├── examples/
├── docs/
├── scripts/
├── benchmarks/
└── CMakeLists.txt
```

Every directory represents a database subsystem.

---

# 6. Coding Standards

## Naming

Namespaces:

```cpp
namespace hamdb {
}
```

Classes:

```cpp
BufferPoolManager
DiskManager
SlottedPage
PageHeader
HeapIterator
```

Methods:

```cpp
fetchPage()
insertTuple()
flushPage()
readTuple()
```

Private members:

```cpp
page_id_
pin_count_
is_dirty_
frames_
```

Constants:

```cpp
kPageSize
kInvalidPageId
kMagicNumber
```

Enums:

```cpp
enum class Status
enum class PageType
```

---

## Formatting

* 4 spaces.
* No tabs.
* Opening brace on new line.
* `const` correctness everywhere.
* `[[nodiscard]]` for functions returning important values.
* `noexcept` whenever applicable.

---

## File Size

Target:

* Header: under 250 lines.
* Source: under 300 lines.
* Split large classes into helper files instead of growing indefinitely.

---

# 7. Documentation Style

Every public class must have Doxygen comments.

Example:

```cpp
/**
 * @brief Reads and writes fixed-size pages from a database file.
 */
class DiskManager
```

Public methods require documentation.

Private methods do not unless non-obvious.

---

# 8. Error Handling

HamDB never throws exceptions for database operations.

Use `Status`.

Example:

```cpp
enum class Status {
    Ok,
    InvalidPage,
    PageFull,
    BufferPoolFull,
    NotFound,
    IOError,
    Corruption,
    Unsupported
};
```

Return `Status` from storage operations.

Tests should verify returned status values.

---

# 9. Memory Management Rules

## Ownership

| Situation          | Rule                                           |
| ------------------ | ---------------------------------------------- |
| Single owner       | `std::unique_ptr`                              |
| Shared ownership   | `std::shared_ptr` only if absolutely necessary |
| Temporary access   | References or pointers                         |
| Fixed-size arrays  | `std::array` or `std::unique_ptr<T[]>`         |
| Dynamic containers | `std::vector` only for logical collections     |

Never leak ownership.

---

## RAII

Resources automatically release.

Examples:

* file descriptors
* mutexes
* page guards
* allocated memory

Destructor performs cleanup.

---

# 10. Testing Rules

Every milestone includes tests.

Rules:

* Unit tests for every class.
* Integration tests for subsystem interactions.
* Existing tests must continue passing.
* New milestone must not reduce coverage.

Naming:

```
buffer_pool_manager_test.cpp
disk_manager_test.cpp
table_heap_test.cpp
```

Test naming:

```cpp
TEST(BufferPoolManagerTest, FetchPageCacheHit)
```

---

# 11. Build Verification

Every implementation must pass:

```bash
make configure
make build
make lint
make test
```

Definition of done:

* Build succeeds.
* clang-format clean.
* clang-tidy has no project warnings.
* All tests pass.

---

# 12. Static Analysis Rules

clang-tidy warnings inside HamDB code should be fixed.

Ignore warnings originating from system headers or GoogleTest.

No new project warnings should be introduced.

---

# 13. Module Boundaries

## Storage Layer

Responsible for:

* Page
* PageHeader
* DiskManager
* MetadataPage
* Serializer
* Deserializer
* SlottedPage
* RID
* TableHeap
* HeapIterator
* Tuple

Storage **does not** know SQL.

---

## Buffer Layer

Responsible for:

* BufferFrame
* BufferPoolManager
* Page table
* Pin counts
* Dirty flags
* Flush logic

Buffer layer communicates with DiskManager.

TableHeap communicates with BufferPoolManager.

---

## Index Layer

Responsible for:

* B+ Tree
* Leaf/Internal pages
* Search
* Insert
* Delete
* Split
* Merge

Index layer never accesses disk directly.

---

## Catalog Layer

Responsible for metadata.

* Table definitions.
* Column definitions.
* Schema.
* Table IDs.
* Metadata persistence.

---

## Parser Layer

Responsible for SQL parsing only.

Output is AST.

No execution.

---

## Planner Layer

Responsible for logical plan generation.

AST → Logical Operators.

---

## Executor Layer

Responsible for execution operators.

* Seq Scan
* Index Scan
* Insert
* Delete
* Update
* Join
* Aggregation

Executor uses TableHeap and Index.

---

## Transaction Layer

Responsible for:

* Transaction Manager.
* MVCC.
* Locks.
* Isolation levels.

---

## WAL Layer

Responsible for:

* Log records.
* LSN.
* Redo.
* Undo.
* Recovery.

---

# 14. Page Architecture

Default page size:

```cpp
constexpr std::size_t kPageSize = 4096;
```

Every page is exactly 4096 bytes.

Memory layout:

```
+----------------------+
| Page Header          |
+----------------------+
| Slot Directory       |
|        ↓             |
|    Free Space        |
|        ↑             |
| Tuple Data           |
+----------------------+
```

---

# 15. Buffer Pool Architecture

Buffer Pool owns memory frames.

Frame contains:

* Page
* PageId
* FrameId
* pin_count
* dirty
* valid

Page table maps:

```
PageId → FrameId
```

No replacement policy until Milestone 1.8.

---

# 16. Serialization Rules

Serializer converts primitive types into little-endian byte sequences.

Deserializer performs inverse operation.

Never depend on compiler struct layout.

Never serialize raw structs with `reinterpret_cast`.

Always serialize field-by-field.

---

# 17. Git Workflow

One commit per milestone.

Format:

```
feat(storage): implement table heap
feat(buffer): implement buffer pool manager
feat(index): implement leaf page split
```

Never combine unrelated milestones.

---

# 18. Completed Milestones

## M1.1 — Project Architecture

Implemented:

* Repository structure.
* CMake.
* GoogleTest.
* clang-format.
* clang-tidy.
* Makefile.

Status: Complete.

---

## M1.2 — Page & PageHeader

Implemented:

* Page.
* PageHeader.
* Page types.
* Fixed page size.
* Unit tests.

Status: Complete.

---

## M1.3 — Database Metadata Page

Implemented:

* Metadata page.
* Magic number.
* Version.
* UUID.
* Page count.
* File initialization.
* Example database creation.

Status: Complete.

---

## M1.4 — Serializer / Deserializer

Implemented:

* Integer serialization.
* Floating-point serialization.
* Boolean serialization.
* String serialization.
* Byte span serialization.

Status: Complete.

---

## M1.5 — Slotted Pages

Implemented:

* Slot directory.
* Variable-length tuples.
* Insert.
* Read.
* Delete.
* Compaction.
* Free-space accounting.

Status: Complete.

---

## M1.6 — Table Heap

Implemented:

* RID.
* Tuple abstraction.
* Linked heap pages.
* TableHeap.
* HeapIterator.
* Sequential scan.
* Cross-page inserts.
* Read/delete by RID.

Status: Complete.

---

## M1.7 — Buffer Pool Manager

Implemented:

* BufferFrame.
* BufferPoolManager.
* Fixed-size frame array.
* Page table.
* Pin/unpin.
* Dirty page tracking.
* Flush page.
* Flush all pages.
* Pool full detection.

Status: Complete.

---

# 19. Upcoming Roadmap

## Week 3

### M1.8 — LRU-K Replacement Policy

* Access history.
* Backward K-distance.
* Evictable frames.
* Victim selection.

### M1.9 — Page Guards

* ReadPageGuard.
* WritePageGuard.
* Automatic pin/unpin via RAII.

---

## Week 4

### M2.0 — B+ Tree

* Leaf pages.
* Internal pages.
* Search.
* Insert.
* Split.
* Merge.
* Iterator.

---

## Week 5

### Catalog

* Schema.
* Table metadata.
* Column metadata.
* Catalog persistence.

---

## Week 6

### SQL Parser

* Lexer.
* Parser.
* AST.
* SQL grammar.

---

## Week 7

### Planner & Executor

* Logical plan.
* Sequential scan.
* Insert/Delete.
* Projection.
* Filter.

---

## Week 8+

### Transactions & Recovery

* WAL.
* LSN.
* Recovery.
* MVCC.
* Lock Manager.

---

# 20. AI Agent Implementation Rules

For every milestone:

1. Read Claude.md first.
2. Modify only required files.
3. Preserve completed APIs.
4. Briefly explain design (2–4 sentences max).
5. List created/modified files.
6. Generate complete code.
7. Generate tests.
8. Ensure build instructions remain unchanged.
9. Do not modify unrelated modules.
10. Do not repeat architecture already defined here.

Assume Claude.md is the authoritative specification for HamDB.

## Documentation Maintenance Policy

HamDB maintains two authoritative project documents.

### Claude.md

Purpose:

* AI engineering specification.
* Coding standards.
* Architecture contracts.
* Module boundaries.
* Implementation rules.

Update Claude.md only when:

* Architecture changes.
* Coding standards change.
* New permanent engineering rules are introduced.
* A new core subsystem is added.

### docs/ROADMAP.md

Purpose:

* Living development roadmap.
* Milestone progress.
* Test count history.
* Version history.
* Current architecture snapshot.
* Upcoming milestone.

Update ROADMAP.md **after every completed milestone**.


### AI Agent Rule

At the end of every milestone implementation:

1. Update `docs/ROADMAP.md` with the completed milestone, test count, architecture snapshot, and next milestone.
2. Update `Claude.md` only if permanent project rules or architecture changed.
3. Treat both documents as project artifacts and include them in the milestone commit if modified.

### M1.8 — LRU-K Replacement Policy

Implemented:

* LRUKReplacer (K = 2).
* Access history timestamps.
* Backward K-distance calculation.
* Infinite-distance handling.
* Deterministic victim selection.
* BufferPoolManager integration.

Status: Complete.

---

### M1.9 — Page Guards (RAII)

Implemented:

* BasicPageGuard (move-only, auto-unpin on destruction).
* ReadPageGuard (const-only Page access).
* WritePageGuard (mutable Page access + markDirty()).
* drop() for early release.
* isValid() ownership predicate.
* BufferPoolManager::fetchPageRead(), fetchPageWrite(), newPageGuard().

Status: Complete.
