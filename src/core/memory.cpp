#include "memory.h"

#include <windows.h>
#include <vector>

namespace lit::memory {

bool read_raw(const process::Handle& p, uintptr_t addr, void* out, size_t bytes) {
    if (!p.h || !addr) return false;
    SIZE_T got = 0;
    return ReadProcessMemory(p.h, reinterpret_cast<LPCVOID>(addr), out, bytes, &got) && got == bytes;
}

bool write_raw(const process::Handle& p, uintptr_t addr, const void* in, size_t bytes) {
    if (!p.h || !addr) return false;
    SIZE_T wrote = 0;
    return WriteProcessMemory(p.h, reinterpret_cast<LPVOID>(addr), in, bytes, &wrote) && wrote == bytes;
}

uintptr_t chain(const process::Handle& p, uintptr_t base, std::initializer_list<uintptr_t> offsets) {
    uintptr_t cur = base;
    for (auto off : offsets) {
        if (!cur) return 0;
        cur = read<uintptr_t>(p, cur + off);
    }
    return cur;
}

static bool parse_pattern(const char* pattern, std::vector<int>& out) {
    out.clear();
    for (const char* c = pattern; *c;) {
        while (*c == ' ') ++c;
        if (!*c) break;
        if (*c == '?') {
            out.push_back(-1);
            ++c;
            if (*c == '?') ++c;
        } else {
            char buf[3] = { c[0], c[1], 0 };
            out.push_back(static_cast<int>(strtoul(buf, nullptr, 16)));
            c += 2;
        }
    }
    return !out.empty();
}

uintptr_t scan(const process::Handle& p, uintptr_t base, uint32_t size, const char* pattern) {
    std::vector<int> pat;
    if (!parse_pattern(pattern, pat)) return 0;

    constexpr size_t chunk = 0x10000;
    std::vector<uint8_t> buf(chunk + pat.size());

    for (uint32_t off = 0; off < size; ) {
        const size_t take = std::min<size_t>(chunk + pat.size(), size - off);
        if (!read_raw(p, base + off, buf.data(), take)) { off += chunk; continue; }

        const size_t end = (take >= pat.size()) ? take - pat.size() : 0;
        for (size_t i = 0; i < end; ++i) {
            bool hit = true;
            for (size_t j = 0; j < pat.size(); ++j) {
                if (pat[j] != -1 && pat[j] != buf[i + j]) { hit = false; break; }
            }
            if (hit) return base + off + i;
        }
        off += static_cast<uint32_t>(chunk);
    }
    return 0;
}

}
