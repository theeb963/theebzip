/*
 * TheebZip - Universal Multi-Format Decompression Module
 * Supports: Native .theeb (Zero-Copy LZ4), RAR, ZIP, 7Z, TAR, GZ, XZ, BZ2, ISO, CAB
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
#include <immintrin.h>

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

static const char MAGIC[4] = {'I','F','D','1'};

static void* map_file_read_d(const std::string& path, HANDLE_FD& fd, size_t& size) {
#ifdef _WIN32
    int wlen = MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, nullptr, 0);
    std::wstring wpath(wlen, 0);
    MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, &wpath[0], wlen);

    fd = CreateFileW(wpath.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
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

static void* map_file_write_d(const std::string& path, HANDLE_FD& fd, size_t size) {
#ifdef _WIN32
    int wlen = MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, nullptr, 0);
    std::wstring wpath(wlen, 0);
    MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, &wpath[0], wlen);

    fd = CreateFileW(wpath.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr,
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

static void unmap_close_d(void* ptr, size_t size, HANDLE_FD fd) {
#ifdef _WIN32
    if (ptr) UnmapViewOfFile(ptr);
    if (fd != INVALID_FD) CloseHandle(fd);
#else
    if (ptr && ptr != MAP_FAILED) munmap(ptr, size);
    if (fd != INVALID_FD) close(fd);
#endif
}

static bool lz4_decompress_block(const uint8_t* src, size_t src_len,
                                 uint8_t* dst, size_t dst_len) {
    const uint8_t* ip = src;
    const uint8_t* const iend = src + src_len;
    uint8_t* op = dst;
    const uint8_t* const oend = dst + dst_len;

    while (ip < iend) {
        uint8_t token = *ip++;
        unsigned lit_len = token >> 4;
        if (lit_len == 15) { uint8_t s; do { s = *ip++; lit_len += s; } while (s == 255); }
        if (lit_len) { memcpy(op, ip, lit_len); op += lit_len; ip += lit_len; }
        if (ip >= iend) break;

        uint16_t offset; memcpy(&offset, ip, 2); ip += 2;
        unsigned match_len = (token & 0xF) + 4;
        if ((token & 0xF) == 15) { uint8_t s; do { s = *ip++; match_len += s; } while (s == 255); }

        const uint8_t* match = op - offset;
        if (offset >= 8) {
            while (match_len >= 8) {
                *(uint64_t*)op = *(const uint64_t*)match;
                op += 8; match += 8; match_len -= 8;
            }
            while (match_len--) *op++ = *match++;
        } else {
            while (match_len--) *op++ = *match++;
        }
    }
    return op == oend;
}

static uint32_t crc32c_sse42(const uint8_t* data, size_t len) {
    uint32_t crc = 0xFFFFFFFF;
    size_t i = 0;
    for (; i + 8 <= len; i += 8) crc = _mm_crc32_u64(crc, *(const uint64_t*)(data + i));
    for (; i + 4 <= len; i += 4) crc = _mm_crc32_u32(crc, *(const uint32_t*)(data + i));
    for (; i < len; ++i) crc = _mm_crc32_u8(crc, data[i]);
    return ~crc;
}

static int extract_system_archive(const std::string& in_path) {
#ifdef _WIN32
    int wlen = MultiByteToWideChar(CP_UTF8, 0, in_path.c_str(), -1, nullptr, 0);
    std::wstring win_path(wlen, 0);
    MultiByteToWideChar(CP_UTF8, 0, in_path.c_str(), -1, &win_path[0], wlen);

    size_t last_slash = win_path.find_last_of(L"\\/");
    std::wstring out_dir = (last_slash != std::wstring::npos) ? win_path.substr(0, last_slash) : L".";

    std::wstring cmd = L"tar.exe -xf \"" + win_path + L"\"";

    STARTUPINFOW si = { sizeof(si) };
    PROCESS_INFORMATION pi;
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;

    std::vector<wchar_t> cmd_buf(cmd.begin(), cmd.end());
    cmd_buf.push_back(0);

    if (CreateProcessW(nullptr, cmd_buf.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, out_dir.c_str(), &si, &pi)) {
        WaitForSingleObject(pi.hProcess, 15000); // حد أقصى للانتظار لتجنب التعليق
        DWORD exit_code = 0;
        GetExitCodeProcess(pi.hProcess, &exit_code);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        return 0;
    }
    return 1;
#else
    size_t last_slash = in_path.find_last_of("\\/");
    std::string out_dir = (last_slash != std::string::npos) ? in_path.substr(0, last_slash) : ".";
    std::string cmd = "tar -xf \"" + in_path + "\" -C \"" + out_dir + "\"";
    return (system(cmd.c_str()) == 0) ? 0 : 1;
#endif
}

int decompress_file(const std::string& in_path, const std::string& out_path) {
    HANDLE_FD in_fd = INVALID_FD;
    size_t in_size = 0;
    void* in_ptr = map_file_read_d(in_path, in_fd, in_size);
    if (!in_ptr) {
        return extract_system_archive(in_path);
    }

    const uint8_t* base = (const uint8_t*)in_ptr;

    if (in_size >= 24 && memcmp(base, MAGIC, 4) == 0) {
        uint32_t version = *(const uint32_t*)(base + 4);
        uint64_t orig_size = *(const uint64_t*)(base + 8);
        uint32_t block_size = *(const uint32_t*)(base + 16);
        uint32_t block_count = *(const uint32_t*)(base + 20);

        if (version == 1 && orig_size > 0 && block_count > 0 && block_size > 0) {
            const uint32_t* comp_sizes = (const uint32_t*)(base + 24);
            const uint8_t* comp_data = base + 24 + (size_t)block_count * 4;

            HANDLE_FD out_fd = INVALID_FD;
            void* out_ptr = map_file_write_d(out_path, out_fd, orig_size);
            if (!out_ptr) {
                unmap_close_d(in_ptr, in_size, in_fd);
                return 1;
            }

            auto t0 = std::chrono::high_resolution_clock::now();
            bool ok = true;
            std::vector<size_t> offsets(block_count + 1, 0);
            for (uint32_t i = 0; i < block_count; ++i) offsets[i+1] = offsets[i] + comp_sizes[i];

            #pragma omp parallel for schedule(dynamic, 64)
            for (int i = 0; i < (int)block_count; ++i) {
                size_t uncomp_size = (i == (int)block_count - 1) ? orig_size - (size_t)block_size * (block_count - 1) : block_size;
                uint8_t* dst = (uint8_t*)out_ptr + (size_t)i * block_size;
                const uint8_t* src = comp_data + offsets[i];
                if (!lz4_decompress_block(src, comp_sizes[i], dst, uncomp_size)) {
                    #pragma omp atomic write
                    ok = false;
                }
            }
            auto t1 = std::chrono::high_resolution_clock::now();
            double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
            double gbps = (double)orig_size / (ms * 1e-3) / 1e9;

            uint32_t crc = crc32c_sse42((const uint8_t*)out_ptr, orig_size);

            unmap_close_d(out_ptr, orig_size, out_fd);
            unmap_close_d(in_ptr, in_size, in_fd);

            std::cout << "=== TheebZip Native Zero-Copy Engine ===" << std::endl;
            std::cout << "Input: " << in_path << " (" << in_size << " bytes)" << std::endl;
            std::cout << "Output: " << out_path << " (" << orig_size << " bytes)" << std::endl;
            std::cout << "Throughput: " << gbps << " GB/s" << std::endl;
            std::cout << "CRC32C (SSE4.2): 0x" << std::hex << crc << std::dec << std::endl;
            return ok ? 0 : 1;
        }
    }

    unmap_close_d(in_ptr, in_size, in_fd);
    return extract_system_archive(in_path);
}