#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace lit::roblox::offsets {

enum class Source { None, Cache, Network };

struct Status {
    Source      source = Source::None;
    std::string version;
    std::string error;
    uint32_t    count = 0;
};

void refresh(bool force_network = false, std::string_view explicit_version = {});

uint32_t get(std::string_view klass, std::string_view field);

uint32_t get_or(std::string_view klass, std::string_view field, uint32_t fallback);

const Status& status();

std::string detect_roblox_version();

}
