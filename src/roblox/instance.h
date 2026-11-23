#pragma once

#include "roblox.h"
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace lit::roblox::instance {

std::string class_name(uintptr_t inst);

std::string name(uintptr_t inst);

std::vector<uintptr_t> children(uintptr_t inst, size_t max_children = 4096);

uintptr_t find_child_by_name (uintptr_t inst, std::string_view target);
uintptr_t find_child_by_class(uintptr_t inst, std::string_view target);

}
