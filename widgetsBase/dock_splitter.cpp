#include "dock_splitter.h"
#include "dock_theme.h"
#include <algorithm>
#include <cmath>

namespace df {

void DockSplitter::updateSplitters(DockLayout::Node* root, const DFRect& containerBounds)
{
    splitters_.clear();
    collectSplitters(root, containerBounds);

    // If layout changed while dragging, drop stale drag state.
    if (activeNode_) {
        bool found = false;
        for (const auto& splitter : splitters_) {
            if (splitter.node == activeNode_) {
                found = true;
                break;
            }
        }
        if (!found) {
            activeNode_ = nullptr;
            activeGrabOffset_ = 0.0f;
        }
    }
    if (hoveredNode_) {
        bool foundHover = false;
        for (const auto& splitter : splitters_) {
            if (splitter.node == hoveredNode_) {
                foundHover = true;
                break;
            }
        }
        if (!foundHover) {
            hoveredNode_ = nullptr;
        }
    }
}

void DockSplitter::collectSplitters(DockLayout::Node* node, const DFRect& bounds)
{
    if (!node || node->type != DockLayout::Node::Type::Split) return;

    Splitter splitter;
    splitter.node = node;
    splitter.vertical = node->vertical;
    splitter.position = node->ratio;
    splitter.parentBounds = bounds;
    splitter.dragging = (activeNode_ != nullptr && activeNode_ == node);

    // Qt 6 style: Splitters are visually thin (often 1px or 4px) but have a standard logic
    if (splitter.vertical) {
        float splitX = bounds.x + bounds.width * node->ratio;
        // Center the splitter visual on the split line
        splitter.bounds = {splitX - SPLITTER_THICKNESS * 0.5f, bounds.y, SPLITTER_THICKNESS, bounds.height};

        DFRect first{bounds.x, bounds.y, splitX - bounds.x, bounds.height};
        DFRect second{splitX, bounds.y, bounds.x + bounds.width - splitX, bounds.height};
        collectSplitters(node->first.get(), first);
        collectSplitters(node->second.get(), second);
    } else {
        float splitY = bounds.y + bounds.height * node->ratio;
        splitter.bounds = {bounds.x, splitY - SPLITTER_THICKNESS * 0.5f, bounds.width, SPLITTER_THICKNESS};

        DFRect first{bounds.x, bounds.y, bounds.width, splitY - bounds.y};
        DFRect second{bounds.x, splitY, bounds.width, bounds.y + bounds.height - splitY};
        collectSplitters(node->first.get(), first);
        collectSplitters(node->second.get(), second);
    }

    splitters_.push_back(splitter);
}

DockSplitter::Splitter* DockSplitter::splitterAtPoint(const DFPoint& p)
{
    for (auto& s : splitters_) {
        DFRect expanded = s.bounds;
        // Qt 6 style: Invisible hover padding makes grabbing thin splitters easier
        if (s.vertical) {
            expanded.x -= (SPLITTER_HOVER_THICKNESS - SPLITTER_THICKNESS) * 0.5f;
            expanded.width = SPLITTER_HOVER_THICKNESS;
        } else {
            expanded.y -= (SPLITTER_HOVER_THICKNESS - SPLITTER_THICKNESS) * 0.5f;
            expanded.height = SPLITTER_HOVER_THICKNESS;
        }
        if (expanded.contains(p)) return &s;
    }
    return nullptr;
}

void DockSplitter::startDrag(Splitter* splitter, const DFPoint& p)
{
    if (!splitter) return;
    activeNode_ = splitter->node;
    activeVertical_ = splitter->vertical;
    activeParentBounds_ = splitter->parentBounds;
    if (activeVertical_) {
        const float splitX = activeParentBounds_.x + activeParentBounds_.width * activeNode_->ratio;
        activeGrabOffset_ = p.x - splitX;
    } else {
        const float splitY = activeParentBounds_.y + activeParentBounds_.height * activeNode_->ratio;
        activeGrabOffset_ = p.y - splitY;
    }
    splitter->dragging = true;
}

void DockSplitter::updateDrag(const DFPoint& p)
{
    if (!activeNode_) return;

    // Refresh active splitter bounds every event so drag stays correct
    // even after per-frame splitter list rebuild.
    for (const auto& splitter : splitters_) {
        if (splitter.node == activeNode_) {
            activeVertical_ = splitter.vertical;
            activeParentBounds_ = splitter.parentBounds;
            break;
        }
    }

    const float total = activeVertical_ ? activeParentBounds_.width : activeParentBounds_.height;
    if (total <= 0.0f) return;

    // Use the values calculated by DockLayout::recalculateMinSizes
    // This ensures the splitter stops exactly where the content says it must.
    float minFirst = std::clamp(activeNode_->minFirstSize, 0.0f, total);
    float minSecond = std::clamp(activeNode_->minSecondSize, 0.0f, total);
    const float minSum = minFirst + minSecond;
    if (minSum > total) {
        // Handle compression when window is too small
        const float scale = total / minSum;
        minFirst *= scale;
        minSecond *= scale;
    }

    float firstSize = 0.0f;
    if (activeVertical_) {
        const float splitX = p.x - activeGrabOffset_;
        firstSize = splitX - activeParentBounds_.x;
    } else {
        const float splitY = p.y - activeGrabOffset_;
        firstSize = splitY - activeParentBounds_.y;
    }
    float maxFirst = std::max(0.0f, total - minSecond);
    if (maxFirst < minFirst) {
        maxFirst = minFirst;
    }

    // Qt-style constraint: Clamp strictly to min/max
    firstSize = std::clamp(firstSize, minFirst, maxFirst);
    const float secondSize = total - firstSize;

    // Update the node state
    activeNode_->ratio = (total > 0.0f) ? (firstSize / total) : 0.5f;
    if (activeNode_->splitSizing == DockLayout::Node::SplitSizing::FixedFirst) {
        activeNode_->fixedSize = firstSize;
    } else if (activeNode_->splitSizing == DockLayout::Node::SplitSizing::FixedSecond) {
        activeNode_->fixedSize = secondSize;
    }
}

void DockSplitter::endDrag()
{
    activeNode_ = nullptr;
    activeGrabOffset_ = 0.0f;
}

void DockSplitter::render(Canvas& canvas)
{
    const auto& theme = CurrentTheme();
    if (!theme.drawSplitter) {
        return;
    }
    for (const auto& s : splitters_) {
        const bool dragging = (activeNode_ != nullptr && activeNode_ == s.node);
        const bool hovered = (hoveredNode_ != nullptr && hoveredNode_ == s.node);

        const DFColor lineColor = theme.splitter;
        DFColor handleColor = theme.splitter;
        if (theme.drawSplitterStateColors) {
            if (dragging) {
                handleColor = theme.splitterDrag;
            } else if (hovered) {
                handleColor = theme.splitterHover;
            }
        }

        const float luminance =
            handleColor.r * 0.2126f +
            handleColor.g * 0.7152f +
            handleColor.b * 0.0722f;
        const DFColor dotColor = (luminance > 0.55f)
            ? DFColor{0.10f, 0.11f, 0.12f, 1.0f}
            : DFColor{0.93f, 0.94f, 0.96f, 1.0f};

        if (s.vertical) {
            const float centerX = s.bounds.x + s.bounds.width * 0.5f;
            const float handleW = std::min(s.bounds.width, std::clamp(s.bounds.width + 3.0f, 7.0f, 12.0f));
            const float handleH = std::min(s.bounds.height, std::clamp(s.bounds.height * 0.16f, 48.0f, 140.0f));
            const DFRect handle{
                centerX - handleW * 0.5f,
                s.bounds.y + (s.bounds.height - handleH) * 0.5f,
                handleW,
                handleH
            };

            const float gap = 3.0f;
            const float topEnd = handle.y - gap;
            const float bottomStart = handle.y + handle.height + gap;
            if (topEnd > s.bounds.y) {
                canvas.drawLine({centerX, s.bounds.y}, {centerX, topEnd}, lineColor, 1.0f);
            }
            if (bottomStart < s.bounds.y + s.bounds.height) {
                canvas.drawLine({centerX, bottomStart}, {centerX, s.bounds.y + s.bounds.height}, lineColor, 1.0f);
            }

            const float radius = std::min(handle.width, handle.height) * 0.45f;
            canvas.drawRoundedRectangle(handle, radius, handleColor);

            const float dotSize = std::clamp(handleW * 0.34f, 1.8f, 2.8f);
            const float dotStep = dotSize * 2.0f;
            const float dotX = centerX - dotSize * 0.5f;
            const float dotStartY = handle.y + handle.height * 0.5f - dotStep;
            for (int i = 0; i < 3; ++i) {
                canvas.drawRectangle({dotX, dotStartY + i * dotStep, dotSize, dotSize}, dotColor);
            }
            continue;
        }

        const float centerY = s.bounds.y + s.bounds.height * 0.5f;
        const float handleH = std::min(s.bounds.height, std::clamp(s.bounds.height + 3.0f, 7.0f, 12.0f));
        const float handleW = std::min(s.bounds.width, std::clamp(s.bounds.width * 0.16f, 48.0f, 140.0f));
        const DFRect handle{
            s.bounds.x + (s.bounds.width - handleW) * 0.5f,
            centerY - handleH * 0.5f,
            handleW,
            handleH
        };

        const float gap = 3.0f;
        const float leftEnd = handle.x - gap;
        const float rightStart = handle.x + handle.width + gap;
        if (leftEnd > s.bounds.x) {
            canvas.drawLine({s.bounds.x, centerY}, {leftEnd, centerY}, lineColor, 1.0f);
        }
        if (rightStart < s.bounds.x + s.bounds.width) {
            canvas.drawLine({rightStart, centerY}, {s.bounds.x + s.bounds.width, centerY}, lineColor, 1.0f);
        }

        const float radius = std::min(handle.width, handle.height) * 0.45f;
        canvas.drawRoundedRectangle(handle, radius, handleColor);

        const float dotSize = std::clamp(handleH * 0.34f, 1.8f, 2.8f);
        const float dotStep = dotSize * 2.0f;
        const float dotY = centerY - dotSize * 0.5f;
        const float dotStartX = handle.x + handle.width * 0.5f - dotStep;
        for (int i = 0; i < 3; ++i) {
            canvas.drawRectangle({dotStartX + i * dotStep, dotY, dotSize, dotSize}, dotColor);
        }
    }
}

bool DockSplitter::handleEvent(Event& event)
{
    switch (event.type) {
    case Event::Type::MouseDown: {
        if (auto* s = splitterAtPoint({event.x, event.y})) {
            startDrag(s, {event.x, event.y});
            event.handled = true;
            return true;
        }
        break;
    }
    case Event::Type::MouseMove: {
        if (activeNode_) {
            updateDrag({event.x, event.y});
            event.handled = true;
            return true;
        }
        if (auto* s = splitterAtPoint({event.x, event.y})) {
            hoveredNode_ = s->node;
        } else {
            hoveredNode_ = nullptr;
        }
        break;
    }
    case Event::Type::MouseUp: {
        if (activeNode_) {
            endDrag();
            event.handled = true;
            return true;
        }
        break;
    }
    default:
        break;
    }
    return false;
}

} // namespace df
