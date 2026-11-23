#pragma once
#include <juce_graphics/juce_graphics.h>
#include <cmath>
#include <vector>
#include <functional>

// Mini renderizador fisico para la interfaz: cada pieza (perillas, tornillos, tuerca, joya, panel)
// se describe como un mapa de alturas con material. Se calculan normales por pixel y se ilumina con
// luces de estudio y un entorno con "softboxes" (reflejos reales en baquelita, cromo y vidrio),
// Fresnel, sombras suaves con la forma real de cada pieza y espacio de color lineal.
namespace r3d
{
struct V3 { float x = 0, y = 0, z = 0; };
inline V3 operator+ (V3 a, V3 b) { return { a.x + b.x, a.y + b.y, a.z + b.z }; }
inline V3 operator- (V3 a, V3 b) { return { a.x - b.x, a.y - b.y, a.z - b.z }; }
inline V3 operator* (V3 a, float s) { return { a.x * s, a.y * s, a.z * s }; }
inline float dot (V3 a, V3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline V3 norm (V3 a) { const float l = std::sqrt (dot (a, a)); return l > 0 ? a * (1.0f / l) : V3 { 0, 0, 1 }; }
inline float clamp01 (float v) { return v < 0 ? 0 : (v > 1 ? 1 : v); }
inline float smooth (float t) { t = clamp01 (t); return t * t * (3 - 2 * t); }
inline float lin (float srgb) { return std::pow (srgb, 2.2f); }
// borde redondeado: 0 fuera, sube como cuarto de circulo en 'w' pixeles
inline float fillet (float d, float w) { const float e = clamp01 (d / w); return std::sqrt (1.0f - (1.0f - e) * (1.0f - e)); }

// ruido de valor 2D (para pintura, baquelita y metal)
inline float hash (int x, int y, int s) { unsigned h = (unsigned) (x * 374761393 + y * 668265263 + s * 1442695041); h = (h ^ (h >> 13)) * 1274126177u; return (float) ((h ^ (h >> 16)) & 0xffffff) / 16777215.0f; }
inline float vnoise (float x, float y, int s)
{
    const int xi = (int) std::floor (x), yi = (int) std::floor (y); const float fx = x - (float) xi, fy = y - (float) yi;
    const float u = fx * fx * (3 - 2 * fx), v = fy * fy * (3 - 2 * fy);
    const float a = hash (xi, yi, s), b = hash (xi + 1, yi, s), c = hash (xi, yi + 1, s), d = hash (xi + 1, yi + 1, s);
    return a + (b - a) * u + (c - a) * v + (a - b - c + d) * u * v;
}
inline float fbm (float x, float y, int s, int oct = 3) { float r = 0, a = 0.5f, f = 1; for (int i = 0; i < oct; ++i) { r += a * vnoise (x * f, y * f, s + i * 17); a *= 0.5f; f *= 2.03f; } return r; }

struct Mat
{
    float r = 0.02f, g = 0.02f, b = 0.02f;   // albedo lineal
    float metal = 0, gloss = 0.85f, alpha = 1, f0 = 0.045f;
    float er = 0, eg = 0, eb = 0;             // emision (lampara)
};
struct Sample { float h = 0; Mat m; };
inline Mat srgb (float r, float g, float b, float metal = 0, float gloss = 0.85f) { Mat m; m.r = lin (r); m.g = lin (g); m.b = lin (b); m.metal = metal; m.gloss = gloss; return m; }

// entorno de estudio visto en las reflexiones (x derecha, y abajo, z hacia el que mira)
inline float lobe (V3 d, V3 c, float w, float I, float rough)
{
    const float ww = w + rough * 0.9f;
    const float c0 = std::cos (ww), c1 = std::cos (ww * 0.45f);
    return I * smooth ((dot (d, norm (c)) - c0) / (c1 - c0));
}
inline float band (float x, float lo, float hi, float w) { return smooth ((x - lo) / w) * smooth ((hi - x) / w); }
inline float env (V3 d, float rough)
{
    // estudio de fotografia de producto con paneles de luz rectangulares (reflejos en tiras nitidas):
    // un softbox grande arriba, una tira vertical a la derecha y otra mas debil a la izquierda.
    // Hacia la camara (frente) todo es oscuro, por eso las caras planas se ven negras.
    const float az = std::atan2 (d.x, d.z), el = std::asin (std::max (-1.0f, std::min (1.0f, -d.y)));
    const float w = 0.05f + 0.55f * rough;
    const float away = smooth ((std::acos (std::min (1.0f, d.z)) - 0.30f + 0.55f * rough * rough) / (0.45f + 0.7f * rough));   // 0 mirando a la camara (los metales satinados difunden)
    float e = 0.16f + away * (0.0f + 0.74f * smooth ((el - 0.10f) / 0.6f));        // carpa blanca arriba, oscuro al nivel del horizonte
    e += 3.6f * band (az, -1.45f, 1.05f, w) * band (el, 0.50f, 1.50f, w);      // softbox de arriba
    e += 5.0f * band (az, 0.98f, 1.24f, w) * band (el, -0.45f, 0.95f, w);      // tira derecha
    e += 1.8f * band (az, -1.36f, -1.12f, w) * band (el, -0.30f, 0.75f, w);    // tira izquierda
    return e;
}

struct Shaded { float r, g, b; };
inline Shaded shade (V3 N, V3 V, const Mat& m)
{
    const float NdV = std::max (dot (N, V), 0.0f);
    const V3 R = norm (N * (2.0f * NdV) - V);
    const V3 L1 = norm ({ -0.55f, -0.70f, 0.60f }), L2 = norm ({ 0.70f, -0.25f, 0.70f });
    const float diff = 0.16f * (0.55f + 0.45f * clamp01 (-N.y + 0.5f)) + 0.95f * std::max (dot (N, L1), 0.0f) + 0.28f * std::max (dot (N, L2), 0.0f);
    const float rough = 1.0f - m.gloss;
    const float E = env (R, rough);
    const float fr = std::pow (1.0f - NdV, 5.0f);
    // brillo especular directo de la luz principal (punto de luz en esferas y cantos)
    const V3 H1 = norm (L1 + V), H2 = norm (L2 + V);
    const float shin = 30.0f + 320.0f * m.gloss * m.gloss;
    const float hot = 3.0f * std::pow (std::max (dot (N, H1), 0.0f), shin) + 3.0f * std::pow (std::max (dot (N, H2), 0.0f), shin);
    auto ch = [&] (float alb)
    {
        const float F0 = m.metal > 0.5f ? alb : m.f0;
        const float F = F0 + (1.0f - F0) * fr * (1.0f - rough * 0.7f);
        return alb * diff * (1.0f - m.metal) + F * (E + hot);
    };
    return { ch (m.r) + m.er, ch (m.g) + m.eg, ch (m.b) + m.eb };
}
inline float toSrgb (float c) { c = c / (1.0f + 0.12f * c) * 1.06f; return std::pow (clamp01 (c), 1.0f / 2.2f); }

// renderiza una pieza: fn(x, y) en pixeles relativos al centro de la imagen.
// sombra: desplazamiento y radio de desenfoque (pixeles) y fuerza 0..1
struct ShadowSpec { float dx = 0, dy = 0, blur = 0, strength = 0; };
inline juce::Image render (int w, int h, const std::function<Sample (float, float)>& fn, ShadowSpec sh = {}, const std::function<V3 (float, float)>* viewFn = nullptr)
{
    const int W = w + 2, H = h + 2;
    std::vector<float> hg ((size_t) (W * H)), al ((size_t) (W * H));
    std::vector<Mat> mats ((size_t) (W * H));
    const float cx = (float) w * 0.5f, cy = (float) h * 0.5f;
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x)
        {
            const auto s = fn ((float) x - 1.0f + 0.5f - cx, (float) y - 1.0f + 0.5f - cy);
            const size_t i = (size_t) (y * W + x);
            hg[i] = s.h; mats[i] = s.m; al[i] = s.m.alpha;
        }
    // sombra: mascara desplazada y desenfocada (3 pasadas de caja = casi gaussiana)
    std::vector<float> sd ((size_t) (W * H), 0.0f);
    if (sh.strength > 0)
    {
        const int ox = (int) std::round (sh.dx), oy = (int) std::round (sh.dy);
        for (int y = 0; y < H; ++y) for (int x = 0; x < W; ++x)
        {
            const int sx = x - ox, sy = y - oy;
            if (sx >= 0 && sy >= 0 && sx < W && sy < H) sd[(size_t) (y * W + x)] = al[(size_t) (sy * W + sx)];
        }
        const int r = std::max (1, (int) (sh.blur / 1.7f));
        std::vector<float> tmp (sd.size());
        for (int pass = 0; pass < 3; ++pass)
        {
            for (int y = 0; y < H; ++y) { float acc = 0; int n = 0; for (int x = -r; x < W + r; ++x) { if (x + r < W) { acc += sd[(size_t) (y * W + x + r)]; ++n; } if (x - r - 1 >= 0) { acc -= sd[(size_t) (y * W + x - r - 1)]; --n; } if (x >= 0 && x < W) tmp[(size_t) (y * W + x)] = n > 0 ? acc / (float) (2 * r + 1) : 0; } }
            for (int x = 0; x < W; ++x) { float acc = 0; for (int y = -r; y < H + r; ++y) { if (y + r < H) acc += tmp[(size_t) ((y + r) * W + x)]; if (y - r - 1 >= 0) acc -= tmp[(size_t) ((y - r - 1) * W + x)]; if (y >= 0 && y < H) sd[(size_t) (y * W + x)] = acc / (float) (2 * r + 1); } }
        }
    }
    juce::Image img (juce::Image::ARGB, w, h, true);
    juce::Image::BitmapData bd (img, juce::Image::BitmapData::writeOnly);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x)
        {
            const size_t i = (size_t) ((y + 1) * W + x + 1);
            const float a = al[i];
            const float s = sh.strength * sd[i];
            float outA = a + (1.0f - a) * s, r = 0, g = 0, b = 0;
            if (a > 0.001f)
            {
                const float dhx = (hg[i + 1] - hg[i - 1]) * 0.5f, dhy = (hg[i + (size_t) W] - hg[i - (size_t) W]) * 0.5f;
                const V3 V = viewFn != nullptr ? (*viewFn) ((float) x + 0.5f - cx, (float) y + 0.5f - cy) : V3 { 0, 0, 1 };
                const auto c = shade (norm ({ -dhx, -dhy, 1.0f }), V, mats[i]);
                r = toSrgb (c.r) * a; g = toSrgb (c.g) * a; b = toSrgb (c.b) * a;
            }
            if (outA <= 0.0005f) continue;
            bd.setPixelColour (x, y, juce::Colour::fromFloatRGBA (r / outA, g / outA, b / outA, outA));
        }
    return img;
}
} // namespace r3d
