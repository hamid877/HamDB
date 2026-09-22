# Storage Engine — Lesson 2 Notes

## What is DiskManager?

DiskManager is responsible for moving fixed-size pages between memory and the `.hamdb` file.

It does not understand SQL, tables, rows, or indexes.

---

## Why pages instead of rows?

* Fixed-size I/O.
* Efficient disk access.
* Alignment with operating system page cache.
* Simpler buffer management.

---

## HamDB Page Size

4096 bytes.

Reasons:

* OS memory page alignment.
* SSD/HDD efficiency.
* Simple page offset calculation.

---

## Metadata Page Layout

| Offset | Field             | Bytes |
| ------ | ----------------- | ----- |
| 0      | Magic Number      | 8     |
| 8      | Version           | 4     |
| 12     | Page Size         | 4     |
| 16     | Total Pages       | 4     |
| 20     | Free Page Pointer | 4     |
| 24     | UUID              | 16    |
| 40     | Created Timestamp | 8     |
| 48–63  | Reserved          | 16    |

Total metadata header: 64 bytes.

Remaining bytes in page 0 are initialized to zero.

---

## Page Offset Formula

page_offset = page_id × PAGE_SIZE

Examples:

* Page 0 → offset 0
* Page 1 → offset 4096
* Page 2 → offset 8192
