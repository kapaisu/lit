#include "instance.h"
#include "offsets.h"
#include "../core/memory.h"

#include <array>

namespace lit::roblox::instance {

namespace {

std::string read_std_string(uintptr_t addr) {
    if (!addr) return {};
    std::array<uint8_t, 32> raw{};
    if (!memory::read_raw(ctx().proc, addr, raw.data(), raw.size())) return {};

    const size_t size = *reinterpret_cast<size_t*>(raw.data() + 16);
    const size_t cap  = *reinterpret_cast<size_t*>(raw.data() + 24);
    if (size == 0 || size > 4096) return {};

    if (cap > 15) {
        const uintptr_t ptr = *reinterpret_cast<uintptr_t*>(raw.data());
        if (!ptr) return {};
        std::string out(size, '\0');
        if (!memory::read_raw(ctx().proc, ptr, out.data(), size)) return {};
        return out;
    }
    return std::string(reinterpret_cast<const char*>(raw.data()), size);
}

}

std::string class_name(uintptr_t inst) {
    if (!inst) return {};
    const uint32_t o_desc = offsets::get("Instance", "ClassDescriptor");
    const uint32_t o_name = offsets::get("Instance", "ClassName");
    if (!o_desc || !o_name) return {};

    const uintptr_t desc = memory::read<uintptr_t>(ctx().proc, inst + o_desc);
    if (!desc) return {};

    auto s = read_std_string(desc + o_name);
    if (!s.empty()) return s;

    struct { uintptr_t ptr; size_t len; } sv{};
    if (memory::read_raw(ctx().proc, desc + o_name, &sv, sizeof(sv))
        && sv.ptr && sv.len > 0 && sv.len < 4096)
    {
        std::string out(sv.len, '\0');
        if (memory::read_raw(ctx().proc, sv.ptr, out.data(), sv.len))
            return out;
    }

    const uintptr_t cstr = memory::read<uintptr_t>(ctx().proc, desc + o_name);
    if (cstr) {
        char buf[128]{};
        if (memory::read_raw(ctx().proc, cstr, buf, sizeof(buf) - 1)) {

            if (buf[0] >= 0x20 && buf[0] < 0x7f) return std::string(buf);
        }
    }
    return {};
}

std::string name(uintptr_t inst) {
    if (!inst) return {};
    const uint32_t o_nc = offsets::get("Instance", "NameContainer");

    if (!o_nc) return {};
    const uint32_t o_n = offsets::get("Instance", "Name");

    const uintptr_t nc = memory::read<uintptr_t>(ctx().proc, inst + o_nc);
    if (!nc) return {};
    return read_std_string(nc + o_n);
}

std::vector<uintptr_t> children(uintptr_t inst, size_t max_children) {
    std::vector<uintptr_t> out;
    if (!inst) return out;

    const uint32_t o_cs = offsets::get("Instance", "ChildrenStart");
    const uint32_t o_ce = offsets::get("Instance", "ChildrenEnd");
    if (!o_cs || !o_ce) return out;

    const uintptr_t container = memory::read<uintptr_t>(ctx().proc, inst + o_cs);
    if (!container) return out;

    const uintptr_t begin = memory::read<uintptr_t>(ctx().proc, container + 0);
    const uintptr_t end   = memory::read<uintptr_t>(ctx().proc, container + o_ce);
    if (!begin || end < begin) return out;

    constexpr size_t slot = 16;
    const size_t count = (end - begin) / slot;
    if (count == 0 || count > max_children) return out;

    out.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        const uintptr_t child = memory::read<uintptr_t>(ctx().proc, begin + i * slot);
        if (child) out.push_back(child);
    }
    return out;
}

uintptr_t find_child_by_name(uintptr_t inst, std::string_view target) {
    for (auto c : children(inst))
        if (name(c) == target) return c;
    return 0;
}

uintptr_t find_child_by_class(uintptr_t inst, std::string_view target) {
    for (auto c : children(inst))
        if (class_name(c) == target) return c;
    return 0;
}

}
