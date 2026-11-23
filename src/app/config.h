#pragma once

#include <string>
#include <vector>

namespace lit::app::config {

void load();
void save();

std::vector<std::string> list_profiles();

bool save_profile(const std::string& name);
bool load_profile(const std::string& name);
bool delete_profile(const std::string& name);

}
