#include "converter.h"
#include <windows.h>
#include <shobjidl.h>
#include <filesystem>
#include <string>
namespace fs=std::filesystem;
HWND sourceBox, destBox, nameBox, logBox;
HWND testLimitBox, lowerSeaLevelBox, winterFreezeWaterBox, skyBedrockFloorBox;
std::wstring choose(bool outputFolder){
    IFileDialog* dialog = nullptr;
    // Both buttons select directories. The old implementation used
    // IFileSaveDialog for the output, which exposed a filename field and
    // prevented selecting a folder directly.
    if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr,
        CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dialog))))
        return {};

    DWORD options = 0;
    dialog->GetOptions(&options);
    dialog->SetOptions(options | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST);
    dialog->SetTitle(outputFolder ? L"选择 0.9 输出文件夹" : L"选择 UWP 世界文件夹");

    std::wstring result;
    if (SUCCEEDED(dialog->Show(nullptr))) {
        IShellItem* item = nullptr;
        if (SUCCEEDED(dialog->GetResult(&item))) {
            PWSTR path = nullptr;
            if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &path)) && path != nullptr) {
                result = path;
                CoTaskMemFree(path);
            }
            item->Release();
        }
    }
    dialog->Release();
    return result;
}
std::wstring text(HWND h){int n=GetWindowTextLengthW(h);std::wstring s(n,L'\0');GetWindowTextW(h,s.data(),n+1);return s;}
void append(const std::wstring&s){SendMessageW(logBox,EM_SETSEL,-1,-1);SendMessageW(logBox,EM_REPLACESEL,FALSE,(LPARAM)(s+L"\r\n").c_str());}
LRESULT CALLBACK proc(HWND w, UINT m, WPARAM a, LPARAM l) {
    if (m == WM_COMMAND) {
        int id = LOWORD(a);
        if (id == 1) {
            auto p = choose(false);
            if (!p.empty()) SetWindowTextW(sourceBox, p.c_str());
        }
        if (id == 2) {
            auto p = choose(true);
            if (!p.empty()) SetWindowTextW(destBox, p.c_str());
        }
        if (id == 3) {
            converter::Options o{
                fs::path(text(sourceBox)),
                fs::path(text(destBox)),
                text(nameBox)
            };
            o.maxChunks = SendMessageW(testLimitBox, BM_GETCHECK, 0, 0) == BST_CHECKED ? 64 : 0;
            o.lowerSeaLevel = SendMessageW(lowerSeaLevelBox, BM_GETCHECK, 0, 0) == BST_CHECKED;
            o.freezeWinterWater = SendMessageW(winterFreezeWaterBox, BM_GETCHECK, 0, 0) == BST_CHECKED;
            o.skyBedrockFloor = SendMessageW(skyBedrockFloorBox, BM_GETCHECK, 0, 0) == BST_CHECKED;
            std::wstring e;
            append(L"Converting…");
            if (converter::Convert(o, append, e))
                append(L"Done. Open the resulting folder with MCPE 0.9.");
            else
                append(L"Error: " + e);
        }
        return 0;
    }
    if (m == WM_DESTROY) {
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(w, m, a, l);
}
int WINAPI wWinMain(HINSTANCE h, HINSTANCE, PWSTR, int nCmdShow) {
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    HINSTANCE instance = GetModuleHandleW(nullptr);
    WNDCLASSW c{};
    c.style = CS_HREDRAW | CS_VREDRAW;
    c.hInstance = instance;
    c.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    c.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    c.lpszClassName = L"MCPE09ConverterWindowClass";
    c.lpfnWndProc = proc;
    c.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    SetLastError(ERROR_SUCCESS);
    ATOM atom = RegisterClassW(&c);
    DWORD registerError = GetLastError();
    if (!atom && registerError != ERROR_CLASS_ALREADY_EXISTS) {
        wchar_t msg[160];
        wsprintfW(msg, L"无法注册转换器窗口类（错误码 %lu）。", registerError);
        MessageBoxW(nullptr, msg, L"MCPE 转换器", MB_ICONERROR);
        return 1;
    }

    SetLastError(ERROR_SUCCESS);
    HWND w = CreateWindowExW(WS_EX_APPWINDOW, c.lpszClassName,
        L"MCPE 0.6.1 UWP → 0.9 Converter", WS_OVERLAPPEDWINDOW,
        200, 200, 720, 530, nullptr, nullptr, instance, nullptr);
    DWORD createError = GetLastError();
    if (!w) {
        wchar_t msg[160];
        wsprintfW(msg, L"无法创建转换器窗口（错误码 %lu）。", createError);
        MessageBoxW(nullptr, msg, L"MCPE 转换器", MB_ICONERROR);
        return 1;
    }

    auto label = [&](const wchar_t* s, int y) {
        CreateWindowExW(0, L"STATIC", s, WS_CHILD | WS_VISIBLE,
            20, y, 130, 22, w, nullptr, instance, nullptr);
    };
    auto edit = [&](int y) {
        return CreateWindowExW(0, L"EDIT", L"",
            WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
            150, y, 430, 24, w, nullptr, instance, nullptr);
    };
    label(L"UWP source world", 20);
    sourceBox = edit(20);
    CreateWindowExW(0, L"BUTTON", L"Browse…", WS_CHILD | WS_VISIBLE,
        590, 20, 100, 24, w, (HMENU)1, instance, nullptr);
    label(L"New 0.9 folder", 56);
    destBox = edit(56);
    CreateWindowExW(0, L"BUTTON", L"Browse…", WS_CHILD | WS_VISIBLE,
        590, 56, 100, 24, w, (HMENU)2, instance, nullptr);
    label(L"World name", 92);
    nameBox = edit(92);
    SetWindowTextW(nameBox, L"Converted UWP World");
    CreateWindowExW(0, L"BUTTON", L"Convert to 0.9", WS_CHILD | WS_VISIBLE,
        20, 132, 180, 30, w, (HMENU)3, instance, nullptr);
    testLimitBox = CreateWindowExW(0, L"BUTTON",
        L"仅转换出生点附近最多 64 个区块（测试）",
        WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
        220, 136, 380, 24, w, (HMENU)4, instance, nullptr);
    lowerSeaLevelBox = CreateWindowExW(0, L"BUTTON",
        L"降低海平面：清除 y=63 水/冰，水下泥土变为草",
        WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
        220, 166, 430, 24, w, (HMENU)5, instance, nullptr);
    winterFreezeWaterBox = CreateWindowExW(0, L"BUTTON",
        L"Winter 专用：将 y=62 水冻结为冰（需同时降低海平面）",
        WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
        220, 196, 480, 24, w, (HMENU)6, instance, nullptr);
    skyBedrockFloorBox = CreateWindowExW(0, L"BUTTON",
        L"天域专用：将 y=0-4 的空气替换为基岩",
        WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
        220, 226, 430, 24, w, (HMENU)7, instance, nullptr);
    logBox = CreateWindowExW(0, L"EDIT",
        L"Choose a UWP V2 infinite, Sky, Alpha, or StorageVersion 3 finite world. The output folder must not already exist.\r\n"
        L"Full conversion is enabled by default; check the 64-chunk test limit only for a small validation run.",
        WS_CHILD | WS_VISIBLE | WS_BORDER | ES_MULTILINE | ES_READONLY | WS_VSCROLL,
        20, 266, 670, 190, w, nullptr, instance, nullptr);
    ShowWindow(w, nCmdShow == SW_HIDE ? SW_SHOWNORMAL : nCmdShow);
    UpdateWindow(w);
    MSG x;
    while (GetMessageW(&x, nullptr, 0, 0) > 0) {
        TranslateMessage(&x);
        DispatchMessageW(&x);
    }
    CoUninitialize();
    return 0;
}
