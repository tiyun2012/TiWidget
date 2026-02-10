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

    if (node->type == DockLayout::Node::Type::Tab && !node->children.empty()) {
        DFRect bar{node->bounds.x, node->bounds.y, node->bounds.width, node->tabBarHeight};
        canvas.drawRectangle(bar, theme.tabStrip);

        const float count = static_cast<float>(node->children.size());
        const float tabWidth = (count > 0.0f) ? (node->bounds.width / count) : node->bounds.width;
        for (size_t i = 0; i < node->children.size(); ++i) {
            DFRect tabRect{
                node->bounds.x + static_cast<float>(i) * tabWidth,
                node->bounds.y,
                tabWidth,
                node->tabBarHeight
            };
            const bool active = (static_cast<int>(i) == node->activeTab);
            const bool tabHovered = hasMousePos_ && tabRect.contains(mousePos_);
            DFColor tabColor = active ? theme.tabActive : theme.tabInactive;
            if (active) {
                tabColor.r = std::min(1.0f, tabColor.r + 0.04f);
                tabColor.g = std::min(1.0f, tabColor.g + 0.04f);
                tabColor.b = std::min(1.0f, tabColor.b + 0.04f);
            }
            if (tabHovered) {
                tabColor.r = std::min(1.0f, tabColor.r + 0.08f);
                tabColor.g = std::min(1.0f, tabColor.g + 0.08f);
                tabColor.b = std::min(1.0f, tabColor.b + 0.08f);
            }
            canvas.drawRectangle(tabRect, tabColor);

            const DFRect closeRect = tabCloseRect(tabRect);
            const bool closeHovered = hasMousePos_ && closeRect.contains(mousePos_);
            const DFColor closeColor = closeHovered
                ? DFColor{1.0f, 0.3f, 0.3f, 1.0f}
                : DFColor{0.8f, 0.2f, 0.2f, 1.0f};
            const float thickness = closeHovered ? 2.5f : 2.0f;
            drawDockIcon(canvas, DockIcon::Close, closeRect, closeColor, thickness);

            const std::string title = (node->children[i] && node->children[i]->widget)
                ? node->children[i]->widget->title()
                : "Untitled";
            const DFColor textColor = active
                ? DFColor{1.0f, 1.0f, 1.0f, 1.0f}
                : DFColor{0.8f, 0.8f, 0.8f, 1.0f};
            const float textX = tabRect.x + 10.0f;
            const float textY = tabRect.y + (tabRect.height - 16.0f) * 0.5f;
            canvas.drawText(textX, textY, title, textColor);
            drawTitlePlaceholder(canvas, tabRect, closeRect, textColor);
        }
    }

    if (node->type == DockLayout::Node::Type::Widget) {
        if (node->widget) {
            node->widget->paint(canvas);
        }
        return;
    }

    if (node->type == DockLayout::Node::Type::Tab) {
        if (!node->children.empty()) {
            const int active = std::clamp(node->activeTab, 0, static_cast<int>(node->children.size()) - 1);
            renderNode(canvas, node->children[active].get(), theme);
        }
        return;
    }

    if (node->type == DockLayout::Node::Type::Split) {
        renderNode(canvas, node->first.get(), theme);
        renderNode(canvas, node->second.get(), theme);
    }
}

} // namespace df
