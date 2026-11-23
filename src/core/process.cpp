#include "process.h"

#include <tlhelp32.h>
#include <psapi.h>
#include <algorithm>

#pragma comment(lib, "psapi.lib")

namespace lit::process {

static bool iequal(std::wstring_view a, std::wstring_view b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i)
        if (::towlower(a[i]) != ::towlower(b[i])) return false;
    return true;
}

uint32_t find_pid(std::wstring_view image_name) {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return 0;

    PROCESSENTRY32W pe{};
    pe.dwSize = sizeof(pe);
    uint32_t found = 0;
    if (Process32FirstW(snap, &pe)) {
        do {
            if (iequal(pe.szExeFile, image_name)) {
                found = pe.th32ProcessID;
                break;
            }
        } while (Process32NextW(snap, &pe));
    }
    CloseHandle(snap);
    return found;
}

bool open(Handle& out, uint32_t pid) {
    if (!pid) return false;
    HANDLE h = OpenProcess(PROCESS_VM_READ | PROCESS_VM_WRITE | PROCESS_QUERY_INFORMATION, FALSE, pid);
    if (!h) return false;
    out.h = h;
    out.pid = pid;
    return true;
}

bool module_bounds(const Handle& proc, std::wstring_view module_name, uintptr_t& base, uint32_t& size) {
    if (!proc.h) return false;

    HMODULE modules[512];
    DWORD needed = 0;
    if (!EnumProcessModulesEx(proc.h, modules, sizeof(modules), &needed, LIST_MODULES_ALL))
        return false;

    const size_t count = needed / sizeof(HMODULE);
    for (size_t i = 0; i < count; ++i) {
        wchar_t name[MAX_PATH];
        if (!GetModuleBaseNameW(proc.h, modules[i], name, MAX_PATH)) continue;
        if (!iequal(name, module_name)) continue;

        MODULEINFO info{};
        if (!GetModuleInformation(proc.h, modules[i], &info, sizeof(info))) continue;
        base = reinterpret_cast<uintptr_t>(info.lpBaseOfDll);
        size = static_cast<uint32_t>(info.SizeOfImage);
        return true;
    }
    return false;
}

std::wstring image_path(const Handle& proc) {
    if (!proc.h) return {};
    wchar_t buf[MAX_PATH];
    DWORD len = MAX_PATH;

    if (!QueryFullProcessImageNameW(proc.h, 0, buf, &len)) return {};
    return std::wstring(buf, len);
}

void close(Handle& proc) {
    if (proc.h) { CloseHandle(proc.h); proc.h = nullptr; }
    proc.pid = 0;
    proc.base = 0;
    proc.size = 0;
}

}
