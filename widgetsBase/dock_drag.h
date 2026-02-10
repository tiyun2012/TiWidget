#pragma once

#include <vector>
#include <algorithm>
#include "core_types.h"
#include "icon_module.h"

namespace df {

class DockWidget;

class DragOverlay {
public:
    enum class DropZone { None, Left, Right, Top, Bottom, Center, Tab };

    void render(Canvas& canvas) {
        if (!visible_) return;

        auto zoneFill = [&](DropZone type) -> DFColor {
            switch (type) {
            case DropZone::Center:
            case DropZone::Tab:
                return DFColor{0.18f, 0.62f, 0.92f, 0.55f};
            case DropZone::Left:
            case DropZone::Right:
            case DropZone::Top:
            case DropZone::Bottom:
                return DFColor{0.82f, 0.66f, 0.30f, 0.70f};
            case DropZone::None:
            default:
                return highlightColor_;
            }
        };

        auto zoneOutline = [&](DropZone type) -> DFColor {
            switch (type) {
            case DropZone::Center:
            case DropZone::Tab:
                return DFColor{0.32f, 0.82f, 1.00f, 0.88f};
            case DropZone::Left:
            case DropZone::Right:
            case DropZone::Top:
            case DropZone::Bottom:
                return DFColor{1.00f, 0.84f, 0.44f, 0.96f};
            case DropZone::None:
            default:
                return highlightColor_;
            }
        };

        for (const auto& zone : dropZones_) {
            // Important: DX12Canvas path is non-blended. "Transparent" colors still
            // write RGB, causing dark ghost hints. Render only active candidate.
            if (!zone.highlighted) {
                continue;
            }

            const DFColor fill = zoneFill(zone.type);
            const DFColor outline = zoneOutline(zone.type);
            canvas.drawRectangle(zone.bounds, fill);

            const float t = 2.0f;
            canvas.drawRectangle({zone.bounds.x, zone.bounds.y, zone.bounds.width, t}, outline);
            canvas.drawRectangle({zone.bounds.x, zone.bounds.y + zone.bounds.height - t, zone.bounds.width, t}, outline);
            canvas.drawRectangle({zone.bounds.x, zone.bounds.y, t, zone.bounds.height}, outline);
            canvas.drawRectangle({zone.bounds.x + zone.bounds.width - t, zone.bounds.y, t, zone.bounds.height}, outline);

            if (zone.type == DropZone::Center || zone.type == DropZone::Tab) {
                const float cx = zone.bounds.x + zone.bounds.width * 0.5f;
                const float cy = zone.bounds.y + zone.bounds.height * 0.5f;
                const float arm = std::min(zone.bounds.width, zone.bounds.height) * 0.24f;
                const float crossW = 2.0f;
                canvas.drawRectangle({cx - arm, cy - crossW * 0.5f, arm * 2.0f, crossW}, outline);
                canvas.drawRectangle({cx - crossW * 0.5f, cy - arm, crossW, arm * 2.0f}, outline);
                DrawDockIcon(canvas, DockIcon::Tabify, zone.bounds, outline, 3.0f);
            } else {
                const float iconThickness = 3.0f;
                if (zone.type == DropZone::Left) {
                    DrawDockIcon(canvas, DockIcon::SplitLeft, zone.bounds, outline, iconThickness);
                } else if (zone.type == DropZone::Right) {
                    DrawDockIcon(canvas, DockIcon::SplitRight, zone.bounds, outline, iconThickness);
                } else if (zone.type == DropZone::Top) {
                    DrawDockIcon(canvas, DockIcon::SplitTop, zone.bounds, outline, iconThickness);
                } else if (zone.type == DropZone::Bottom) {
                    DrawDockIcon(canvas, DockIcon::SplitBottom, zone.bounds, outline, iconThickness);
                }
            }
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
    DFColor highlightColor_{0.2f, 0.6f, 1.0f, 0.35f};
    DFColor zoneColor_{0.2f, 0.2f, 0.2f, 0.2f};
    DFColor previewColor_{0.8f, 0.8f, 1.0f, 0.4f};
};

} // namespace df

