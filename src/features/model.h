#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace lit::features::model {

struct Vertex { float px, py, pz; float nx, ny, nz; float cr, cg, cb; };

struct Mesh {
    std::vector<Vertex>   verts;
    std::vector<uint32_t> indices;
    float min[3]{ 0,0,0 };
    float max[3]{ 0,0,0 };
    bool  loaded = false;
};

bool load_glb(const std::string& path, Mesh& out);

}
