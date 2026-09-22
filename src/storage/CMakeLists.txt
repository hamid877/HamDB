add_library(hamdb_storage STATIC
    database_metadata.cpp
    page_header.cpp
    page.cpp
    disk_manager.cpp
    tuple_slot.cpp
    slotted_page.cpp
    tuple.cpp
    rid.cpp
    table_heap.cpp
    heap_iterator.cpp
)

target_link_libraries(hamdb_storage PUBLIC
    hamdb_common
    hamdb_utils
)

target_include_directories(hamdb_storage PUBLIC
    ${PROJECT_SOURCE_DIR}/include
)

target_compile_features(hamdb_storage PUBLIC cxx_std_20)
