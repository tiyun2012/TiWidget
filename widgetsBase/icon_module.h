#pragma once

#include <algorithm>
#include <cmath>

#include "core_types.h"
#include "icons/IconsFontAwesome6.h"

namespace df {

enum class DockIcon {
    Close,
    Undock,
    Tabify,
    SplitLeft,
    SplitRight,
    SplitTop,
    SplitBottom
};

inline const char* DockIconGlyph(DockIcon icon)
{
    switch (icon) {
    case DockIcon::Close:
        return ICON_FA_XMARK;
    case DockIcon::Undock:
        return ICON_FA_UP_RIGHT_FROM_SQUARE;
    case DockIcon::Tabify:
        return ICON_FA_PLUS;
    case DockIcon::SplitLeft:
        return ICON_FA_ARROW_LEFT;
    case DockIcon::SplitRight:
        return ICON_FA_ARROW_RIGHT;
    case DockIcon::SplitTop:
        return ICON_FA_ARROW_UP;
    case DockIcon::SplitBottom:
        return ICON_FA_ARROW_DOWN;
    default:
        return ICON_FA_PLUS;
    }
}

inline void DrawDockIcon(Canvas& canvas, DockIcon icon, const DFRect& bounds, const DFColor& color, float thickness = 2.0f)
{
    const float cx = bounds.x + bounds.width * 0.5f;
    const float cy = bounds.y + bounds.height * 0.5f;
    const float arm = std::max(3.0f, std::min(bounds.width, bounds.height) * 0.30f);
    const float tip = std::max(2.0f, arm * 0.65f);

    switch (icon) {
    case DockIcon::Close:
        canvas.drawLine({cx - arm, cy - arm}, {cx + arm, cy + arm}, color, thickness);
        canvas.drawLine({cx - arm, cy + arm}, {cx + arm, cy - arm}, color, thickness);
        break;
    case DockIcon::Tabify:
        canvas.drawLine({cx - arm, cy}, {cx + arm, cy}, color, thickness);
        canvas.drawLine({cx, cy - arm}, {cx, cy + arm}, color, thickness);
        break;
    case DockIcon::SplitLeft:
        canvas.drawLine({cx + arm, cy - tip}, {cx - arm, cy}, color, thickness);
        canvas.drawLine({cx + arm, cy + tip}, {cx - arm, cy}, color, thickness);
        break;
    case DockIcon::SplitRight:
        canvas.drawLine({cx - arm, cy - tip}, {cx + arm, cy}, color, thickness);
        canvas.drawLine({cx - arm, cy + tip}, {cx + arm, cy}, color, thickness);
        break;
    case DockIcon::SplitTop:
        canvas.drawLine({cx - tip, cy + arm}, {cx, cy - arm}, color, thickness);
        canvas.drawLine({cx + tip, cy + arm}, {cx, cy - arm}, color, thickness);
        break;
    case DockIcon::SplitBottom:
        canvas.drawLine({cx - tip, cy - arm}, {cx, cy + arm}, color, thickness);
        canvas.drawLine({cx + tip, cy - arm}, {cx, cy + arm}, color, thickness);
        break;
    case DockIcon::Undock: {
        // Box corner + outgoing arrow (Qt-like "pop out" cue).
        const float corner = std::max(2.0f, arm * 0.9f);
        canvas.drawLine({cx - corner, cy + corner}, {cx + corner, cy + corner}, color, thickness);
        canvas.drawLine({cx - corner, cy + corner}, {cx - corner, cy - corner}, color, thickness);
        canvas.drawLine({cx - arm * 0.2f, cy + arm * 0.2f}, {cx + arm * 0.9f, cy - arm * 0.9f}, color, thickness);
        canvas.drawLine({cx + arm * 0.3f, cy - arm * 0.9f}, {cx + arm * 0.9f, cy - arm * 0.9f}, color, thickness);
        canvas.drawLine({cx + arm * 0.9f, cy - arm * 0.3f}, {cx + arm * 0.9f, cy - arm * 0.9f}, color, thickness);
        break;
    }
    default:
        break;
    }
}

inline void drawDockIcon(Canvas& canvas, DockIcon icon, const DFRect& bounds, const DFColor& color, float thickness = 2.0f)
{
    DrawDockIcon(canvas, icon, bounds, color, thickness);
}

inline void drawDockIcon(Canvas& canvas, DockIcon icon, const DFPoint& center, float size, const DFColor& color, float thickness = 2.0f)
{
    const float half = std::max(1.0f, size * 0.5f);
    DrawDockIcon(canvas, icon, {center.x - half, center.y - half, half * 2.0f, half * 2.0f}, color, thickness);
}

} // namespace df
