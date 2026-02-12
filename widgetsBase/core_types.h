// Engine-agnostic placeholder types; rename avoids Win32 name clashes.
#pragma once
#include <algorithm>
#include <cstdint>
#include <string>

struct DFPoint {
    float x = 0;
    float y = 0;
};

struct DFRect {
    float x = 0;
    float y = 0;
    float width = 0;
    float height = 0;

    bool contains(const DFPoint& p) const {
        return p.x >= x && p.x <= x + width && p.y >= y && p.y <= y + height;
    }
};

class Event {
public:
    enum class Type { Unknown, MouseDown, MouseUp, MouseMove, KeyDown, KeyUp, Close };
    explicit Event(Type t = Type::Unknown) : type(t) {}
    Type type = Type::Unknown;
    float x = 0.0f;
    float y = 0.0f;
    int key = 0;
    bool handled = false;
};

struct DFColor {
    float r = 1, g = 1, b = 1, a = 1;
};

inline constexpr DFColor DFColorFromHex(uint32_t rgb, float alpha = 1.0f)
{
    return DFColor{
        static_cast<float>((rgb >> 16) & 0xFFu) / 255.0f,
        static_cast<float>((rgb >> 8) & 0xFFu) / 255.0f,
        static_cast<float>(rgb & 0xFFu) / 255.0f,
        alpha
    };
}

struct DFSize {
    float width = 0;
    float height = 0;
};

class Canvas {
public:
    virtual ~Canvas() = default;
    virtual void drawRectangle(const DFRect&, const DFColor&) {}
    virtual void drawRoundedRectangle(const DFRect& rect, float /*radius*/, const DFColor& color) { drawRectangle(rect, color); }
    virtual void drawRoundedRectangleOutline(const DFRect& rect, float /*radius*/, const DFColor& color, float thickness = 1.0f)
    {
        if (rect.width <= 0.0f || rect.height <= 0.0f) {
            return;
        }
        const float t = std::max(1.0f, thickness);
        drawRectangle({rect.x, rect.y, rect.width, t}, color);
        drawRectangle({rect.x, rect.y + std::max(0.0f, rect.height - t), rect.width, t}, color);
        drawRectangle({rect.x, rect.y, t, rect.height}, color);
        drawRectangle({rect.x + std::max(0.0f, rect.width - t), rect.y, t, rect.height}, color);
    }
    virtual void drawLine(const DFPoint&, const DFPoint&, const DFColor&, float /*thickness*/ = 1.0f) {}
    virtual void drawText(float /*x*/, float /*y*/, const std::string& /*text*/, const DFColor& /*color*/) {}
};

class Widget {
public:
    virtual ~Widget() = default;
    virtual void setBounds(const DFRect& r) { bounds_ = r; }
    virtual void paint(Canvas& /*canvas*/) {}
    virtual void handleEvent(Event& /*event*/) {}
    virtual DFSize minimumSize() const { return {}; }
    const DFRect& bounds() const { return bounds_; }
protected:
    DFRect bounds_{};
};

