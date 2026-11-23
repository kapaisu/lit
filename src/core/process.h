#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <windows.h>

namespace lit::process {

struct Handle {
    HANDLE h = nullptr;
    uint32_t pid = 0;
    uintptr_t base = 0;
    uint32_t size = 0;
};

uint32_t find_pid(std::wstring_view image_name);

bool open(Handle& out, uint32_t pid);

bool module_bounds(const Handle& proc, std::wstring_view module_name, uintptr_t& base, uint32_t& size);

std::wstring image_path(const Handle& proc);

void close(Handle& proc);

}
