/*
 * TheebZip - Hardware-Accelerated Compression Module
 * Copyright (c) 2026 Mohammed Al-Iraqi (theeb963). All rights reserved.
 * GitHub: https://github.com/theeb963 | Instagram: @sys.m2
 * Licensed under the MIT License.
 */

#include <iostream>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <chrono>
#include <string>
#include <vector>
#include <algorithm>

#ifdef _OPENMP
#include <omp.h>
#endif

#ifdef _WIN32
#include <windows.h>
using HANDLE_FD = HANDLE;
#define INVALID_FD INVALID_HANDLE_VALUE
#else
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
using HANDLE_FD = int;
#define INVALID_FD -1
#endif

static constexpr uint32_t BLOCK_SIZE = 64 * 1024;
static const char MAGIC[4] = {'I','F','D','1'};

static void* map_file_read_c(const std::string& path, HANDLE_FD& fd, size_t& size) {
#ifdef _WIN32
    fd = CreateFileA(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                     OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (fd == INVALID_FD) return nullptr;
    LARGE_INTEGER li;
    if (!GetFileSizeEx(fd, &li)) { CloseHandle(fd); return nullptr; }
    size = (size_t)li.QuadPart;
    HANDLE map = CreateFileMappingA(fd, nullptr, PAGE_READONLY, 0, 0, nullptr);
    if (!map) { CloseHandle(fd); return nullptr; }
    void* ptr = MapViewOfFile(map, FILE_MAP_READ, 0, 0, 0);
    CloseHandle(map);
    return ptr;
#else
    fd = open(path.c_str(), O_RDONLY);
    if (fd == INVALID_FD) return nullptr;
    struct stat st;
    if (fstat(fd, &st) != 0) { close(fd); return nullptr; }
    size = st.st_size;
    void* ptr = mmap(nullptr, size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (ptr == MAP_FAILED) { close(fd); return nullptr; }
    return ptr;
#endif
}

static void* map_file_write_c(const std::string& path, HANDLE_FD& fd, size_t size) {
#ifdef _WIN32
    fd = CreateFileA(path.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr,
                     CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (fd == INVALID_FD) return nullptr;
    LARGE_INTEGER li; li.QuadPart = (LONGLONG)size;
    if (!SetFilePointerEx(fd, li, nullptr, FILE_BEGIN) || !SetEndOfFile(fd)) {
        CloseHandle(fd); return nullptr;
    }
    HANDLE map = CreateFileMappingA(fd, nullptr, PAGE_READWRITE, 0, 0, nullptr);
    if (!map) { CloseHandle(fd); return nullptr; }
    void* ptr = MapViewOfFile(map, FILE_MAP_ALL_ACCESS, 0, 0, 0);
    CloseHandle(map);
    return ptr;
#else
    fd = open(path.c_str(), O_RDWR | O_CREAT | O_TRUNC, 0644);
    if (fd == INVALID_FD) return nullptr;
    if (ftruncate(fd, size) != 0) { close(fd); return nullptr; }
    void* ptr = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (ptr == MAP_FAILED) { close(fd); return nullptr; }
    return ptr;
#endif
}

static void unmap_close_c(void* ptr, size_t size, HANDLE_FD fd) {
#ifdef _WIN32
    if (ptr) UnmapViewOfFile(ptr);
    if (fd != INVALID_FD) CloseHandle(fd);
#else
    if (ptr && ptr != MAP_FAILED) munmap(ptr, size);
    if (fd != INVALID_FD) close(fd);
#endif
}

static size_t lz4_compress_block(const uint8_t* src, size_t src_len,
                                 uint8_t* dst, size_t dst_capacity) {
    uint16_t table[65536];
    memset(table, 0xFF, sizeof(table));
    const uint8_t* ip = src;
    const uint8_t* const iend = src + src_len;
    uint8_t* op = dst;
    const uint8_t* anchor = src;

    auto emit_length = [&](size_t len) {
        while (len >= 255) { *op++ = 255; len -= 255; }
        *op++ = (uint8_t)len;
    };

    while (ip < iend - 4) {
        uint32_t v = *(const uint32_t*)ip;
        uint32_t h = (v * 2654435761u) >> 16;
        uint16_t candidate = table[h];
        table[h] = (uint16_t)(ip - src);

        if (candidate != 0xFFFF && (ip - src - candidate) <= 65535) {
            const uint8_t* ref = src + candidate;
            if (*(const uint32_t*)ip == *(const uint32_t*)ref) {
                size_t match_len = 4;
                while (ip + match_len < iend && ref + match_len < ip &&
                       ip[match_len] == ref[match_len])
                    ++match_len;
                if (match_len > 255 + 4) match_len = 255 + 4;

                size_t lit_len = ip - anchor;
                if (lit_len >= 15) {
                    *op++ = (15 << 4) | ((match_len - 4) < 15 ? (match_len - 4) : 15);
                    emit_length(lit_len - 15);
                } else {
                    *op++ = (uint8_t)((lit_len << 4) | ((match_len - 4) < 15 ? (match_len - 4) : 15));
                }
                if (lit_len) { memcpy(op, anchor, lit_len); op += lit_len; }
                uint16_t offset = (uint16_t)(ip - src - candidate);
                memcpy(op, &offset, 2); op += 2;
                if (match_len - 4 >= 15) emit_length(match_len - 4 - 15);

                ip += match_len;
                anchor = ip;
                continue;
            }
        }
        ++ip;
    }

    size_t lit_len = iend - anchor;
    if (lit_len > 0) {
        if (lit_len >= 15) { *op++ = (15 << 4); emit_length(lit_len - 15); }
        else *op++ = (uint8_t)(lit_len << 4);
        memcpy(op, anchor, lit_len); op += lit_len;
    }
    return (size_t)(op - dst);
}

int compress_file(const std::string& in_path, const std::string& out_path) {
    HANDLE_FD in_fd = INVALID_FD;
    size_t in_size = 0;
    void* in_ptr = map_file_read_c(in_path, in_fd, in_size);
    if (!in_ptr) return 1;

    uint32_t block_count = (uint32_t)((in_size + BLOCK_SIZE - 1) / BLOCK_SIZE);
    if (block_count == 0) block_count = 1;

    // تخصيص مساحة مستمرة واحدة فقط بدلاً من آلاف الـ vectors المنفصلة
    size_t max_block_csize = BLOCK_SIZE + BLOCK_SIZE / 255 + 16;
    std::vector<uint8_t> contiguous_buffer((size_t)block_count * max_block_csize);
    std::vector<uint32_t> comp_sizes(block_count);

    auto t0 = std::chrono::high_resolution_clock::now();

    #pragma omp parallel for schedule(dynamic, 64)
    for (int i = 0; i < (int)block_count; ++i) {
        size_t offset = (size_t)i * BLOCK_SIZE;
        size_t len = std::min((size_t)BLOCK_SIZE, in_size - offset);
        const uint8_t* src = (const uint8_t*)in_ptr + offset;
        uint8_t* dst = contiguous_buffer.data() + (size_t)i * max_block_csize;

        comp_sizes[i] = (uint32_t)lz4_compress_block(src, len, dst, max_block_csize);
    }

    auto t1 = std::chrono::high_resolution_clock::now();
    double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    double gbps = (double)in_size / (ms * 1e-3) / 1e9;

    uint64_t total_compressed = 0;
    for (uint32_t s : comp_sizes) total_compressed += s;
    size_t header_size = 4 + 4 + 8 + 4 + 4 + (size_t)block_count * 4;
    size_t out_size = header_size + total_compressed;

    HANDLE_FD out_fd = INVALID_FD;
    void* out_ptr = map_file_write_c(out_path, out_fd, out_size);
    if (!out_ptr) { unmap_close_c(in_ptr, in_size, in_fd); return 1; }

    uint8_t* out = (uint8_t*)out_ptr;
    memcpy(out, MAGIC, 4); out += 4;
    *(uint32_t*)out = 1; out += 4;
    *(uint64_t*)out = (uint64_t)in_size; out += 8;
    *(uint32_t*)out = BLOCK_SIZE; out += 4;
    *(uint32_t*)out = block_count; out += 4;
    for (uint32_t i = 0; i < block_count; ++i) { *(uint32_t*)out = comp_sizes[i]; out += 4; }
    
    for (uint32_t i = 0; i < block_count; ++i) {
        uint8_t* src_blk = contiguous_buffer.data() + (size_t)i * max_block_csize;
        memcpy(out, src_blk, comp_sizes[i]);
        out += comp_sizes[i];
    }

    unmap_close_c(out_ptr, out_size, out_fd);
    unmap_close_c(in_ptr, in_size, in_fd);

    double ratio = (double)total_compressed * 100.0 / (double)in_size;
    std::cout << "=== TheebZip Compression Dashboard ===" << std::endl;
    std::cout << "Input: " << in_path << " (" << in_size << " bytes)" << std::endl;
    std::cout << "Output: " << out_path << " (" << out_size << " bytes)" << std::endl;
    std::cout << "Throughput: " << gbps << " GB/s" << std::endl;
    return 0;
}