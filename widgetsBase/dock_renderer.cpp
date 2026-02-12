#include "dock_renderer.h"
#include "icon_module.h"

#include <algorithm>

namespace df {

DFRect DockRenderer::tabCloseRect(const DFRect& tabRect)
{
    const float iconSize = 12.0f;
    const float iconMargin = 6.0f;
    return {
        tabRect.x + tabRect.width - iconSize - iconMargin - iconSize * 0.5f,
        tabRect.y + (tabRect.height - iconSize) * 0.5f,
        iconSize,
        iconSize
    };
}

void DockRenderer::drawTitlePlaceholder(Canvas& canvas, const DFRect& tabRect, const DFRect& closeRect, const DFColor& color)
{
    const float paddingLeft = 8.0f;
    const float paddingRight = 6.0f;
    const float startX = tabRect.x + paddingLeft;
    const float endX = std::max(startX, closeRect.x - paddingRight);
    const float available = endX - startX;
    if (available < 8.0f) {
        return;
    }
    const float h = std::max(4.0f, std::min(8.0f, tabRect.height * 0.30f));
    const float y = tabRect.y + (tabRect.height - h) * 0.5f;
    const float w = std::max(10.0f, std::min(available, std::max(24.0f, available * 0.72f)));
    canvas.drawRectangle({startX, y, w, h}, color);
}

void DockRenderer::render(Canvas& canvas, DockLayout::Node* node)
{
    if (!node) return;
    renderNode(canvas, node, CurrentTheme());
}

void DockRenderer::renderNode(Canvas& canvas, DockLayout::Node* node, const DockTheme& theme)
{
    if (!node) return;
    (void)theme;

    if (node->type == DockLayout::Node::Type::Widget) {
        if (node->widget) {
            node->widget->paint(canvas);
        }
        return;
    }

    if (node->type == DockLayout::Node::Type::Tab) {
        for (const auto& child : node->children) {
            renderNode(canvas, child.get(), theme);
        }
        return;
    }

    if (node->type == DockLayout::Node::Type::Split) {
        renderNode(canvas, node->first.get(), theme);
        renderNode(canvas, node->second.get(), theme);
    }
}

} // namespace df
