# TheebZip ⚡
**High-Throughput, Hardware-Accelerated Zero-Copy Compression Engine**

[![License](https://img.shields.io/badge/License-Apache_2.0-blue.svg)](https://opensource.org/licenses/Apache-2.0)
[![C++ Standard](https://img.shields.io/badge/C%2B%2B-17-darkblue.svg)](https://en.cppreference.com/w/cpp/17)
[![Platform](https://img.shields.io/badge/Platform-Windows_x86--64-brightgreen.svg)]()
[![Build](https://img.shields.io/badge/Optimization-AVX2%20%7C%20SSE4.2%20%7C%20OpenMP-orange.svg)]()

**TheebZip** is an ultra-fast data compression and extraction tool designed for maximum I/O throughput on modern multi-core architectures. Built with **Modern C++17**, it leverages OS-level direct memory mapping (**Zero-Copy MMF**), hardware-level data integrity acceleration (**SSE4.2 CRC32C**), and dynamic parallel block scheduling via **OpenMP** to eliminate traditional I/O bottlenecks.

---

## 🏛️ Architectural Overview

TheebZip bypasses traditional userspace-kernel buffering streams (`fread`/`ifstream`) to interact directly with the virtual address space:

+-------------------------------------------------------------------------+
|                              TheebZip Engine                            |
+-------------------------------------------------------------------------+
|                                              |
[Memory-Mapped I/O]                         [Hardware Acceleration]
CreateFileMapping / MapViewOfFile            SSE4.2 Intrinsics (_mm_crc32_u64)
(Eliminates Kernel-User Copying)             (8-byte per-cycle data integrity)
|                                              |
+----------------------+-----------------------+
|
[Parallel Pipeline]
OpenMP Dynamic Scheduler
(Scales linearly across P/E-cores)
|
+-----------------+-----------------+
|                                   |
[.theeb Native Format]           [Universal Fallback]
Custom Binary Framing            Native Win32 Extraction
(IFD1 Magic Header)              (RAR, ZIP, 7Z, ISO, TAR)

---

## 🚀 Key Technical Highlights

* **Zero-Copy Memory Mapping:** Maps entire files directly into memory space (`MapViewOfFile`), achieving throughput rates bounded only by the underlying NVMe/SSD physical hardware.
* **Hardware-Accelerated Integrity Check:** Leverages Intel `SSE4.2` hardware intrinsics (`_mm_crc32_u64`) to calculate real-time **CRC32C** checksums with near-zero CPU penalty.
* **Multi-Core Scaling:** Dynamic block-level parallelism (`#pragma omp parallel for schedule(dynamic, 64)`) designed to balance compute loads efficiently across heterogeneous core layouts (Intel P-Cores & E-Cores).
* **Native Custom Binary Framing (`.theeb`):** Structured binary header with `IFD1` magic bytes, original size tracking, block-level indexing, and direct random-access offset tables.
* **Universal Archive Engine:** Integrated transparent fallback extractor for common system archives (`.rar`, `.zip`, `.7z`, `.iso`, `.tar`, `.gz`) supporting full Unicode/UTF-16 path resolution.
* **Ultra-Lightweight Hybrid Interface:** Includes a fully asynchronous, dual-language (Arabic / English) native **Win32 GUI** (`<120 KB` executable footprint) alongside a high-performance **CLI mode**.

---

## 📊 Real-World Performance Benchmarks

Tested on a **5.0 GB** live operating system image (`.iso`):

| Operation | Input File Size | Output File Size | Throughput (Speed) | Processing Time | Integrity Status |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **Decompression** | `4.60 GB` | `4.97 GB` | **0.761 GB/s (~761 MB/s)** | **~7.8 sec** | `CRC32C Match (0xddd11371)` |
| **Compression** | `4.97 GB` | `3.91 GB` | **0.748 GB/s (~748 MB/s)** | **~8.2 sec** | `Verified` |

> *Test Environment: x86-64 Architecture, NVMe PCIe Storage, Windows 11.*

---

## 📂 Project Structure

```text
TheebZip/
├── compress.cpp      # Parallel compression & .theeb container generator
├── decompress.cpp    # Zero-copy native decompressor & universal extractor
├── gui.cpp           # Native Win32 UI (Bilingual) & CLI Entry Point
├── LICENSE           # Apache 2.0 Open-Source License
├── .gitignore        # Git exclusion rules
└── README.md         # Architecture & technical documentation

🛠️ Build & Compilation

To compile TheebZip with maximum vectorization, SIMD optimizations, and OpenMP multi-threading enabled:
GCC / MinGW-w64:
g++ -O3 -mavx2 -msse4.2 -fopenmp -std=c++17 compress.cpp decompress.cpp gui.cpp -o theebzip.exe -luser32 -lcomdlg32 -lgdi32 -lshell32

Intel oneAPI (ICX Compiler):
icx -O3 -mavx2 -msse4.2 -qopenmp /std:c++17 compress.cpp decompress.cpp gui.cpp -o theebzip.exe user32.lib comdlg32.lib gdi32.lib shell32.lib

💻 Usage
1. Graphical Interface (GUI)

Run theebzip.exe directly without CLI arguments to launch the lightweight native Win32 dashboard:

    One-Click Compress/Decompress: Select files via native Explorer dialogs.

    Instant Language Toggle: Switch seamlessly between Arabic and English.

    Non-Blocking UI: Heavy compute tasks run on detached background worker threads (std::thread).

2. Command Line Interface (CLI)

# Compress a target file to .theeb format
theebzip.exe -c <input_path> [output_path.theeb]

# Extract a .theeb archive or any supported format
theebzip.exe -d <archive_path> [output_path]
🗺️ Roadmap (v2.0-dev)

    [ ] Header-Embedded Original CRC: Embed pre-calculated original data CRC32C in the IFD1 header for instant validation on extraction.

    [ ] Hardware-Accelerated Hashing: Implement _mm_crc32_u32 for 18-bit / 20-bit pattern matching hash tables.

    [ ] AVX2 256-bit Vectorized Matching: Utilize _mm256_cmpeq_epi8 for 32-byte simultaneous match comparisons.

    [ ] Cross-Platform Linux Port: Implement POSIX mmap and pthreads support.

📄 License

This project is open-source under the terms of the Apache License 2.0. See the LICENSE file for full details.
📬 Author & Contacts

    Lead Developer: Mohammed Al-Iraqi (theeb963)

    GitHub: @theeb963

    Instagram: @sys.m2

