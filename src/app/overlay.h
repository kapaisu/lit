#pragma once

#include <windows.h>

namespace lit::app::overlay {

void init(HWND self);

void tick();

bool menu_visible();
void set_menu_visible(bool v);

int  menu_key();
void set_menu_key(int vk);

}
