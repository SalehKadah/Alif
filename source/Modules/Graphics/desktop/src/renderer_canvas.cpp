/**
 * ═══════════════════════════════════════════════════════════════════
 * ملف: renderer_canvas.cpp
 * المسار: features/graphics/backends/desktop/src/renderer_canvas.cpp
 * ═══════════════════════════════════════════════════════════════════
 *
 * دوال Canvas API (دوائر، خطوط، مثلثات، مضلعات، أقواس، بيزيه)
 * + تطبيقات واجهة PlatformRenderer (Override methods)
 *
 * ═══════════════════════════════════════════════════════════════════
 */

#include "renderer_internal.h"

namespace sad {
namespace ui {
namespace desktop {

// ═══════════════════════════════════════════════════════════════════
// Canvas API
// ═══════════════════════════════════════════════════════════════════

// رسم دائرة ممتلئة باستخدام خوارزمية Midpoint Circle
void DesktopRenderer::drawCircle(float cx, float cy, float radius, const Color& color) {
    if (!sdlRenderer_ || radius <= 0) return;
    
    SDL_SetRenderDrawColor(sdlRenderer_, toU8(color.r), toU8(color.g), toU8(color.b), toU8(color.a));
    
    int x = static_cast<int>(radius);
    int y = 0;
    int centerX = static_cast<int>(cx);
    int centerY = static_cast<int>(cy);
    int radiusError = 1 - x;
    
    while (x >= y) {
        SDL_RenderDrawLine(sdlRenderer_, centerX - x, centerY + y, centerX + x, centerY + y);
        SDL_RenderDrawLine(sdlRenderer_, centerX - x, centerY - y, centerX + x, centerY - y);
        SDL_RenderDrawLine(sdlRenderer_, centerX - y, centerY + x, centerX + y, centerY + x);
        SDL_RenderDrawLine(sdlRenderer_, centerX - y, centerY - x, centerX + y, centerY - x);
        
        y++;
        if (radiusError < 0) {
            radiusError += 2 * y + 1;
        } else {
            x--;
            radiusError += 2 * (y - x + 1);
        }
    }
}

// رسم دائرة فارغة (حدود فقط)
void DesktopRenderer::drawCircleOutline(float cx, float cy, float radius, const Color& color, float thickness) {
    if (!sdlRenderer_ || radius <= 0) return;
    
    SDL_SetRenderDrawColor(sdlRenderer_, toU8(color.r), toU8(color.g), toU8(color.b), toU8(color.a));
    
    for (float t = 0; t < thickness; t += 1.0f) {
        float r = radius - t;
        if (r <= 0) break;
        
        int x = static_cast<int>(r);
        int y = 0;
        int centerX = static_cast<int>(cx);
        int centerY = static_cast<int>(cy);
        int radiusError = 1 - x;
        
        while (x >= y) {
            SDL_RenderDrawPoint(sdlRenderer_, centerX + x, centerY + y);
            SDL_RenderDrawPoint(sdlRenderer_, centerX + y, centerY + x);
            SDL_RenderDrawPoint(sdlRenderer_, centerX - y, centerY + x);
            SDL_RenderDrawPoint(sdlRenderer_, centerX - x, centerY + y);
            SDL_RenderDrawPoint(sdlRenderer_, centerX - x, centerY - y);
            SDL_RenderDrawPoint(sdlRenderer_, centerX - y, centerY - x);
            SDL_RenderDrawPoint(sdlRenderer_, centerX + y, centerY - x);
            SDL_RenderDrawPoint(sdlRenderer_, centerX + x, centerY - y);
            
            y++;
            if (radiusError < 0) {
                radiusError += 2 * y + 1;
            } else {
                x--;
                radiusError += 2 * (y - x + 1);
            }
        }
    }
}

// رسم قطع ناقص (ellipse)
void DesktopRenderer::drawEllipse(float cx, float cy, float rx, float ry, const Color& color) {
    if (!sdlRenderer_ || rx <= 0 || ry <= 0) return;
    
    SDL_SetRenderDrawColor(sdlRenderer_, toU8(color.r), toU8(color.g), toU8(color.b), toU8(color.a));
    
    int centerX = static_cast<int>(cx);
    int centerY = static_cast<int>(cy);
    
    int a2 = static_cast<int>(rx * rx);
    int b2 = static_cast<int>(ry * ry);
    int fa2 = 4 * a2;
    int fb2 = 4 * b2;
    int x, y, sigma;
    
    // المنطقة الأولى
    for (x = 0, y = static_cast<int>(ry), sigma = 2 * b2 + a2 * (1 - 2 * static_cast<int>(ry)); b2 * x <= a2 * y; x++) {
        SDL_RenderDrawLine(sdlRenderer_, centerX - x, centerY + y, centerX + x, centerY + y);
        SDL_RenderDrawLine(sdlRenderer_, centerX - x, centerY - y, centerX + x, centerY - y);
        
        if (sigma >= 0) {
            sigma += fa2 * (1 - y);
            y--;
        }
        sigma += b2 * ((4 * x) + 6);
    }
    
    // المنطقة الثانية
    for (x = static_cast<int>(rx), y = 0, sigma = 2 * a2 + b2 * (1 - 2 * static_cast<int>(rx)); a2 * y <= b2 * x; y++) {
        SDL_RenderDrawLine(sdlRenderer_, centerX - x, centerY + y, centerX + x, centerY + y);
        SDL_RenderDrawLine(sdlRenderer_, centerX - x, centerY - y, centerX + x, centerY - y);
        
        if (sigma >= 0) {
            sigma += fb2 * (1 - x);
            x--;
        }
        sigma += a2 * ((4 * y) + 6);
    }
}

// رسم خط
void DesktopRenderer::drawLine(float x1, float y1, float x2, float y2, const Color& color, float thickness) {
    if (!sdlRenderer_) return;
    
    SDL_SetRenderDrawColor(sdlRenderer_, toU8(color.r), toU8(color.g), toU8(color.b), toU8(color.a));
    
    if (thickness <= 1.0f) {
        SDL_RenderDrawLine(sdlRenderer_, 
            static_cast<int>(x1), static_cast<int>(y1),
            static_cast<int>(x2), static_cast<int>(y2));
    } else {
        float dx = x2 - x1;
        float dy = y2 - y1;
        float len = std::sqrt(dx * dx + dy * dy);
        if (len < 0.001f) return;
        
        dx /= len;
        dy /= len;
        
        float px = -dy * thickness * 0.5f;
        float py = dx * thickness * 0.5f;
        
        SDL_Vertex vertices[4];
        vertices[0].position = { x1 + px, y1 + py };
        vertices[0].color = { toU8(color.r), toU8(color.g), toU8(color.b), toU8(color.a) };
        vertices[1].position = { x1 - px, y1 - py };
        vertices[1].color = { toU8(color.r), toU8(color.g), toU8(color.b), toU8(color.a) };
        vertices[2].position = { x2 - px, y2 - py };
        vertices[2].color = { toU8(color.r), toU8(color.g), toU8(color.b), toU8(color.a) };
        vertices[3].position = { x2 + px, y2 + py };
        vertices[3].color = { toU8(color.r), toU8(color.g), toU8(color.b), toU8(color.a) };
        
        int indices[6] = { 0, 1, 2, 0, 2, 3 };
        SDL_RenderGeometry(sdlRenderer_, nullptr, vertices, 4, indices, 6);
    }
}

// رسم مثلث ممتلئ
void DesktopRenderer::drawTriangle(float x1, float y1, float x2, float y2, float x3, float y3, const Color& color) {
    if (!sdlRenderer_) return;
    
    SDL_Vertex vertices[3];
    vertices[0].position = { x1, y1 };
    vertices[0].color = { toU8(color.r), toU8(color.g), toU8(color.b), toU8(color.a) };
    vertices[1].position = { x2, y2 };
    vertices[1].color = { toU8(color.r), toU8(color.g), toU8(color.b), toU8(color.a) };
    vertices[2].position = { x3, y3 };
    vertices[2].color = { toU8(color.r), toU8(color.g), toU8(color.b), toU8(color.a) };
    
    SDL_RenderGeometry(sdlRenderer_, nullptr, vertices, 3, nullptr, 0);
}

// رسم مثلث فارغ (حدود فقط)
void DesktopRenderer::drawTriangleOutline(float x1, float y1, float x2, float y2, float x3, float y3, const Color& color, float thickness) {
    drawLine(x1, y1, x2, y2, color, thickness);
    drawLine(x2, y2, x3, y3, color, thickness);
    drawLine(x3, y3, x1, y1, color, thickness);
}

// رسم مضلع ممتلئ
void DesktopRenderer::drawPolygon(const std::vector<std::pair<float, float>>& points, const Color& color) {
    if (!sdlRenderer_ || points.size() < 3) return;
    
    std::vector<SDL_Vertex> vertices(points.size());
    for (size_t i = 0; i < points.size(); i++) {
        vertices[i].position = { points[i].first, points[i].second };
        vertices[i].color = { toU8(color.r), toU8(color.g), toU8(color.b), toU8(color.a) };
    }
    
    std::vector<int> indices;
    for (size_t i = 1; i < points.size() - 1; i++) {
        indices.push_back(0);
        indices.push_back(static_cast<int>(i));
        indices.push_back(static_cast<int>(i + 1));
    }
    
    SDL_RenderGeometry(sdlRenderer_, nullptr, vertices.data(), static_cast<int>(vertices.size()),
                       indices.data(), static_cast<int>(indices.size()));
}

// رسم قوس
void DesktopRenderer::drawArc(float cx, float cy, float radius, float startAngle, float endAngle, const Color& color, float thickness) {
    if (!sdlRenderer_ || radius <= 0) return;
    
    int segments = static_cast<int>(radius * 0.5f);
    if (segments < 8) segments = 8;
    if (segments > 360) segments = 360;
    
    float angleStep = (endAngle - startAngle) / segments;
    float prevX = cx + radius * std::cos(startAngle);
    float prevY = cy + radius * std::sin(startAngle);
    
    for (int i = 1; i <= segments; i++) {
        float angle = startAngle + i * angleStep;
        float currX = cx + radius * std::cos(angle);
        float currY = cy + radius * std::sin(angle);
        
        drawLine(prevX, prevY, currX, currY, color, thickness);
        
        prevX = currX;
        prevY = currY;
    }
}

// رسم منحنى بيزيه تربيعي
void DesktopRenderer::drawBezier(float x1, float y1, float cx, float cy, float x2, float y2, const Color& color, float thickness) {
    if (!sdlRenderer_) return;
    
    float dist = std::sqrt((x2 - x1) * (x2 - x1) + (y2 - y1) * (y2 - y1));
    int segments = static_cast<int>(dist * 0.1f);
    if (segments < 8) segments = 8;
    if (segments > 100) segments = 100;
    
    float prevX = x1;
    float prevY = y1;
    
    for (int i = 1; i <= segments; i++) {
        float t = static_cast<float>(i) / segments;
        float t2 = t * t;
        float mt = 1.0f - t;
        float mt2 = mt * mt;
        
        float currX = mt2 * x1 + 2 * mt * t * cx + t2 * x2;
        float currY = mt2 * y1 + 2 * mt * t * cy + t2 * y2;
        
        drawLine(prevX, prevY, currX, currY, color, thickness);
        
        prevX = currX;
        prevY = currY;
    }
}

// رسم نقطة
void DesktopRenderer::drawPoint(float x, float y, const Color& color, float size) {
    if (!sdlRenderer_) return;
    
    if (size <= 1.0f) {
        SDL_SetRenderDrawColor(sdlRenderer_, toU8(color.r), toU8(color.g), toU8(color.b), toU8(color.a));
        SDL_RenderDrawPoint(sdlRenderer_, static_cast<int>(x), static_cast<int>(y));
    } else {
        drawCircle(x, y, size * 0.5f, color);
    }
}

// تعيين وضع المزج
void DesktopRenderer::setBlendMode(int mode) {
    if (!sdlRenderer_) return;
    
    SDL_BlendMode blendMode;
    switch (mode) {
        case 0:  blendMode = SDL_BLENDMODE_NONE; break;
        case 1:  blendMode = SDL_BLENDMODE_BLEND; break;
        case 2:  blendMode = SDL_BLENDMODE_ADD; break;
        case 3:  blendMode = SDL_BLENDMODE_MOD; break;
        default: blendMode = SDL_BLENDMODE_BLEND; break;
    }
    SDL_SetRenderDrawBlendMode(sdlRenderer_, blendMode);
}

// تعيين منطقة القص
void DesktopRenderer::setClipRect(const LayoutRect* rect) {
    if (!sdlRenderer_) return;
    
    if (rect == nullptr) {
        SDL_RenderSetClipRect(sdlRenderer_, nullptr);
    } else {
        SDL_Rect clipRect = {
            static_cast<int>(rect->x),
            static_cast<int>(rect->y),
            static_cast<int>(rect->width),
            static_cast<int>(rect->height)
        };
        SDL_RenderSetClipRect(sdlRenderer_, &clipRect);
    }
}

// ═══════════════════════════════════════════════════════════════════
// تطبيق واجهة PlatformRenderer (Override methods)
// ═══════════════════════════════════════════════════════════════════

bool DesktopRenderer::initialize(void* nativeSurface, const PlatformRenderOptions& opts) {
    options_ = opts;
    return initialize(static_cast<SDL_Renderer*>(nativeSurface), RenderOptions{
        opts.antialiasing, opts.vsync, opts.scaleFactor, opts.debugBounds, opts.debugLayout
    });
}

bool DesktopRenderer::isReady() const {
    return initialized_;
}

void DesktopRenderer::drawFilledRect(float x, float y, float w, float h, const Color& color) {
    LayoutRect r{x, y, w, h};
    drawFilledRect(r, color);
}

void DesktopRenderer::drawRoundedRect(float x, float y, float w, float h,
                                       const Color& color, float radius) {
    LayoutRect r{x, y, w, h};
    drawRoundedRect(r, color, radius);
}

void DesktopRenderer::drawRectOutline(float x, float y, float w, float h,
                                       const Color& color, float thickness) {
#ifdef SAD_UI_USE_SDL2
    if (!sdlRenderer_) return;
    SDL_SetRenderDrawColor(sdlRenderer_,
        static_cast<uint8_t>(color.r * 255),
        static_cast<uint8_t>(color.g * 255),
        static_cast<uint8_t>(color.b * 255),
        static_cast<uint8_t>(color.a * 255));
    for (float t = 0; t < thickness; t += 1.0f) {
        SDL_Rect r = {
            static_cast<int>(x + t), static_cast<int>(y + t),
            static_cast<int>(w - 2*t), static_cast<int>(h - 2*t)
        };
        SDL_RenderDrawRect(sdlRenderer_, &r);
    }
#else
    (void)x; (void)y; (void)w; (void)h; (void)color; (void)thickness;
#endif
}

std::pair<float, float> DesktopRenderer::drawText(
    const std::string& text, float x, float y,
    const Color& color, float fontSize
) {
    drawTextInternal(text, x, y, color, fontSize);
    return measureText(text, fontSize);
}

std::pair<float, float> DesktopRenderer::measureText(
    const std::string& text, float fontSize
) {
#ifdef SAD_UI_HAS_SDL_TTF
    TTF_Font* font = getFontForSize(fontSize);
    if (!font) font = defaultFont_;
    if (font) {
        int w = 0, h = 0;
        TTF_SizeUTF8(font, text.c_str(), &w, &h);
        return {static_cast<float>(w), static_cast<float>(h)};
    }
#endif
    float approxW = utf8CharCount(text) * fontSize * 0.55f;
    return {approxW, fontSize};
}

void DesktopRenderer::drawImage(const std::string& path,
                                 float x, float y, float w, float h) {
    LayoutRect r{x, y, w, h};
    drawImage(path, r);
}

void DesktopRenderer::drawLinearGradient(float x, float y, float w, float h,
                                          const Color& startColor,
                                          const Color& endColor,
                                          bool vertical, float radius) {
    LayoutRect r{x, y, w, h};
    drawLinearGradient(r, startColor, endColor, vertical, radius);
}

void DesktopRenderer::setClipRect(float x, float y, float w, float h) {
    LayoutRect r{x, y, w, h};
    setClipRect(&r);
}

void DesktopRenderer::clearClipRect() {
    setClipRect(nullptr);
}

} // namespace desktop
} // namespace ui
} // namespace sad
