#pragma once

#include <cstdint>

struct ImDrawList;

namespace lit::features::esp {

void draw(ImDrawList* dl);

uint32_t with_alpha(uint32_t col, float alpha_mult);
uint32_t rainbow_color(double t);
void draw_corner_box(ImDrawList* dl, float tlx, float tly, float brx, float bry,
                      uint32_t col, float thickness);
void draw_glow(ImDrawList* dl, float tlx, float tly, float brx, float bry,
               uint32_t col, float alpha_mult);

}
