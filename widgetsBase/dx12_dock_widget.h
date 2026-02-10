#pragma once

#include "dock_framework.h"
#include "dock_theme.h"
#include "dx12_canvas.h"
#include "icon_module.h"
#include <algorithm>

namespace df {

class DX12DockWidget : public DockWidget {
public:
    using DockWidget::DockWidget;
    static constexpr float TITLE_BAR_HEIGHT = 28.0f;
    static constexpr float CLOSE_BUTTON_SIZE = 14.0f;
    static constexpr float CLOSE_BUTTON_MARGIN = 6.0f;
    static constexpr float UNDOCK_BUTTON_SIZE = 14.0f;
    static constexpr float UNDOCK_BUTTON_MARGIN = 4.0f;

    static DFRect CloseButtonRect(const DFRect& titleBar)
    {
        return {
            titleBar.x + titleBar.width - CLOSE_BUTTON_SIZE - CLOSE_BUTTON_MARGIN,
            titleBar.y + (titleBar.height - CLOSE_BUTTON_SIZE) * 0.5f,
            CLOSE_BUTTON_SIZE,
            CLOSE_BUTTON_SIZE
        };
    }

    static DFRect UndockButtonRect(const DFRect& titleBar)
    {
        const DFRect closeRect = CloseButtonRect(titleBar);
        return {
            closeRect.x - UNDOCK_BUTTON_MARGIN - UNDOCK_BUTTON_SIZE,
            titleBar.y + (titleBar.height - UNDOCK_BUTTON_SIZE) * 0.5f,
            UNDOCK_BUTTON_SIZE,
            UNDOCK_BUTTON_SIZE
        };
    }

    DFSize minimumSize() const override {
        DFSize min = DockWidget::minimumSize();
        if (isDocked() && isSingleDocked()) {
            min.height += TITLE_BAR_HEIGHT;
        }
        return min;
    }

    void paint(Canvas& canvas) override {
        auto* dx12 = dynamic_cast<DX12Canvas*>(&canvas);
        if (!dx12) return;
        const auto& theme = df::CurrentTheme();

        const DFRect& b = bounds();
        const bool showTitleBar = isDocked() && isSingleDocked();
        const float topOffset = showTitleBar ? TITLE_BAR_HEIGHT : 0.0f;
        if (showTitleBar) {
            const DFRect titleBar{b.x, b.y, b.width, TITLE_BAR_HEIGHT};
            dx12->drawRectangle(titleBar, theme.dockTitleBar);
            const DFRect undockRect = UndockButtonRect(titleBar);
            const DFRect closeRect = CloseButtonRect(titleBar);
            dx12->drawRectangle(undockRect, theme.overlayAccentSoft);
            dx12->drawRectangle(closeRect, theme.floatingCloseButton);
            DrawDockIcon(canvas, DockIcon::Undock, undockRect, {0.93f, 0.96f, 1.00f, 0.95f}, 2.0f);
            DrawDockIcon(canvas, DockIcon::Close, closeRect, {0.95f, 0.97f, 1.00f, 0.95f}, 2.0f);
        }

        DFRect background{b.x, b.y + topOffset, b.width, std::max(0.0f, b.height - topOffset)};
        dx12->drawRectangle(background, theme.dockBackground);

        // simple border
        dx12->drawRectangle({b.x, b.y, b.width, 1.0f}, theme.dockBorder);
        dx12->drawRectangle({b.x, b.y + b.height - 1.0f, b.width, 1.0f}, theme.dockBorder);
        dx12->drawRectangle({b.x, b.y, 1.0f, b.height}, theme.dockBorder);
        dx12->drawRectangle({b.x + b.width - 1.0f, b.y, 1.0f, b.height}, theme.dockBorder);

        if (content()) {
            content()->setBounds(background);
            content()->paint(canvas);
        }
    }

    void handleEvent(Event& event) override {
        const DFRect& b = bounds();

        const bool showTitleBar = isDocked() && isSingleDocked();
        const float topOffset = showTitleBar ? TITLE_BAR_HEIGHT : 0.0f;

        DFPoint p{event.x, event.y};
        if (event.type == Event::Type::MouseDown && showTitleBar) {
            const DFRect titleBar{b.x, b.y, b.width, TITLE_BAR_HEIGHT};
            if (titleBar.contains(p)) {
                if (CloseButtonRect(titleBar).contains(p)) {
                    df::DockManager::instance().closeWidget(this);
                    event.handled = true;
                    return;
                }
                if (UndockButtonRect(titleBar).contains(p)) {
                    df::DockManager::instance().startUndockDrag(this, p);
                    event.handled = true;
                    return;
                }
                df::DockManager::instance().startDrag(this, p);
                event.handled = true;
                return;
            }
        }

        if (!content()) return;

        // Forward to content with local coordinates (account for title height)
        Event local = event;
        local.x -= b.x;
        local.y -= (b.y + topOffset);
        content()->handleEvent(local);
        if (local.handled) event.handled = true;
    }
};

} // namespace df

