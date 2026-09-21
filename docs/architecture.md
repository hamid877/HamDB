# HamDB — Architecture

> Last updated: Milestone 1.1 (structural refactor)

---

## Module Map

```
hamdb_core  (INTERFACE — links everything)
│
├── hamdb_common     (enums.cpp)
│   └── Provides: PageType, Status, statusToString
│
├── hamdb_storage    (page_header.cpp, page.cpp, disk_manager.cpp)
│   └── Depends on: hamdb_common
│   └── Provides: PageHeader, Page, DiskManager
│
├── hamdb_utils      (serializer.cpp, deserializer.cpp)
│   └── Depends on: hamdb_common
│   └── Provides: Serializer, Deserializer
│
└── hamdb_database   (database.cpp)
    └── Depends on: hamdb_common, hamdb_storage
    └── Provides: Database
```

---

## Folder Hierarchy

```
include/
├── common/
│   ├── constants.hpp      Compile-time constants (kPageSize, kMagic, PageId, …)
│   ├── enums.hpp          Shared enumerations (PageType, Status)
│   ├── config.hpp         Runtime Config aggregate
│   ├── logger.hpp         Logging facade (stub)
│   └── exception.hpp      Exception hierarchy (HamDBException and subtypes)
│
├── storage/
│   ├── page_header.hpp    PageHeader struct — 16-byte on-disk page header
│   ├── page.hpp           Page class — fixed 4 KiB I/O unit (non-copyable)
│   └── disk_manager.hpp   DiskManager — POSIX page-granular file I/O
│
├── utils/
│   ├── serializer.hpp     Serializer — writes primitives into a byte span
│   └── deserializer.hpp   Deserializer — reads primitives from a byte span
│
└── database/
    └── database.hpp       Database — top-level user-facing façade

src/
├── common/       enums.cpp
├── storage/      page_header.cpp, page.cpp, disk_manager.cpp
├── utils/        serializer.cpp, deserializer.cpp
└── database/     database.cpp
```

---

## Include Conventions

| From module | Include |
|---|---|
| Any file needing page constants | `#include "common/constants.hpp"` |
| Any file needing `PageType` / `Status` | `#include "common/enums.hpp"` |
| Storage code | `#include "storage/page.hpp"` etc. |
| Serialization | `#include "utils/serializer.hpp"` etc. |
| Top-level code | `#include "database/database.hpp"` |

---

## Page Layout

```
┌─────────────────────────────────┐  offset 0
│         PageHeader (16 B)       │
│  page_id (4) | type (1)         │
│  free_space_ptr (2) | slots (2) │
│  checksum (4) | reserved (3)    │
├─────────────────────────────────┤  offset 16
│                                 │
│         Page Body               │
│         (4080 B)                │
│                                 │
└─────────────────────────────────┘  offset 4096
```

---

## PageType Values

| Enum value | Int | Description |
|---|---|---|
| `PageType::Free`     | 0 | Unallocated — available for reuse |
| `PageType::Metadata` | 1 | Database file header / catalog root |
| `PageType::Table`    | 2 | Heap-file page with row data |
| `PageType::Index`    | 3 | B-tree node (internal or leaf) |
| `PageType::Overflow` | 4 | Variable-length column overflow |

---

## Future Modules (Planned)

| Module | Milestone | Description |
|---|---|---|
| `buffer/` | 3 | Buffer pool — LRU page cache |
| `catalog/` | 4 | Schema management — tables, columns |
| `parser/` | 5 | SQL lexer and parser |
| `executor/` | 6 | Query planner and executor |
| `index/` | 7 | B-tree index implementation |
| `transaction/` | 8 | MVCC and lock manager |
| `wal/` | 8 | Write-ahead log |
| `network/` | 9 | Client protocol layer |
