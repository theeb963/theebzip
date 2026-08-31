/*
 * TheebZip - Hybrid Launcher & Multilingual Native GUI
 * Copyright (c) 2026 Mohammed Al-Iraqi (theeb963). All rights reserved.
 * GitHub: https://github.com/theeb963 | Instagram: @sys.m2
 * Licensed under the MIT License.
 */

#include <iostream>
#include <string>
#include <cstring>
#include <cstdio>
#include <thread>

extern int compress_file(const std::string& in_path, const std::string& out_path);
extern int decompress_file(const std::string& in_path, const std::string& out_path);

#ifdef _WIN32
#include <windows.h>
#include <commdlg.h>
#include <shellapi.h>

static HWND g_hwnd = nullptr;
static HWND g_btn_compress = nullptr;
static HWND g_btn_decompress = nullptr;
static HWND g_btn_cli = nullptr;
static HWND g_btn_lang = nullptr;

static bool g_arabic_mode = true;

static void update_ui_language() {
    if (g_arabic_mode) {
        SetWindowTextW(g_hwnd, L"محرك TheebZip للضغط");
        SetWindowTextW(g_btn_compress, L"ضغط ملف");
        SetWindowTextW(g_btn_decompress, L"فك ضغط ملف");
        SetWindowTextW(g_btn_cli, L"فتح الطرفية (CLI)");
        SetWindowTextW(g_btn_lang, L"English");
    } else {
        SetWindowTextW(g_hwnd, L"TheebZip Fast Engine");
        SetWindowTextW(g_btn_compress, L"Compress File");
        SetWindowTextW(g_btn_decompress, L"Decompress File");
        SetWindowTextW(g_btn_cli, L"Open CLI Terminal");
        SetWindowTextW(g_btn_lang, L"العربية");
    }
}

static void show_file_picker(bool is_compress_mode) {
    OPENFILENAMEW ofn;
    wchar_t file_buffer[1024] = {0};
    ZeroMemory(&ofn, sizeof(ofn));
    
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = g_hwnd;
    ofn.lpstrFile = file_buffer;
    ofn.nMaxFile = sizeof(file_buffer) / sizeof(wchar_t);
    ofn.lpstrFilter = is_compress_mode ? L"All Files (*.*)\0*.*\0" : L"All Supported Archives (*.theeb;*.rar;*.zip;*.7z;*.tar;*.gz;*.iso)\0*.theeb;*.rar;*.zip;*.7z;*.tar;*.gz;*.iso\0All Files (*.*)\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;

    if (GetOpenFileNameW(&ofn)) {
        int size_needed = WideCharToMultiByte(CP_UTF8, 0, file_buffer, -1, NULL, 0, NULL, NULL);
        std::string in_path(size_needed - 1, 0);
        WideCharToMultiByte(CP_UTF8, 0, file_buffer, -1, &in_path[0], size_needed, NULL, NULL);
        
        std::thread([in_path, is_compress_mode]() {
            EnableWindow(g_btn_compress, FALSE);
            EnableWindow(g_btn_decompress, FALSE);

            if (g_arabic_mode) {
                SetWindowTextW(g_hwnd, L"TheebZip (جاري المعالجة...)");
            } else {
                SetWindowTextW(g_hwnd, L"TheebZip (Processing...)");
            }

            if (is_compress_mode) {
                std::string out_path = in_path + ".theeb";
                int res = compress_file(in_path, out_path);
                
                EnableWindow(g_btn_compress, TRUE);
                EnableWindow(g_btn_decompress, TRUE);
                update_ui_language();

                if (res == 0) {
                    if (g_arabic_mode) {
                        std::wstring wout(out_path.begin(), out_path.end());
                        MessageBoxW(g_hwnd, (L"تم اكتمال الضغط بنجاح!\nالملف المحفوظ:\n" + wout).c_str(), L"TheebZip", MB_OK | MB_ICONINFORMATION | MB_SETFOREGROUND);
                    } else {
                        MessageBoxA(g_hwnd, ("Compression Complete!\nSaved to:\n" + out_path).c_str(), "TheebZip", MB_OK | MB_ICONINFORMATION | MB_SETFOREGROUND);
                    }
                } else {
                    if (g_arabic_mode) {
                        MessageBoxW(g_hwnd, L"حدث خطأ أثناء عملية الضغط!", L"خطأ", MB_OK | MB_ICONERROR | MB_SETFOREGROUND);
                    } else {
                        MessageBoxA(g_hwnd, "Compression Failed!", "TheebZip Error", MB_OK | MB_ICONERROR | MB_SETFOREGROUND);
                    }
                }
            } else {
                std::string out_path;
                if (in_path.size() > 6 && in_path.substr(in_path.size() - 6) == ".theeb") {
                    out_path = in_path.substr(0, in_path.size() - 6);
                } else {
                    out_path = in_path + ".extracted";
                }

                DWORD attr = GetFileAttributesA(out_path.c_str());
                if (attr != INVALID_FILE_ATTRIBUTES) {
                    out_path += ".restored";
                }

                int res = decompress_file(in_path, out_path);
                
                EnableWindow(g_btn_compress, TRUE);
                EnableWindow(g_btn_decompress, TRUE);
                update_ui_language();

                if (res == 0) {
                    if (g_arabic_mode) {
                        MessageBoxW(g_hwnd, L"تم استخراج الأرشيف بنجاح في مكان الملف!", L"TheebZip", MB_OK | MB_ICONINFORMATION | MB_SETFOREGROUND);
                    } else {
                        MessageBoxA(g_hwnd, "Extraction completed successfully!", "TheebZip", MB_OK | MB_ICONINFORMATION | MB_SETFOREGROUND);
                    }
                } else {
                    if (g_arabic_mode) {
                        MessageBoxW(g_hwnd, L"فشل فك الضغط: الأرشيف تالف أو الصيغة غير مدعومة.", L"خطأ", MB_OK | MB_ICONERROR | MB_SETFOREGROUND);
                    } else {
                        MessageBoxA(g_hwnd, "Decompression Failed: Corrupted archive.", "TheebZip Error", MB_OK | MB_ICONERROR | MB_SETFOREGROUND);
                    }
                }
            }
        }).detach();
    }
}

static void launch_cli_console() {
    ShellExecuteA(nullptr, "open", "cmd.exe", "/k title TheebZip CLI Engine && echo === TheebZip CLI Interactive Terminal === && echo Usage: theebzip -c ^<input^> ^[output.theeb^] ^| theebzip -d ^<input.theeb^> ^[output^] && echo.", nullptr, SW_SHOW);
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE: {
        HFONT hFont = CreateFontW(15, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                                  OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                                  VARIABLE_PITCH, L"Segoe UI");

        g_btn_compress = CreateWindowW(L"BUTTON", L"ضغط ملف", WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
                                       20, 15, 200, 34, hwnd, (HMENU)1, nullptr, nullptr);
        g_btn_decompress = CreateWindowW(L"BUTTON", L"فك ضغط ملف", WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
                                         20, 56, 200, 34, hwnd, (HMENU)2, nullptr, nullptr);
        g_btn_cli = CreateWindowW(L"BUTTON", L"فتح الطرفية (CLI)", WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
                                  20, 97, 200, 34, hwnd, (HMENU)3, nullptr, nullptr);
        g_btn_lang = CreateWindowW(L"BUTTON", L"English", WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
                                   20, 138, 200, 30, hwnd, (HMENU)4, nullptr, nullptr);

        SendMessage(g_btn_compress, WM_SETFONT, (WPARAM)hFont, TRUE);
        SendMessage(g_btn_decompress, WM_SETFONT, (WPARAM)hFont, TRUE);
        SendMessage(g_btn_cli, WM_SETFONT, (WPARAM)hFont, TRUE);
        SendMessage(g_btn_lang, WM_SETFONT, (WPARAM)hFont, TRUE);
        return 0;
    }
    case WM_COMMAND:
        if (LOWORD(wParam) == 1) {
            show_file_picker(true);
        } else if (LOWORD(wParam) == 2) {
            show_file_picker(false);
        } else if (LOWORD(wParam) == 3) {
            launch_cli_console();
        } else if (LOWORD(wParam) == 4) {
            g_arabic_mode = !g_arabic_mode;
            update_ui_language();
        }
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

static void run_gui_mode() {
    HINSTANCE hInst = GetModuleHandleW(nullptr);
    WNDCLASSW wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.lpszClassName = L"TheebZipMainWindow";
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    RegisterClassW(&wc);

    g_hwnd = CreateWindowExW(0, wc.lpszClassName, L"محرك TheebZip للضغط",
                             WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_VISIBLE,
                             CW_USEDEFAULT, CW_USEDEFAULT, 255, 225,
                             nullptr, nullptr, hInst, nullptr);
    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
}
#endif

int main(int argc, char* argv[]) {
    if (argc == 1) {
#ifdef _WIN32
        run_gui_mode();
        return 0;
#else
        fprintf(stderr, "TheebZip Engine\nUsage: %s -c <input> [output.theeb] | -d <input.theeb> [output]\n", argv[0]);
        return 1;
#endif
    }

    if (argc >= 2 && strcmp(argv[1], "-c") == 0) {
        if (argc < 3) return 1;
        std::string in = argv[2];
        std::string out = (argc >= 4) ? argv[3] : in + ".theeb";
        return compress_file(in, out);
    } else if (argc >= 2 && strcmp(argv[1], "-d") == 0) {
        if (argc < 3) return 1;
        std::string in = argv[2];
        std::string out = (argc >= 4) ? argv[3] : (in + ".out");
        return decompress_file(in, out);
    }
    return 1;
}