#define NOMINMAX
#include "preview.h"
#include "model.h"

#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <vector>

#pragma comment(lib, "d3dcompiler.lib")

namespace lit::features::preview {

namespace {

constexpr const char* kHLSL = R"(
cbuffer CB : register(b0) {
    row_major float4x4 mvp;
    float4 light_dir;
};
struct VSIn  { float3 pos: POSITION; float3 norm: NORMAL; float3 col: COLOR; };
struct VSOut { float4 pos: SV_POSITION; float3 norm: NORMAL; float3 col: COLOR; };
VSOut vs_main(VSIn i) {
    VSOut o;
    o.pos = mul(float4(i.pos, 1.0), mvp);
    o.norm = i.norm;
    o.col = i.col;
    return o;
}
float4 ps_main(VSOut i) : SV_TARGET {
    float ndl = saturate(dot(normalize(i.norm), -normalize(light_dir.xyz))) * 0.7 + 0.3;
    return float4(i.col * ndl, 1.0);
}
)";

struct Vertex { float px, py, pz; float nx, ny, nz; float cr, cg, cb; };
struct CB     { float mvp[16]; float light[4]; };

struct Part { float cx, cy, cz; float hx, hy, hz; float r, g, b; };

std::vector<Vertex>   g_verts;
std::vector<uint32_t> g_indices;
float g_scene_min[3]{ -1.f, 0.f, -1.f };
float g_scene_max[3]{  1.f, 5.f,  1.f };

ID3D11Device*        g_dev = nullptr;
ID3D11DeviceContext* g_ctx = nullptr;

ID3D11VertexShader*  g_vs  = nullptr;
ID3D11PixelShader*   g_ps  = nullptr;
ID3D11InputLayout*   g_il  = nullptr;
ID3D11Buffer*        g_vb  = nullptr;
ID3D11Buffer*        g_ib  = nullptr;
ID3D11Buffer*        g_cb  = nullptr;
ID3D11RasterizerState*   g_rs = nullptr;
ID3D11DepthStencilState* g_ds = nullptr;

ID3D11Texture2D*         g_rt_tex  = nullptr;
ID3D11RenderTargetView*  g_rtv     = nullptr;
ID3D11ShaderResourceView*g_srv     = nullptr;
ID3D11Texture2D*         g_ds_tex  = nullptr;
ID3D11DepthStencilView*  g_dsv     = nullptr;
uint32_t                 g_rt_w = 0, g_rt_h = 0;

float g_rotation = 0.f;

void push_box(std::vector<Vertex>& v, std::vector<uint32_t>& i, const Part& p) {
    const uint32_t base = (uint32_t)v.size();
    const float x0 = p.cx - p.hx, x1 = p.cx + p.hx;
    const float y0 = p.cy - p.hy, y1 = p.cy + p.hy;
    const float z0 = p.cz - p.hz, z1 = p.cz + p.hz;

    struct Face { float nx, ny, nz; int a, b, c, d; };
    const float corner[8][3] = {
        {x0,y0,z0},{x1,y0,z0},{x1,y1,z0},{x0,y1,z0},
        {x0,y0,z1},{x1,y0,z1},{x1,y1,z1},{x0,y1,z1},
    };
    const Face faces[6] = {
        { 0,0,-1, 0,3,2,1 },
        { 0,0, 1, 4,5,6,7 },
        {-1,0, 0, 0,4,7,3 },
        { 1,0, 0, 1,2,6,5 },
        { 0,-1,0, 0,1,5,4 },
        { 0, 1,0, 3,7,6,2 },
    };
    for (int f = 0; f < 6; ++f) {
        const Face& F = faces[f];
        const int c[4] = { F.a, F.b, F.c, F.d };
        for (int k = 0; k < 4; ++k) {
            v.push_back({
                corner[c[k]][0], corner[c[k]][1], corner[c[k]][2],
                F.nx, F.ny, F.nz,
                p.r, p.g, p.b,
            });
        }
        const uint32_t o = base + (uint32_t)(f * 4);
        i.push_back(o);   i.push_back(o+1); i.push_back(o+2);
        i.push_back(o);   i.push_back(o+2); i.push_back(o+3);
    }
}

std::filesystem::path exe_dir() {
    wchar_t buf[MAX_PATH];
    const DWORD n = GetModuleFileNameW(nullptr, buf, MAX_PATH);
    if (!n) return {};
    return std::filesystem::path(buf).parent_path();
}

bool try_load_miku() {
    const auto p = exe_dir() / L"cat_hatsune_miku.glb";
    if (!std::filesystem::exists(p)) return false;

    model::Mesh m;
    if (!model::load_glb(p.string(), m)) return false;
    if (m.verts.empty() || m.indices.empty()) return false;

    g_verts.clear();
    g_indices.clear();
    g_verts.reserve(m.verts.size());
    for (const auto& v : m.verts) {
        g_verts.push_back({ v.px, v.py, v.pz, v.nx, v.ny, v.nz, v.cr, v.cg, v.cb });
    }
    g_indices = std::move(m.indices);
    std::memcpy(g_scene_min, m.min, sizeof(m.min));
    std::memcpy(g_scene_max, m.max, sizeof(m.max));
    return true;
}

void build_procedural_rig() {
    g_verts.clear();
    g_indices.clear();

    const float sk = 0.94f, sh = 0.30f, pn = 0.20f;
    push_box(g_verts, g_indices, {0.0f, 4.6f, 0.0f, 0.60f,0.60f,0.60f, sk,sk*0.85f,sk*0.72f});
    push_box(g_verts, g_indices, {0.0f, 3.2f, 0.0f, 1.00f,0.55f,0.50f, sh,sh*0.6f, sh*0.4f});
    push_box(g_verts, g_indices, {0.0f, 2.1f, 0.0f, 1.00f,0.55f,0.50f, sh*0.9f,sh*0.55f,sh*0.35f});
    push_box(g_verts, g_indices, {-1.30f, 3.0f, 0.0f, 0.30f,0.85f,0.30f, sk,sk*0.85f,sk*0.72f});
    push_box(g_verts, g_indices, { 1.30f, 3.0f, 0.0f, 0.30f,0.85f,0.30f, sk,sk*0.85f,sk*0.72f});
    push_box(g_verts, g_indices, {-0.50f, 1.0f, 0.0f, 0.40f,1.00f,0.50f, pn,pn*0.7f, pn*0.4f});
    push_box(g_verts, g_indices, { 0.50f, 1.0f, 0.0f, 0.40f,1.00f,0.50f, pn,pn*0.7f, pn*0.4f});
    g_scene_min[0] = -1.9f; g_scene_min[1] = 0.0f; g_scene_min[2] = -0.5f;
    g_scene_max[0] =  1.9f; g_scene_max[1] = 5.2f; g_scene_max[2] =  0.5f;
}

void build_scene() {
    if (!try_load_miku()) build_procedural_rig();
}

void mat_identity(float m[16]) {
    std::memset(m, 0, sizeof(float)*16);
    m[0]=m[5]=m[10]=m[15]=1.f;
}
void mat_rotate_y(float m[16], float a) {
    mat_identity(m);
    const float c = std::cos(a), s = std::sin(a);
    m[0]=c;  m[2]=s;
    m[8]=-s; m[10]=c;
}
void mat_translate(float m[16], float x, float y, float z) {
    mat_identity(m);
    m[3]=x; m[7]=y; m[11]=z;
}
void mat_perspective(float m[16], float fov_y, float aspect, float zn, float zf) {
    const float f = 1.f / std::tan(fov_y * 0.5f);
    std::memset(m, 0, sizeof(float)*16);
    m[0]=f/aspect;
    m[5]=f;
    m[10]=zf / (zf - zn);
    m[11]=(-zn*zf) / (zf - zn);
    m[14]=1.f;
}
void mat_mul(float o[16], const float a[16], const float b[16]) {
    float t[16];
    for (int r = 0; r < 4; ++r)
        for (int c = 0; c < 4; ++c) {
            t[r*4+c] = 0.f;
            for (int k = 0; k < 4; ++k) t[r*4+c] += a[r*4+k] * b[k*4+c];
        }
    std::memcpy(o, t, sizeof(t));
}

void release_rt() {
    if (g_dsv)     { g_dsv->Release();     g_dsv = nullptr; }
    if (g_ds_tex) { g_ds_tex->Release();  g_ds_tex = nullptr; }
    if (g_srv)    { g_srv->Release();     g_srv = nullptr; }
    if (g_rtv)    { g_rtv->Release();     g_rtv = nullptr; }
    if (g_rt_tex) { g_rt_tex->Release();  g_rt_tex = nullptr; }
    g_rt_w = g_rt_h = 0;
}

bool ensure_rt(uint32_t w, uint32_t h) {
    if (g_rt_tex && g_rt_w == w && g_rt_h == h) return true;
    release_rt();

    D3D11_TEXTURE2D_DESC td{};
    td.Width  = w; td.Height = h;
    td.MipLevels = 1; td.ArraySize = 1;
    td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    td.SampleDesc.Count = 1;
    td.Usage = D3D11_USAGE_DEFAULT;
    td.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    if (FAILED(g_dev->CreateTexture2D(&td, nullptr, &g_rt_tex))) return false;
    if (FAILED(g_dev->CreateRenderTargetView(g_rt_tex, nullptr, &g_rtv))) return false;
    if (FAILED(g_dev->CreateShaderResourceView(g_rt_tex, nullptr, &g_srv))) return false;

    D3D11_TEXTURE2D_DESC dd = td;
    dd.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    dd.BindFlags = D3D11_BIND_DEPTH_STENCIL;
    if (FAILED(g_dev->CreateTexture2D(&dd, nullptr, &g_ds_tex))) return false;
    if (FAILED(g_dev->CreateDepthStencilView(g_ds_tex, nullptr, &g_dsv))) return false;

    g_rt_w = w; g_rt_h = h;
    return true;
}

bool compile_shaders() {
    ID3DBlob* vs_blob = nullptr; ID3DBlob* ps_blob = nullptr;
    ID3DBlob* err = nullptr;
    if (FAILED(D3DCompile(kHLSL, std::strlen(kHLSL), "preview_vs", nullptr,
        nullptr, "vs_main", "vs_5_0", 0, 0, &vs_blob, &err))) {
        if (err) err->Release();
        return false;
    }
    if (FAILED(D3DCompile(kHLSL, std::strlen(kHLSL), "preview_ps", nullptr,
        nullptr, "ps_main", "ps_5_0", 0, 0, &ps_blob, &err))) {
        if (err) err->Release();
        vs_blob->Release();
        return false;
    }
    bool ok = true;
    if (FAILED(g_dev->CreateVertexShader(vs_blob->GetBufferPointer(),
        vs_blob->GetBufferSize(), nullptr, &g_vs))) ok = false;
    if (FAILED(g_dev->CreatePixelShader(ps_blob->GetBufferPointer(),
        ps_blob->GetBufferSize(), nullptr, &g_ps))) ok = false;

    const D3D11_INPUT_ELEMENT_DESC ied[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR",    0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };
    if (FAILED(g_dev->CreateInputLayout(ied, 3,
        vs_blob->GetBufferPointer(), vs_blob->GetBufferSize(), &g_il))) ok = false;

    vs_blob->Release();
    ps_blob->Release();
    return ok;
}

}

bool init(ID3D11Device* dev, ID3D11DeviceContext* ctx) {
    if (g_dev) return true;
    if (!dev || !ctx) return false;
    g_dev = dev; g_ctx = ctx;

    build_scene();
    if (!compile_shaders()) { shutdown(); return false; }

    D3D11_BUFFER_DESC bd{};
    bd.Usage = D3D11_USAGE_IMMUTABLE;
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    bd.ByteWidth = (UINT)(g_verts.size() * sizeof(Vertex));
    D3D11_SUBRESOURCE_DATA sd{ g_verts.data(), 0, 0 };
    if (FAILED(dev->CreateBuffer(&bd, &sd, &g_vb))) { shutdown(); return false; }

    bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
    bd.ByteWidth = (UINT)(g_indices.size() * sizeof(uint32_t));
    sd.pSysMem = g_indices.data();
    if (FAILED(dev->CreateBuffer(&bd, &sd, &g_ib))) { shutdown(); return false; }

    D3D11_BUFFER_DESC cbd{};
    cbd.Usage = D3D11_USAGE_DYNAMIC;
    cbd.ByteWidth = sizeof(CB);
    cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    cbd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    if (FAILED(dev->CreateBuffer(&cbd, nullptr, &g_cb))) { shutdown(); return false; }

    D3D11_RASTERIZER_DESC rd{};
    rd.FillMode = D3D11_FILL_SOLID;
    rd.CullMode = D3D11_CULL_BACK;
    rd.FrontCounterClockwise = FALSE;
    dev->CreateRasterizerState(&rd, &g_rs);

    D3D11_DEPTH_STENCIL_DESC dsd{};
    dsd.DepthEnable = TRUE;
    dsd.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
    dsd.DepthFunc = D3D11_COMPARISON_LESS;
    dev->CreateDepthStencilState(&dsd, &g_ds);

    return true;
}

ID3D11ShaderResourceView* render(float dt, uint32_t w, uint32_t h) {
    if (!g_dev || w == 0 || h == 0) return nullptr;
    if (!ensure_rt(w, h)) return nullptr;

    g_rotation += dt * 0.5f;

    const float sx = g_scene_max[0] - g_scene_min[0];
    const float sy = g_scene_max[1] - g_scene_min[1];
    const float sz = g_scene_max[2] - g_scene_min[2];
    const float half_h = 0.5f * (sy > 1e-3f ? sy : 1.f);
    const float radius = 0.5f * std::sqrt(sx*sx + sy*sy + sz*sz);
    const float fov = 0.9f;
    const float dist = radius / std::tan(fov * 0.5f) + radius * 0.4f;

    const float cy = 0.5f * (g_scene_max[1] + g_scene_min[1]);

    float model[16], center[16], rot[16], view[16], proj[16], tmp[16], mv[16], mvp[16];
    mat_translate(center, 0.f, -cy, 0.f);
    mat_rotate_y(rot, g_rotation);
    mat_mul(model, rot, center);
    mat_translate(view, 0.f, 0.f, dist);
    mat_perspective(proj, fov, (float)w / (float)h, 0.1f, dist * 4.f + 10.f);
    (void)half_h;
    mat_mul(mv, view, model);
    mat_mul(mvp, proj, mv);
    (void)tmp;

    D3D11_MAPPED_SUBRESOURCE msr{};
    if (SUCCEEDED(g_ctx->Map(g_cb, 0, D3D11_MAP_WRITE_DISCARD, 0, &msr))) {
        CB* cb = (CB*)msr.pData;
        std::memcpy(cb->mvp, mvp, sizeof(mvp));
        cb->light[0] = -0.5f; cb->light[1] = -1.f; cb->light[2] = 0.3f; cb->light[3] = 0.f;
        g_ctx->Unmap(g_cb, 0);
    }

    ID3D11RenderTargetView* prev_rtv = nullptr;
    ID3D11DepthStencilView* prev_dsv = nullptr;
    g_ctx->OMGetRenderTargets(1, &prev_rtv, &prev_dsv);
    UINT prev_vp_count = 1; D3D11_VIEWPORT prev_vp{};
    g_ctx->RSGetViewports(&prev_vp_count, &prev_vp);
    ID3D11RasterizerState*   prev_rs = nullptr; g_ctx->RSGetState(&prev_rs);
    ID3D11DepthStencilState* prev_ds = nullptr; UINT prev_sref = 0;
    g_ctx->OMGetDepthStencilState(&prev_ds, &prev_sref);

    g_ctx->OMSetRenderTargets(1, &g_rtv, g_dsv);
    D3D11_VIEWPORT vp{ 0, 0, (float)w, (float)h, 0.f, 1.f };
    g_ctx->RSSetViewports(1, &vp);
    const float clear[4] = { 0.10f, 0.10f, 0.12f, 1.f };
    g_ctx->ClearRenderTargetView(g_rtv, clear);
    g_ctx->ClearDepthStencilView(g_dsv, D3D11_CLEAR_DEPTH, 1.f, 0);

    UINT stride = sizeof(Vertex), offset = 0;
    g_ctx->IASetInputLayout(g_il);
    g_ctx->IASetVertexBuffers(0, 1, &g_vb, &stride, &offset);
    g_ctx->IASetIndexBuffer(g_ib, DXGI_FORMAT_R32_UINT, 0);
    g_ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    g_ctx->VSSetShader(g_vs, nullptr, 0);
    g_ctx->VSSetConstantBuffers(0, 1, &g_cb);
    g_ctx->PSSetShader(g_ps, nullptr, 0);
    g_ctx->RSSetState(g_rs);
    g_ctx->OMSetDepthStencilState(g_ds, 0);
    g_ctx->DrawIndexed((UINT)g_indices.size(), 0, 0);

    g_ctx->OMSetRenderTargets(1, &prev_rtv, prev_dsv);
    g_ctx->RSSetViewports(prev_vp_count ? prev_vp_count : 1, &prev_vp);
    g_ctx->RSSetState(prev_rs);
    g_ctx->OMSetDepthStencilState(prev_ds, prev_sref);
    if (prev_rtv) prev_rtv->Release();
    if (prev_dsv) prev_dsv->Release();
    if (prev_rs)  prev_rs->Release();
    if (prev_ds)  prev_ds->Release();

    return g_srv;
}

void shutdown() {
    release_rt();
    if (g_cb) { g_cb->Release(); g_cb = nullptr; }
    if (g_ib) { g_ib->Release(); g_ib = nullptr; }
    if (g_vb) { g_vb->Release(); g_vb = nullptr; }
    if (g_il) { g_il->Release(); g_il = nullptr; }
    if (g_ps) { g_ps->Release(); g_ps = nullptr; }
    if (g_vs) { g_vs->Release(); g_vs = nullptr; }
    if (g_ds) { g_ds->Release(); g_ds = nullptr; }
    if (g_rs) { g_rs->Release(); g_rs = nullptr; }
    g_dev = nullptr; g_ctx = nullptr;
}

}
