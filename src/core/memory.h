#pragma once

#include "process.h"
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

namespace lit::memory {

bool read_raw(const process::Handle& p, uintptr_t addr, void* out, size_t bytes);
bool write_raw(const process::Handle& p, uintptr_t addr, const void* in, size_t bytes);

template <typename T>
T read(const process::Handle& p, uintptr_t addr) {
    T v{};
    read_raw(p, addr, &v, sizeof(T));
    return v;
}

template <typename T>
bool write(const process::Handle& p, uintptr_t addr, const T& value) {
    return write_raw(p, addr, &value, sizeof(T));
}

uintptr_t chain(const process::Handle& p, uintptr_t base, std::initializer_list<uintptr_t> offsets);

uintptr_t scan(const process::Handle& p, uintptr_t base, uint32_t size, const char* pattern);

}
