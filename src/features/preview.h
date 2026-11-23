#pragma once

#include <cstdint>

struct ID3D11Device;
struct ID3D11DeviceContext;
struct ID3D11ShaderResourceView;

namespace lit::features::preview {

bool init(ID3D11Device* dev, ID3D11DeviceContext* ctx);

ID3D11ShaderResourceView* render(float dt, uint32_t w, uint32_t h);

void shutdown();

}
