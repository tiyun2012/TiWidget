#pragma once

#include "dock_framework.h"
#include <algorithm>

namespace df {

class BasicDockWidget : public DockWidget {
public:
    using DockWidget::DockWidget;
    static constexpr float TITLE_BAR_HEIGHT = 24.0f;

    DFSize minimumSize() const override {
        DFSize min = DockWidget::minimumSize();
        if (isDocked() && isSingleDocked()) {
            min.height += TITLE_BAR_HEIGHT;
        }
        return min;
    }

    void paint(Canvas& canvas) override {
        const DFRect b = bounds();
        // background
        canvas.drawRectangle(b, {0.2f, 0.2f, 0.25f, 1.0f});
        // title bar
        DFRect title{b.x, b.y, b.width, TITLE_BAR_HEIGHT};
        canvas.drawRectangle(title, {0.35f, 0.35f, 0.4f, 1.0f});
        // content area
        DFRect contentArea{b.x, b.y + TITLE_BAR_HEIGHT, b.width, std::max(0.0f, b.height - TITLE_BAR_HEIGHT)};
        canvas.drawRectangle(contentArea, {0.15f, 0.15f, 0.2f, 1.0f});
        if (content()) {
            content()->setBounds(contentArea);
            content()->paint(canvas);
        }
    }

    void handleEvent(Event& event) override {
        if (event.type == Event::Type::MouseDown) {
            DFPoint mousePos{event.x, event.y};
            const DFRect b = bounds();
            DFRect title{b.x, b.y, b.width, TITLE_BAR_HEIGHT};
            if (title.contains(mousePos)) {
                DockManager::instance().startDrag(this, mousePos);
                event.handled = true;
            }
        }
    }
};

} // namespace df

