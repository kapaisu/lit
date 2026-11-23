#define CGLTF_IMPLEMENTATION
#include <cgltf/cgltf.h>

#include "model.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>

namespace lit::features::model {

namespace {

void mat_identity(float m[16]) {
    std::memset(m, 0, sizeof(float) * 16);
    m[0] = m[5] = m[10] = m[15] = 1.f;
}
void mat_mul(float o[16], const float a[16], const float b[16]) {
    float t[16];
    for (int c = 0; c < 4; ++c)
        for (int r = 0; r < 4; ++r) {
            float v = 0.f;
            for (int k = 0; k < 4; ++k) v += a[k * 4 + r] * b[c * 4 + k];
            t[c * 4 + r] = v;
        }
    std::memcpy(o, t, sizeof(t));
}
void transform_point(const float m[16], float x, float y, float z,
                     float& ox, float& oy, float& oz) {
    ox = m[0]*x + m[4]*y + m[8] *z + m[12];
    oy = m[1]*x + m[5]*y + m[9] *z + m[13];
    oz = m[2]*x + m[6]*y + m[10]*z + m[14];
}

void transform_normal(const float m[16], float x, float y, float z,
                      float& ox, float& oy, float& oz) {
    ox = m[0]*x + m[4]*y + m[8] *z;
    oy = m[1]*x + m[5]*y + m[9] *z;
    oz = m[2]*x + m[6]*y + m[10]*z;
    const float l = std::sqrt(ox*ox + oy*oy + oz*oz);
    if (l > 1e-6f) { ox /= l; oy /= l; oz /= l; }
}

void get_local_matrix(const cgltf_node* n, float out[16]) {
    cgltf_node_transform_local(n, out);
}

void append_primitive(const cgltf_primitive* prim, const float world[16],
                      const float tint[3],
                      std::vector<Vertex>& verts, std::vector<uint32_t>& indices,
                      float mn[3], float mx[3])
{
    if (!prim || prim->type != cgltf_primitive_type_triangles) return;

    const cgltf_accessor* apos  = nullptr;
    const cgltf_accessor* anorm = nullptr;
    const cgltf_accessor* acol  = nullptr;
    for (cgltf_size a = 0; a < prim->attributes_count; ++a) {
        const cgltf_attribute& at = prim->attributes[a];
        if (at.type == cgltf_attribute_type_position && at.index == 0) apos  = at.data;
        if (at.type == cgltf_attribute_type_normal   && at.index == 0) anorm = at.data;
        if (at.type == cgltf_attribute_type_color    && at.index == 0) acol  = at.data;
    }
    if (!apos) return;

    const cgltf_size vcount = apos->count;
    const uint32_t base = (uint32_t)verts.size();
    verts.reserve(verts.size() + vcount);

    for (cgltf_size i = 0; i < vcount; ++i) {
        float p[3]{}, n[3]{ 0, 1, 0 }, c[4]{ tint[0], tint[1], tint[2], 1.f };
        cgltf_accessor_read_float(apos, i, p, 3);
        if (anorm) cgltf_accessor_read_float(anorm, i, n, 3);
        if (acol)  cgltf_accessor_read_float(acol, i, c, 4);

        Vertex v{};
        transform_point (world, p[0], p[1], p[2], v.px, v.py, v.pz);
        transform_normal(world, n[0], n[1], n[2], v.nx, v.ny, v.nz);
        v.cr = c[0]; v.cg = c[1]; v.cb = c[2];
        verts.push_back(v);

        if (v.px < mn[0]) mn[0] = v.px; if (v.px > mx[0]) mx[0] = v.px;
        if (v.py < mn[1]) mn[1] = v.py; if (v.py > mx[1]) mx[1] = v.py;
        if (v.pz < mn[2]) mn[2] = v.pz; if (v.pz > mx[2]) mx[2] = v.pz;
    }

    if (prim->indices) {
        const cgltf_size ic = prim->indices->count;
        indices.reserve(indices.size() + ic);
        for (cgltf_size i = 0; i < ic; ++i) {
            const cgltf_size idx = cgltf_accessor_read_index(prim->indices, i);
            indices.push_back(base + (uint32_t)idx);
        }
    } else {

        indices.reserve(indices.size() + vcount);
        for (cgltf_size i = 0; i < vcount; ++i) indices.push_back(base + (uint32_t)i);
    }
}

void walk_node(const cgltf_node* n, const float parent[16],
               std::vector<Vertex>& verts, std::vector<uint32_t>& indices,
               float mn[3], float mx[3])
{
    float local[16]; get_local_matrix(n, local);
    float world[16]; mat_mul(world, parent, local);

    if (n->mesh) {

        for (cgltf_size p = 0; p < n->mesh->primitives_count; ++p) {
            const cgltf_primitive* prim = &n->mesh->primitives[p];
            float tint[3] = { 0.85f, 0.85f, 0.88f };
            if (prim->material && prim->material->has_pbr_metallic_roughness) {
                const float* bc = prim->material->pbr_metallic_roughness.base_color_factor;
                tint[0] = bc[0]; tint[1] = bc[1]; tint[2] = bc[2];
            }
            append_primitive(prim, world, tint, verts, indices, mn, mx);
        }
    }

    for (cgltf_size i = 0; i < n->children_count; ++i)
        walk_node(n->children[i], world, verts, indices, mn, mx);
}

}

bool load_glb(const std::string& path, Mesh& out) {
    out = Mesh{};

    cgltf_options opt{};
    cgltf_data* data = nullptr;
    if (cgltf_parse_file(&opt, path.c_str(), &data) != cgltf_result_success)
        return false;
    if (cgltf_load_buffers(&opt, data, path.c_str()) != cgltf_result_success) {
        cgltf_free(data);
        return false;
    }

    float mn[3] = { std::numeric_limits<float>::infinity(),
                    std::numeric_limits<float>::infinity(),
                    std::numeric_limits<float>::infinity() };
    float mx[3] = { -mn[0], -mn[1], -mn[2] };

    float ident[16]; mat_identity(ident);
    const cgltf_scene* scene = data->scene ? data->scene :
        (data->scenes_count ? &data->scenes[0] : nullptr);
    if (scene) {
        for (cgltf_size i = 0; i < scene->nodes_count; ++i)
            walk_node(scene->nodes[i], ident, out.verts, out.indices, mn, mx);
    } else {
        for (cgltf_size i = 0; i < data->nodes_count; ++i)
            walk_node(&data->nodes[i], ident, out.verts, out.indices, mn, mx);
    }

    cgltf_free(data);

    if (out.verts.empty() || out.indices.empty()) return false;

    std::memcpy(out.min, mn, sizeof(mn));
    std::memcpy(out.max, mx, sizeof(mx));
    out.loaded = true;
    return true;
}

}
