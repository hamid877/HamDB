# HamDB

A lightweight relational database engine written from scratch in **C++20**.

HamDB is an educational yet production-quality project that explores how a relational
database works at the systems level — from raw page I/O all the way up to query
execution.

---

## Project Status

| Milestone | Description | Status |
|-----------|-------------|--------|
| 1 | Project scaffolding | 🔨 In progress |
| 2 | Page & disk storage | ⏳ Planned |
| 3 | Buffer pool manager | ⏳ Planned |
| 4 | Catalog & schema | ⏳ Planned |
| 5 | SQL parser | ⏳ Planned |
| 6 | Query executor | ⏳ Planned |
| 7 | B-Tree index | ⏳ Planned |
| 8 | Transactions & WAL | ⏳ Planned |
| 9 | Network layer | ⏳ Planned |

---

## Architecture

```
HamDB
├── include/
│   ├── common/          # Shared utilities (types, status, serialization)
│   └── storage/         # Page layout, disk I/O
├── src/
│   ├── common/          # Implementations of common utilities
│   └── storage/         # Implementations of storage classes
├── tests/
│   └── storage/         # GoogleTest suites per module
├── examples/            # Standalone usage examples
└── docs/                # Doxygen-generated API docs
```

---

## Requirements

| Tool | Minimum version |
|------|----------------|
| CMake | 3.20 |
| C++ compiler (GCC / Clang) | C++20 support |
| Doxygen *(optional)* | 1.9 |
| dot / Graphviz *(optional)* | any |

---

## Building

```bash
# 1. Configure
cmake -B build -DCMAKE_BUILD_TYPE=Debug

# 2. Compile
cmake --build build -j$(nproc)

# 3. Run tests
ctest --test-dir build --output-on-failure

# 4. Generate docs (optional — requires Doxygen)
cmake --build build --target docs
```

---

## Coding Standards

* C++20 only — no external database libraries.
* RAII everywhere; smart pointers for owned resources.
* Header / source separation.
* One responsibility per class.
* Every public symbol has a Doxygen comment.
* Namespace: `hamdb::*`

---

## License

MIT — see [LICENSE](LICENSE) for details.
