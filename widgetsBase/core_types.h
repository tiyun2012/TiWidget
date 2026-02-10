// Engine-agnostic placeholder types; rename avoids Win32 name clashes.
#pragma once
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

struct DFSize {
    float width = 0;
    float height = 0;
};

class Canvas {
public:
    virtual ~Canvas() = default;
    virtual void drawRectangle(const DFRect&, const DFColor&) {}
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

