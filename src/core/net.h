#pragma once

#include <string>
#include <string_view>

namespace lit::net {

std::string http_get(std::wstring_view url, std::wstring_view user_agent = L"lit/0.1");

}
