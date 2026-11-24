#pragma once
#include <functional>
#include <map>
#include <vector>
#include "Render3D.h"

// Dibujo 3D por pixel de cada microfono de TOP MIC COLLECTION (vista de frente, cuerpo de revolucion).
// Disenos propios de AnalogHM: cada modelo tiene su forma, su acabado y su placa "HM"; no copian ninguna marca.
// Rejilla tejida de verdad (alambres que pasan por arriba y por abajo) y a traves de los huecos se ve la capsula.
namespace micart
{
using namespace r3d;
enum Kind { Mesh = 0, Metal, Paint, Ring, SlotsH, SlotsV, Fins, Foam, Bars, Perf, Dark };
enum Cap { CapNone = 0, CapLdc, CapRibbon, CapDyn };
using Prof = std::function<float (float)>;

struct Part
{
    float y0, y1; int kind; Prof w;
    float r, g, b, gloss; bool metal;
    bool flat = false; float edge = 6.0f;          // placa plana con canto redondeado (cintas)
    float inset = 0;                               // panel oscuro al frente (ancho medio) entre y0+12 y y0+58
};
struct Design { std::vector<Part> parts; float height = 400, halfW = 50; int cap = CapLdc; float capY = 60, capR = 30; float badgeY = -1; float meshP = 2.2f; float mr = 0.75f, mg = 0.75f, mb = 0.74f; };

// perfiles (medio ancho en funcion de y)
inline Prof flatW (float W) { return [W] (float) { return W; }; }
inline Prof taperW (float y0, float y1, float a, float b) { return [=] (float y) { const float t = std::clamp ((y - y0) / std::max (1.0f, y1 - y0), 0.0f, 1.0f); return a + (b - a) * t; }; }
inline Prof roundRectW (float y0, float H, float W, float rt, float rb)
{
    return [=] (float y)
    {
        const float d = y - y0, e = y0 + H - y;
        if (d < rt) { const float q = rt - d; return W - rt + std::sqrt (std::max (0.0f, rt * rt - q * q)); }
        if (e < rb) { const float q = rb - e; return W - rb + std::sqrt (std::max (0.0f, rb * rb - q * q)); }
        return W;
    };
}
inline Prof ellipseW (float y0, float H, float W) { return [=] (float y) { const float u = (y - y0 - H * 0.5f) / (H * 0.5f); return W * std::sqrt (std::max (0.0f, 1.0f - u * u)); }; }
inline Prof superW (float y0, float H, float W, float p) { return [=] (float y) { const float u = std::abs ((y - y0) / H * 2.0f - 1.0f); return W * std::pow (std::max (0.0f, 1.0f - std::pow (u, p)), 1.0f / p); }; }

inline Part P (float y0, float y1, int kind, Prof w, float r, float g, float b, float gloss, bool metal) { Part p; p.y0 = y0; p.y1 = y1; p.kind = kind; p.w = std::move (w); p.r = r; p.g = g; p.b = b; p.gloss = gloss; p.metal = metal; return p; }

inline Design designFor (int model)
{
    Design d;
    const float chr = 0.86f;
    auto ring = [&] (float y0, float y1, float W, float r = 0.86f, float g = 0.86f, float b = 0.87f) { return P (y0, y1, Ring, roundRectW (y0, y1 - y0, W, 2.5f, 2.5f), r, g, b, 0.92f, true); };
    switch (model)
    {
        case 0:   // TM-V1: valvula, cuerpo champan, cabeza de capsula alta, banda negra ranurada
            d.parts = { P (0, 150, Mesh, roundRectW (0, 150, 46, 46, 12), 0, 0, 0, 0, true), ring (146, 156, 47.5f),
                        P (156, 176, SlotsV, flatW (45), 0.07f, 0.07f, 0.075f, 0.6f, false),
                        P (176, 432, Metal, flatW (45), 0.80f, 0.72f, 0.55f, 0.62f, true), P (432, 458, Metal, taperW (432, 458, 45, 34), 0.70f, 0.62f, 0.47f, 0.6f, true),
                        ring (456, 466, 34) };
            d.height = 466; d.halfW = 48; d.capY = 72; d.capR = 33; d.badgeY = 208; d.mr = 0.80f; d.mg = 0.74f; d.mb = 0.60f; break;
        case 1:   // TM-V2: cabeza ovalada grande, cuerpo negro mate, aros dorados
            d.parts = { P (0, 150, Mesh, ellipseW (0, 150, 54), 0, 0, 0, 0, true), ring (144, 156, 42, 0.90f, 0.74f, 0.40f),
                        P (156, 440, Paint, taperW (156, 440, 41, 36), 0.055f, 0.055f, 0.06f, 0.38f, false), ring (438, 450, 37, 0.90f, 0.74f, 0.40f) };
            d.height = 450; d.halfW = 54; d.capY = 75; d.capR = 36; d.badgeY = 192; break;
        case 2:   // TM-F1: niquel satinado, cabeza de esquinas redondeadas, dos ranuras finas
            d.parts = { P (0, 128, Mesh, roundRectW (0, 128, 46, 22, 10), 0, 0, 0, 0, true), ring (126, 134, 47),
                        P (134, 420, Metal, flatW (44), 0.72f, 0.72f, 0.70f, 0.60f, true),
                        P (300, 303, Dark, flatW (44.4f), 0.03f, 0.03f, 0.03f, 0.3f, false), P (307, 310, Dark, flatW (44.4f), 0.03f, 0.03f, 0.03f, 0.3f, false),
                        P (420, 446, Metal, taperW (420, 446, 44, 30), 0.62f, 0.62f, 0.60f, 0.6f, true), ring (444, 452, 30) };
            d.height = 452; d.halfW = 47; d.capY = 64; d.capR = 32; d.badgeY = 172; break;
        case 3:   // TM-V3: marfil pintado, cabeza casi cuadrada con malla dorada
            d.parts = { P (0, 120, Mesh, superW (0, 120, 46, 3.2f), 0, 0, 0, 0, true), ring (116, 126, 40),
                        P (126, 440, Paint, flatW (37), 0.86f, 0.83f, 0.74f, 0.55f, false), ring (438, 450, 37) };
            d.height = 450; d.halfW = 46; d.capY = 60; d.capR = 30; d.badgeY = 162; d.mr = 0.80f; d.mg = 0.70f; d.mb = 0.48f; break;
        case 4:   // TM-V4: crema verdoso, cabeza redondeada, aro dorado
            d.parts = { P (0, 126, Mesh, roundRectW (0, 126, 47, 26, 12), 0, 0, 0, 0, true), ring (122, 132, 44, 0.90f, 0.74f, 0.40f),
                        P (132, 440, Paint, flatW (41), 0.76f, 0.78f, 0.70f, 0.50f, false), P (440, 462, Paint, taperW (440, 462, 41, 32), 0.70f, 0.72f, 0.64f, 0.5f, false),
                        ring (460, 468, 32) };
            d.height = 468; d.halfW = 47; d.capY = 63; d.capR = 31; d.badgeY = 168; break;
        case 5:   // TM-V5: dorado con aletas de enfriamiento, malla oscura
            d.parts = { P (0, 150, Mesh, roundRectW (0, 150, 48, 48, 14), 0, 0, 0, 0, true), ring (146, 158, 50, 0.92f, 0.76f, 0.42f),
                        P (158, 232, Fins, flatW (47), 0.86f, 0.70f, 0.36f, 0.72f, true), P (232, 440, Metal, flatW (46), 0.86f, 0.70f, 0.38f, 0.70f, true),
                        ring (438, 450, 46, 0.92f, 0.76f, 0.42f) };
            d.height = 450; d.halfW = 50; d.capY = 75; d.capR = 34; d.badgeY = 256; d.mr = 0.16f; d.mg = 0.16f; d.mb = 0.17f; break;
        case 6:   // TM-F2: grafito, cabeza cuadrada con malla oscura y panel de controles
        {
            Part body = P (124, 410, Metal, flatW (41), 0.26f, 0.27f, 0.28f, 0.58f, true); body.inset = 13;
            d.parts = { P (0, 118, Mesh, superW (0, 118, 50, 4.0f), 0, 0, 0, 0, true), ring (114, 124, 44, 0.55f, 0.56f, 0.58f), body,
                        P (410, 436, Metal, taperW (410, 436, 41, 30), 0.22f, 0.23f, 0.24f, 0.6f, true), ring (434, 442, 30) };
            d.height = 442; d.halfW = 50; d.capY = 59; d.capR = 33; d.badgeY = 226; d.mr = 0.42f; d.mg = 0.43f; d.mb = 0.45f; break;
        }
        case 7:   // TM-F3: corto, niquel
            d.parts = { P (0, 118, Mesh, roundRectW (0, 118, 46, 46, 10), 0, 0, 0, 0, true), ring (114, 124, 47.5f),
                        P (124, 300, Metal, flatW (46), 0.74f, 0.74f, 0.72f, 0.62f, true), P (300, 322, Metal, taperW (300, 322, 46, 36), 0.64f, 0.64f, 0.62f, 0.6f, true),
                        ring (320, 330, 36) };
            d.height = 330; d.halfW = 48; d.capY = 59; d.capR = 31; d.badgeY = 152; break;
        case 8:   // TM-D1: dinamico de mano delgado
            d.parts = { P (0, 92, Mesh, roundRectW (0, 92, 25, 8, 4), 0, 0, 0, 0, true), ring (88, 98, 26.5f),
                        P (98, 400, Paint, taperW (98, 400, 25, 17), 0.10f, 0.10f, 0.11f, 0.62f, false), P (400, 424, Metal, flatW (17), 0.70f, 0.70f, 0.70f, 0.6f, true) };
            d.height = 424; d.halfW = 27; d.cap = CapDyn; d.capY = 40; d.capR = 18; d.badgeY = 128; d.meshP = 1.8f; d.mr = 0.42f; d.mg = 0.42f; d.mb = 0.43f; break;
        case 9:   // TM-D2: dinamico de radio con espuma
            d.parts = { P (0, 150, Foam, roundRectW (0, 150, 52, 40, 8), 0.035f, 0.035f, 0.04f, 0.2f, false),
                        P (150, 330, Paint, flatW (45), 0.07f, 0.07f, 0.075f, 0.45f, false), ring (328, 338, 46), P (338, 362, Dark, taperW (338, 362, 40, 32), 0.05f, 0.05f, 0.05f, 0.4f, false) };
            d.height = 362; d.halfW = 52; d.cap = CapNone; d.badgeY = 190; break;
        case 10:  // TM-D3: dinamico largo con ranuras horizontales
            d.parts = { P (0, 200, SlotsH, roundRectW (0, 200, 31, 10, 3), 0.62f, 0.60f, 0.55f, 0.62f, true), ring (198, 206, 32),
                        P (206, 430, Paint, flatW (31), 0.55f, 0.52f, 0.47f, 0.5f, false), P (430, 446, Dark, flatW (26), 0.05f, 0.05f, 0.05f, 0.4f, false) };
            d.height = 446; d.halfW = 33; d.cap = CapNone; d.badgeY = 240; break;
        case 11:  // TM-R1: cinta moderna delgada
            d.parts = { P (0, 190, Mesh, roundRectW (0, 190, 22, 22, 6), 0, 0, 0, 0, true), ring (186, 196, 23),
                        P (196, 420, Paint, flatW (22), 0.05f, 0.05f, 0.055f, 0.45f, false), ring (418, 428, 22) };
            d.height = 428; d.halfW = 24; d.cap = CapRibbon; d.capY = 95; d.capR = 70; d.badgeY = 230; d.meshP = 1.9f; d.mr = 0.30f; d.mg = 0.30f; d.mb = 0.31f; break;
        case 12:  // TM-R2: cinta vintage: placa con barras cromadas
        {
            Part head = P (0, 230, Bars, roundRectW (0, 230, 62, 56, 56), 0.84f, 0.84f, 0.85f, 0.9f, true); head.flat = true; head.edge = 9;
            d.parts = { head, P (228, 262, Metal, flatW (12), 0.30f, 0.30f, 0.31f, 0.6f, true), P (260, 310, Paint, roundRectW (260, 50, 34, 10, 6), 0.16f, 0.16f, 0.17f, 0.55f, false) };
            d.height = 310; d.halfW = 62; d.cap = CapRibbon; d.capY = 115; d.capR = 90; d.badgeY = 285; break;
        }
        case 13:  // TM-R3: cinta britanica: placa ovalada perforada
        {
            Part head = P (0, 210, Perf, ellipseW (0, 210, 56), 0.70f, 0.70f, 0.69f, 0.65f, true); head.flat = true; head.edge = 7;
            d.parts = { head, P (206, 270, Metal, flatW (22), 0.66f, 0.66f, 0.64f, 0.6f, true), P (270, 292, Dark, roundRectW (270, 22, 28, 4, 4), 0.05f, 0.05f, 0.05f, 0.4f, false) };
            d.height = 292; d.halfW = 56; d.cap = CapRibbon; d.capY = 105; d.capR = 80; d.badgeY = 240; break;
        }
        case 14:  // TM-V6: negro piano, cabeza grande, filete dorado
            d.parts = { P (0, 170, Mesh, roundRectW (0, 170, 52, 52, 14), 0, 0, 0, 0, true), ring (166, 178, 54),
                        P (178, 446, Paint, flatW (46), 0.02f, 0.02f, 0.022f, 0.9f, false), P (300, 303, Ring, flatW (46.5f), 0.92f, 0.76f, 0.42f, 0.9f, true),
                        ring (444, 456, 46) };
            d.height = 456; d.halfW = 54; d.capY = 85; d.capR = 38; d.badgeY = 208; d.mr = 0.82f; d.mg = 0.82f; d.mb = 0.83f; break;
        default:  // 15 TM-F4: champan niquel, cabeza ancha
            d.parts = { P (0, 140, Mesh, roundRectW (0, 140, 48, 48, 12), 0, 0, 0, 0, true), ring (136, 146, 49.5f),
                        P (146, 436, Metal, flatW (48), 0.76f, 0.72f, 0.62f, 0.60f, true), P (436, 456, Metal, taperW (436, 456, 48, 36), 0.66f, 0.62f, 0.53f, 0.6f, true),
                        ring (454, 462, 36) };
            d.height = 462; d.halfW = 50; d.capY = 70; d.capR = 34; d.badgeY = 176; break;
    }
    (void) chr;
    return d;
}

inline Mat mixM (Mat a, const Mat& b, float t)
{
    a.r += (b.r - a.r) * t; a.g += (b.g - a.g) * t; a.b += (b.b - a.b) * t; a.metal += (b.metal - a.metal) * t; a.gloss += (b.gloss - a.gloss) * t;
    a.f0 += (b.f0 - a.f0) * t; a.er += (b.er - a.er) * t; a.eg += (b.eg - a.eg) * t; a.eb += (b.eb - a.eb) * t; return a;
}

// pixel del microfono: X centrado, Y desde arriba (unidades de diseno)
inline Sample micPixel (const Design& d, float X, float Y)
{
    Sample best; best.h = -1; best.m.alpha = 0;
    float bestZ = -1;
    for (const auto& p : d.parts)
    {
        if (Y < p.y0 - 0.5f || Y > p.y1 + 0.5f) continue;
        const float w = p.w (std::clamp (Y, p.y0, p.y1));
        if (w <= 0.2f || std::abs (X) > w + 0.6f) continue;
        const float cov = clamp01 (w + 0.5f - std::abs (X)) * clamp01 (Y - p.y0 + 0.5f) * clamp01 (p.y1 - Y + 0.5f);
        const float wp = (p.w (std::clamp (Y + 0.5f, p.y0, p.y1)) - p.w (std::clamp (Y - 0.5f, p.y0, p.y1)));     // dw/dy
        float s = std::clamp (X / w, -1.0f, 1.0f), c = std::sqrt (std::max (0.0f, 1.0f - s * s));
        float z; V3 n;
        if (p.flat)
        {
            const float e = std::abs (X) - (w - p.edge);
            if (e > 0) { const float t = clamp01 (e / p.edge); s = (X > 0 ? 1.0f : -1.0f) * t; c = std::sqrt (std::max (0.0f, 1.0f - t * t)); }
            else { s = 0; c = 1; }
            z = 14.0f * c + 4.0f; n = norm ({ s, -wp * 0.6f, c });
        }
        else { z = w * c; n = norm ({ s, -wp, c }); }
        if (z <= bestZ) continue;
        bestZ = z;
        Sample q; q.h = z; q.hasN = true;
        const float phi = std::asin (s), u = phi * w;                         // coordenada a lo largo de la circunferencia
        const V3 tu = norm ({ c, 0.0f, -s });                                   // tangente horizontal
        auto tilt = [&] (float du, float dv) { q.n = norm (n + tu * du + V3 { 0, 1, 0 } * dv); };
        q.n = n;
        switch (p.kind)
        {
            case Mesh:
            {
                const float pp = d.meshP, rw = 0.34f * pp;
                const float fu = u / pp, fv = Y / pp;
                const float cu = fu - std::floor (fu), cv = fv - std::floor (fv);
                const float du = (cu < 0.5f ? cu : cu - 1.0f) * pp, dv = (cv < 0.5f ? cv : cv - 1.0f) * pp;   // distancia al alambre mas cercano
                const int iu = (int) std::floor (fu + 0.5f), iv = (int) std::floor (fv + 0.5f);
                const float aw = clamp01 ((rw - std::abs (du)) / 0.35f + 0.5f), ah = clamp01 ((rw - std::abs (dv)) / 0.35f + 0.5f);
                const bool vertOnTop = ((iu + iv) & 1) == 0;
                Mat wire = srgb (d.mr, d.mg, d.mb, 1, 0.78f);
                const float tone = 0.92f + 0.08f * vnoise (u * 0.05f, Y * 0.05f, 9);
                wire.r *= tone; wire.g *= tone; wire.b *= tone;
                // interior: tela interior fina y la capsula
                Mat in = srgb (0.018f, 0.018f, 0.02f, 0, 0.3f);
                const float fine = 0.5f + 0.5f * std::cos (u * 6.2831853f / (pp * 0.42f)) * std::cos (Y * 6.2831853f / (pp * 0.42f));
                in.r += 0.010f * fine; in.g += 0.010f * fine; in.b += 0.011f * fine;
                if (d.cap == CapLdc)
                {
                    const float rr = std::sqrt (X * X + (Y - d.capY) * (Y - d.capY));
                    if (rr < d.capR) { Mat gold = srgb (0.28f, 0.20f, 0.07f, 1, 0.85f); in = mixM (in, gold, 0.55f * (1.0f - 0.5f * rr / d.capR)); }
                    if (std::abs (rr - d.capR) < 2.2f) in = mixM (in, srgb (0.62f, 0.48f, 0.22f, 1, 0.8f), 0.8f);
                }
                else if (d.cap == CapRibbon)
                {
                    if (std::abs (X) < 2.6f && std::abs (Y - d.capY) < d.capR) { Mat al = srgb (0.55f, 0.55f, 0.56f, 1, 0.6f); const float corr = 0.75f + 0.25f * std::sin (Y * 1.9f); al.r *= corr; al.g *= corr; al.b *= corr; in = mixM (in, al, 0.85f); }
                }
                else if (d.cap == CapDyn)
                {
                    const float rr = std::sqrt (X * X + (Y - d.capY) * (Y - d.capY));
                    if (rr < d.capR) in = mixM (in, srgb (0.06f, 0.06f, 0.065f, 0, 0.5f), 0.7f);
                }
                float h = z - 2.2f; q.m = in;
                if (aw > 0 || ah > 0)
                {
                    // dos alambres: el de arriba gana; perfil cilindrico de cada uno
                    const float zv = std::sqrt (std::max (0.0f, 1.0f - (du / rw) * (du / rw))) * rw, zh = std::sqrt (std::max (0.0f, 1.0f - (dv / rw) * (dv / rw))) * rw;
                    const float liftV = vertOnTop ? 0.45f * rw : 0.0f, liftH = vertOnTop ? 0.0f : 0.45f * rw;
                    const float hv = aw > 0 ? zv + liftV : -9, hh = ah > 0 ? zh + liftH : -9;
                    if (hv >= hh) { q.m = mixM (in, wire, aw); tilt (du / rw * 0.9f * aw, 0); h = z - 1.0f + hv; }
                    else { q.m = mixM (in, wire, ah); tilt (0, dv / rw * 0.9f * ah); h = z - 1.0f + hh; }
                }
                q.h = h;
                // sombra suave dentro de la cabeza hacia los bordes
                const float edgeDark = 0.55f + 0.45f * c;
                q.m.r *= edgeDark; q.m.g *= edgeDark; q.m.b *= edgeDark;
                break;
            }
            case Metal: case Ring:
            {
                q.m = srgb (p.r, p.g, p.b, 1, p.gloss);
                const float lathe = 0.94f + 0.06f * vnoise (0.5f, Y * 3.1f, 21) + 0.03f * (hash ((int) (Y * 4), 3, 5) - 0.5f);
                q.m.r *= lathe; q.m.g *= lathe; q.m.b *= lathe;
                if (p.inset > 0 && std::abs (X) < p.inset && Y > p.y0 + 12 && Y < p.y0 + 58)
                {
                    const float e = clamp01 ((p.inset - std::abs (X)) / 1.2f) * clamp01 ((Y - p.y0 - 12) / 1.2f) * clamp01 ((p.y0 + 58 - Y) / 1.2f);
                    q.m = mixM (q.m, srgb (0.05f, 0.05f, 0.055f, 0, 0.55f), e); q.h -= 0.8f * e;
                    // tres interruptores chicos
                    for (int k = 0; k < 3; ++k) { const float sy = p.y0 + 22 + 13.0f * (float) k, dd = std::hypot (X, Y - sy); if (dd < 3.2f) { q.m = srgb (0.80f, 0.80f, 0.80f, 1, 0.8f); q.h += 1.5f * (1 - dd / 3.2f); } }
                }
                break;
            }
            case Paint:
            {
                q.m = srgb (p.r, p.g, p.b, 0, p.gloss); q.m.f0 = 0.05f;
                const float t = 0.97f + 0.03f * fbm (u * 0.06f, Y * 0.06f, 31, 3);
                q.m.r *= t; q.m.g *= t; q.m.b *= t;
                break;
            }
            case Dark: q.m = srgb (p.r, p.g, p.b, 0, p.gloss); break;
            case SlotsV:
            {
                q.m = srgb (p.r, p.g, p.b, 0, p.gloss);
                const float per = 5.0f, cu = u / per - std::floor (u / per);
                if (cu > 0.25f && cu < 0.75f && Y > p.y0 + 4 && Y < p.y1 - 4) { q.m = srgb (0.45f, 0.36f, 0.18f, 1, 0.6f); q.m.r *= 0.5f; q.m.g *= 0.5f; q.m.b *= 0.5f; q.h -= 1.5f; }
                break;
            }
            case SlotsH:
            {
                q.m = srgb (p.r, p.g, p.b, 1, p.gloss);
                const float per = 6.5f, cv = (Y - p.y0) / per - std::floor ((Y - p.y0) / per);
                if (Y > p.y0 + 14 && Y < p.y1 - 10 && std::abs (s) < 0.82f && cv > 0.38f && cv < 0.86f)
                { q.m = srgb (0.02f, 0.02f, 0.022f, 0, 0.3f); q.h -= 2.0f; tilt (0, (cv - 0.62f) * 1.5f); }
                break;
            }
            case Fins:
            {
                q.m = srgb (p.r, p.g, p.b, 1, p.gloss);
                const float per = 4.2f, ph = u / per * 6.2831853f;
                tilt (0.55f * std::sin (ph), 0); q.h += 0.8f * std::cos (ph);
                const float g = 0.85f + 0.15f * std::cos (ph); q.m.r *= g; q.m.g *= g; q.m.b *= g;
                break;
            }
            case Foam:
            {
                q.m = srgb (p.r, p.g, p.b, 0, p.gloss); q.m.f0 = 0.02f;
                const float nx = fbm (u * 0.9f, Y * 0.9f, 41, 3) - 0.5f, ny = fbm (u * 0.9f + 17, Y * 0.9f, 43, 3) - 0.5f;
                tilt (nx * 0.9f, ny * 0.9f); q.h += (fbm (u * 0.5f, Y * 0.5f, 47, 3) - 0.5f) * 2.0f;
                const float g = 0.85f + 0.3f * fbm (u * 0.4f, Y * 0.4f, 49, 2); q.m.r *= g; q.m.g *= g; q.m.b *= g;
                break;
            }
            case Bars:
            {
                const float frame = std::abs (X) - (w - 7.0f);
                q.m = srgb (p.r, p.g, p.b, 1, p.gloss);
                const float rr = Y - p.y0, rb = p.y1 - Y;
                if (frame < 0 && rr > 8 && rb > 8)
                {
                    const float per = 6.5f, cv = rr / per - std::floor (rr / per);
                    if (cv > 0.62f)
                    {
                        Mat in = srgb (0.02f, 0.02f, 0.022f, 0, 0.3f);
                        if (std::abs (X) < 3.0f && std::abs (Y - d.capY) < d.capR) in = mixM (in, srgb (0.5f, 0.5f, 0.52f, 1, 0.6f), 0.7f);
                        q.m = in; q.h -= 3.0f;
                    }
                    else tilt (0, (cv - 0.31f) * 2.2f);
                }
                break;
            }
            case Perf:
            {
                q.m = srgb (p.r, p.g, p.b, 1, p.gloss);
                const float per = 4.6f, gx = X / per - std::floor (X / per) - 0.5f, gy = Y / per - std::floor (Y / per) - 0.5f;
                const float dd = std::sqrt (gx * gx + gy * gy) * per;
                if (std::abs (X) < w - 8.0f && Y > p.y0 + 9 && Y < p.y1 - 9 && dd < 1.45f)
                {
                    Mat in = srgb (0.02f, 0.02f, 0.022f, 0, 0.3f);
                    if (std::abs (X) < 3.0f && std::abs (Y - d.capY) < d.capR) in = mixM (in, srgb (0.5f, 0.5f, 0.52f, 1, 0.6f), 0.7f);
                    q.m = mixM (q.m, in, clamp01 ((1.45f - dd) / 0.5f)); q.h -= 1.0f;
                }
                break;
            }
        }
        q.m.alpha = cov;
        best = q;
    }
    // placa propia "HM" (esmalte negro con aro dorado); el texto se pinta encima
    if (d.badgeY > 0 && best.m.alpha > 0)
    {
        const float ex = X / 11.0f, ey = (Y - d.badgeY) / 14.0f, rr = std::sqrt (ex * ex + ey * ey);
        if (rr < 1.0f)
        {
            const float k = clamp01 ((1.0f - rr) * 11.0f);
            Mat en = srgb (0.03f, 0.03f, 0.035f, 0, 0.92f); en.f0 = 0.06f;
            if (rr > 0.80f) en = srgb (0.90f, 0.74f, 0.40f, 1, 0.85f);
            best.m = mixM (best.m, en, k); best.h += 1.2f * k;
        }
    }
    return best;
}

// imagen del microfono (cache); k = pixeles por unidad de diseno
inline const juce::Image& micImage (int model, float k)
{
    static std::map<std::pair<int, int>, juce::Image> cache;
    const auto key = std::make_pair (model, (int) std::lround (k * 100));
    auto it = cache.find (key);
    if (it != cache.end()) return it->second;
    const Design d = designFor (model);
    const float mx = 26, my = 30;                                              // margen para la sombra
    const int iw = (int) std::ceil ((d.halfW * 2 + mx * 2) * k), ih = (int) std::ceil ((d.height + my * 2) * k);
    const float cx = (float) iw * 0.5f, cy = (float) ih * 0.5f;
    auto img = render (iw, ih, [&] (float rx, float ry)
    {
        const float px = rx + cx, py = ry + cy;
        const float X = px / k - (d.halfW + mx), Y = py / k - my;
        Sample s = micPixel (d, X, Y);
        s.h *= k;
        return s;
    }, { 9.0f * k, 12.0f * k, 11.0f * k, 0.55f, 2.0f * k, 0.35f });
    return cache[key] = img;
}
} // namespace micart
