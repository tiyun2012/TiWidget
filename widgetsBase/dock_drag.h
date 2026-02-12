#pragma once

#include <vector>
#include <algorithm>
#include "core_types.h"

namespace df {

class DockWidget;

class DragOverlay {
public:
    enum class DropZone { None, Left, Right, Top, Bottom, Center, Tab };

    void render(Canvas& canvas) {
        if (!visible_) return;
        const DFColor edgeColor{0.2f, 0.6f, 1.0f, 1.0f};
        const float edgeThickness = 4.0f;

        for (const auto& zone : dropZones_) {
            // Important: DX12Canvas path is non-blended. "Transparent" colors still
            // write RGB, causing dark ghost hints. Render only active candidate.
            if (!zone.highlighted) {
                continue;
            }

            const DFRect& r = zone.bounds;
            canvas.drawLine({r.x, r.y}, {r.x + r.width, r.y}, edgeColor, edgeThickness);
            canvas.drawLine({r.x, r.y + r.height}, {r.x + r.width, r.y + r.height}, edgeColor, edgeThickness);
            canvas.drawLine({r.x, r.y}, {r.x, r.y + r.height}, edgeColor, edgeThickness);
            canvas.drawLine({r.x + r.width, r.y}, {r.x + r.width, r.y + r.height}, edgeColor, edgeThickness);
        }
        if (draggedWidget_) {
            canvas.drawRectangle(dragPreview_, previewColor_);
            const DFColor outline{0.90f, 0.94f, 1.0f, 0.85f};
            const float t = 2.0f;
            canvas.drawRectangle({dragPreview_.x, dragPreview_.y, dragPreview_.width, t}, outline);
            canvas.drawRectangle({dragPreview_.x, dragPreview_.y + dragPreview_.height - t, dragPreview_.width, t}, outline);
            canvas.drawRectangle({dragPreview_.x, dragPreview_.y, t, dragPreview_.height}, outline);
            canvas.drawRectangle({dragPreview_.x + dragPreview_.width - t, dragPreview_.y, t, dragPreview_.height}, outline);
        }
    }

    DropZone findZone(const DFPoint& p) const {
        for (const auto& zone : dropZones_) {
            if (zone.bounds.contains(p)) return zone.type;
        }
        return DropZone::None;
    }

    void clearZones() { dropZones_.clear(); }
    size_t addZone(const DFRect& bounds, DropZone type) {
        dropZones_.push_back({type, bounds, false});
        return dropZones_.size() - 1;
    }
    void highlightZone(DropZone zone) {
        for (auto& item : dropZones_) {
            item.highlighted = (item.type == zone && zone != DropZone::None);
        }
    }
    void highlightZoneIndex(size_t index) {
        for (size_t i = 0; i < dropZones_.size(); ++i) {
            dropZones_[i].highlighted = (i == index);
        }
    }

    void setVisible(bool v) { visible_ = v; }
    bool visible() const { return visible_; }
    void setDraggedWidget(DockWidget* w) { draggedWidget_ = w; }
    void setPreview(const DFRect& r) { dragPreview_ = r; }

private:
    struct Zone {
        DropZone type;
        DFRect bounds;
        bool highlighted = false;
    };

    std::vector<Zone> dropZones_;
    DFRect dragPreview_{};
    DockWidget* draggedWidget_ = nullptr;
    bool visible_ = false;
    DFColor previewColor_{0.8f, 0.8f, 1.0f, 0.4f};
};

} // namespace df

