#include "dock_framework.h"
#include "dock_layout.h"
#include "dock_theme.h"
#include "window_manager.h"
#include "core_types.h"

#include <algorithm>
#include <functional>
#include <limits>
#include <cmath>

namespace {

using Node = df::DockLayout::Node;

std::unique_ptr<Node>* FindNodeHandle(std::unique_ptr<Node>& node, Node* target)
{
    if (!node || !target) {
        return nullptr;
    }
    if (node.get() == target) {
        return &node;
    }
    if (auto* handle = FindNodeHandle(node->first, target)) {
        return handle;
    }
    if (auto* handle = FindNodeHandle(node->second, target)) {
        return handle;
    }
    for (auto& child : node->children) {
        if (auto* handle = FindNodeHandle(child, target)) {
            return handle;
        }
    }
    return nullptr;
}

std::unique_ptr<Node>* FindParentTabHandle(std::unique_ptr<Node>& node, Node* target)
{
    if (!node || !target) {
        return nullptr;
    }

    if (node->type == Node::Type::Tab) {
        for (auto& child : node->children) {
            if (child.get() == target) {
                return &node;
            }
        }
    }

    if (auto* handle = FindParentTabHandle(node->first, target)) {
        return handle;
    }
    if (auto* handle = FindParentTabHandle(node->second, target)) {
        return handle;
    }
    for (auto& child : node->children) {
        if (auto* handle = FindParentTabHandle(child, target)) {
            return handle;
        }
    }
    return nullptr;
}

void NormalizeNode(std::unique_ptr<Node>& node)
{
    if (!node) {
        return;
    }

    if (node->type == Node::Type::Split) {
        if (!node->first && !node->second) {
            node.reset();
            return;
        }
        if (!node->first && node->second) {
            node = std::move(node->second);
            NormalizeNode(node);
            return;
        }
        if (node->first && !node->second) {
            node = std::move(node->first);
            NormalizeNode(node);
            return;
        }
        return;
    }

    if (node->type == Node::Type::Tab) {
        node->children.erase(
            std::remove_if(node->children.begin(), node->children.end(),
                           [](const std::unique_ptr<Node>& child) { return child == nullptr; }),
            node->children.end());
        if (node->children.empty()) {
            node.reset();
            return;
        }
        if (node->children.size() == 1) {
            node = std::move(node->children.front());
            NormalizeNode(node);
            return;
        }
        node->activeTab = std::clamp(node->activeTab, 0, static_cast<int>(node->children.size()) - 1);
    }
}

bool RemoveWidgetNode(std::unique_ptr<Node>& node, df::DockWidget* target, std::unique_ptr<Node>& extracted)
{
    if (!node || !target) {
        return false;
    }

    if (node->type == Node::Type::Widget && node->widget == target) {
        extracted = std::move(node);
        return true;
    }

    auto recurseChild = [&](std::unique_ptr<Node>& child) -> bool {
        if (RemoveWidgetNode(child, target, extracted)) {
            NormalizeNode(child);
            return true;
        }
        return false;
    };

    if (recurseChild(node->first)) {
        NormalizeNode(node);
        return true;
    }
    if (recurseChild(node->second)) {
        NormalizeNode(node);
        return true;
    }

    for (auto& child : node->children) {
        if (recurseChild(child)) {
            NormalizeNode(node);
            return true;
        }
    }

    return false;
}

bool IsInTabDockCenterZone(const DFRect& bounds, const DFPoint& point)
{
    const float insetX = std::clamp(bounds.width * 0.28f, 18.0f, 140.0f);
    const float insetY = std::clamp(bounds.height * 0.28f, 14.0f, 120.0f);
    const DFRect center{
        bounds.x + insetX,
        bounds.y + insetY,
        std::max(0.0f, bounds.width - insetX * 2.0f),
        std::max(0.0f, bounds.height - insetY * 2.0f)
    };
    return center.width > 1.0f && center.height > 1.0f && center.contains(point);
}

bool ComputeTabDockCenterZone(const DFRect& bounds, DFRect& outCenter)
{
    const float insetX = std::clamp(bounds.width * 0.28f, 18.0f, 140.0f);
    const float insetY = std::clamp(bounds.height * 0.28f, 14.0f, 120.0f);
    outCenter = {
        bounds.x + insetX,
        bounds.y + insetY,
        std::max(0.0f, bounds.width - insetX * 2.0f),
        std::max(0.0f, bounds.height - insetY * 2.0f)
    };
    return outCenter.width > 1.0f && outCenter.height > 1.0f;
}

DFRect InsetRect(const DFRect& rect, float insetX, float insetY)
{
    const float clampedInsetX = std::max(0.0f, std::min(insetX, rect.width * 0.5f));
    const float clampedInsetY = std::max(0.0f, std::min(insetY, rect.height * 0.5f));
    return {
        rect.x + clampedInsetX,
        rect.y + clampedInsetY,
        std::max(0.0f, rect.width - clampedInsetX * 2.0f),
        std::max(0.0f, rect.height - clampedInsetY * 2.0f)
    };
}

DFRect OffsetAndShrinkRect(const DFRect& rect, float offsetX, float offsetY, float shrinkX, float shrinkY)
{
    DFRect out = rect;
    out.x += offsetX;
    out.y += offsetY;
    out.width = std::max(0.0f, out.width - shrinkX);
    out.height = std::max(0.0f, out.height - shrinkY);
    return out;
}

DFRect MakeBottomEdgeTabHintRect(const DFRect& stripRect)
{
    // Keep tab hints close to the header bottom edge (Qt-like drop affordance).
    const DFRect inner = InsetRect(stripRect, 3.0f, 1.0f);
    const float hintH = std::clamp(inner.height * 0.28f, 4.0f, 8.0f);
    const float y = inner.y + std::max(0.0f, inner.height - hintH - 1.0f);
    return {
        inner.x,
        y,
        inner.width,
        hintH
    };
}

Node* FindBestWidgetNodeAtPoint(Node* node, const DFPoint& point, df::DockWidget* movingWidget, float& bestArea)
{
    if (!node) {
        return nullptr;
    }
    Node* best = nullptr;
    if (node->type == Node::Type::Widget && node->widget && node->widget != movingWidget) {
        const DFRect& b = node->widget->bounds();
        // Accept any point inside the docked panel. Requiring a strict center zone
        // makes redocking feel unreliable for users and automation.
        if (b.width > 1.0f && b.height > 1.0f && b.contains(point)) {
            const float area = b.width * b.height;
            if (!best || area < bestArea) {
                best = node;
                bestArea = area;
            }
        }
    }

    if (node->first) {
        if (Node* child = FindBestWidgetNodeAtPoint(node->first.get(), point, movingWidget, bestArea)) {
            best = child;
        }
    }
    if (node->second) {
        if (Node* child = FindBestWidgetNodeAtPoint(node->second.get(), point, movingWidget, bestArea)) {
            best = child;
        }
    }
    for (auto& child : node->children) {
        if (Node* c = FindBestWidgetNodeAtPoint(child.get(), point, movingWidget, bestArea)) {
            best = c;
        }
    }
    return best;
}

} // namespace

namespace df {

// ----- DockWidget ---------------------------------------------------
DockWidget::DockWidget(const std::string& title) : title_(title)
{
    const auto& theme = CurrentTheme();
    clientAreaPadding_ = std::max(0.0f, theme.clientAreaPadding);
    clientAreaCornerRadius_ = std::max(0.0f, theme.clientAreaCornerRadius);
    clientAreaBorderThickness_ = std::max(0.0f, theme.clientAreaBorderThickness);
}
DockWidget::~DockWidget() = default;

void DockWidget::setTitle(const std::string& title) { title_ = title; }

void DockWidget::setContent(std::unique_ptr<Widget> widget) { content_ = std::move(widget); }

void DockWidget::setMinimumSize(float width, float height)
{
    minimumSize_.width = std::max(0.0f, width);
    minimumSize_.height = std::max(0.0f, height);
}

DFSize DockWidget::minimumSize() const
{
    const DFSize contentMin = content_ ? content_->minimumSize() : DFSize{};
    return {
        std::max(minimumSize_.width, contentMin.width),
        std::max(minimumSize_.height, contentMin.height)
    };
}

void DockWidget::setClientAreaPadding(float padding)
{
    clientAreaPadding_ = std::max(0.0f, padding);
}

void DockWidget::setClientAreaCornerRadius(float radius)
{
    clientAreaCornerRadius_ = std::max(0.0f, radius);
}

void DockWidget::setClientAreaBorderThickness(float thickness)
{
    clientAreaBorderThickness_ = std::max(0.0f, thickness);
}

DFRect DockWidget::clientAreaRect(const DFRect& contentBounds) const
{
    if (!childrenFloat_) {
        return contentBounds;
    }
    const float pad = std::max(0.0f, clientAreaPadding_);
    return {
        contentBounds.x + pad,
        contentBounds.y + pad,
        std::max(0.0f, contentBounds.width - pad * 2.0f),
        std::max(0.0f, contentBounds.height - pad * 2.0f)
    };
}

void DockWidget::paintClientArea(Canvas& canvas, const DFRect& contentBounds) const
{
    const DFRect client = clientAreaRect(contentBounds);
    if (client.width <= 0.0f || client.height <= 0.0f) {
        return;
    }

    const auto& theme = CurrentTheme();
    const float maxRadius = std::min(client.width, client.height) * 0.5f;
    const float cornerRadius = std::clamp(clientAreaCornerRadius_, 0.0f, maxRadius);
    canvas.drawRoundedRectangle(client, cornerRadius, theme.clientAreaFill);
    canvas.drawRoundedRectangleOutline(
        client,
        cornerRadius,
        theme.clientAreaBorder,
        std::max(1.0f, clientAreaBorderThickness_));
}

void DockWidget::setBounds(const DFRect& r)
{
    bounds_ = r;
    if (!floating_) {
        hostType_ = HostType::DockedLayout;
        hostWindow_ = nullptr;
    }
    if (content_) content_->setBounds(r);
}

DFRect DockWidget::globalBounds() const
{
    if (hostWindow_) {
        return hostWindow_->globalBounds();
    }
    const DFPoint origin = WindowManager::instance().clientOriginScreen();
    return {
        bounds_.x + origin.x,
        bounds_.y + origin.y,
        bounds_.width,
        bounds_.height
    };
}

void DockWidget::paint(Canvas& canvas)
{
    const auto& theme = CurrentTheme();
    canvas.drawRectangle(bounds_, theme.dockBackground);
    paintClientArea(canvas, bounds_);
    if (content_) {
        const DFRect client = clientAreaRect(bounds_);
        content_->setBounds(client);
        content_->paint(canvas);
    }
}

void DockWidget::handleEvent(Event& event)
{
    if (!content_) {
        return;
    }

    Event local = event;
    const DFRect client = clientAreaRect(bounds_);
    local.x -= client.x;
    local.y -= client.y;
    content_->handleEvent(local);
    if (local.handled) {
        event.handled = true;
    }
}

// ----- DockArea -----------------------------------------------------
DockArea::DockArea(Position pos) : position_(pos) {}

void DockArea::addDockWidget(DockWidget* widget)
{
    if (widget) {
        widgets_.push_back(widget);
    }
}

void DockArea::removeDockWidget(DockWidget* widget)
{
    widgets_.erase(std::remove(widgets_.begin(), widgets_.end(), widget), widgets_.end());
}

// ----- DockContainer ------------------------------------------------
DockContainer::DockContainer() = default;

DockArea* DockContainer::addDockArea(DockArea::Position position)
{
    auto it = areas_.find(position);
    if (it != areas_.end()) {
        return it->second.get();
    }
    auto area = std::make_unique<DockArea>(position);
    DockArea* raw = area.get();
    areas_[position] = std::move(area);
    return raw;
}

DockArea* DockContainer::dockArea(DockArea::Position position) const
{
    auto it = areas_.find(position);
    return it != areas_.end() ? it->second.get() : nullptr;
}

void DockContainer::setCentralWidget(std::unique_ptr<Widget> widget)
{
    centralWidget_ = std::move(widget);
}

void DockContainer::updateLayout(const DFRect& bounds)
{
    if (centralWidget_) centralWidget_->setBounds(bounds);
}

// ----- DockManager --------------------------------------------------
DockManager& DockManager::instance()
{
    static DockManager inst;
    return inst;
}

void DockManager::registerWidget(DockWidget* widget)
{
    if (!widget) return;
    if (std::find(widgets_.begin(), widgets_.end(), widget) == widgets_.end()) {
        widgets_.push_back(widget);
    }
}

void DockManager::unregisterWidget(DockWidget* widget)
{
    widgets_.erase(std::remove(widgets_.begin(), widgets_.end(), widget), widgets_.end());
}

void DockManager::startDrag(DockWidget* widget, const DFPoint& mousePos)
{
    drag_.widget = widget;
    drag_.startPos = mousePos;
    drag_.lastPos = mousePos;
    drag_.currentPos = mousePos;
    drag_.startBounds = widget ? widget->bounds() : DFRect{};
    drag_.active = widget != nullptr;
}

void DockManager::updateDrag(const DFPoint& mousePos)
{
    if (!drag_.active || !drag_.widget) return;
    drag_.currentPos = mousePos;

    if (!drag_.widget->isFloating()) {
        // Promote docked title-bar drags to undock after a movement threshold.
        // Requiring the cursor to leave the full panel makes large panels feel
        // "stuck" and is not Qt-like.
        const float dxFromStart = mousePos.x - drag_.startPos.x;
        const float dyFromStart = mousePos.y - drag_.startPos.y;
        const float distanceSq = dxFromStart * dxFromStart + dyFromStart * dyFromStart;
        float undockDistanceSq = 4900.0f; // ~70px default intentional undock drag
        const DFRect widgetBounds = drag_.startBounds;
        const float widgetArea = widgetBounds.width * widgetBounds.height;
        const float containerArea = mainContainerBounds_.width * mainContainerBounds_.height;
        if (containerArea > 1.0f) {
            const float coverage = widgetArea / containerArea;
            // If a single panel covers most of the workspace, make undocking easier.
            if (coverage > 0.85f) {
                undockDistanceSq = 1024.0f; // ~32px
            }
        }
        const bool movedEnough = distanceSq > undockDistanceSq;
        if (drag_.widget->isSingleDocked() && movedEnough) {
            DockWidget* widget = drag_.widget;
            endDrag();
            startUndockDrag(widget, mousePos);
            return;
        }
        // Keep docked widgets layout-driven until they explicitly undock.
        drag_.lastPos = mousePos;
        return;
    }

    const float dx = mousePos.x - drag_.lastPos.x;
    const float dy = mousePos.y - drag_.lastPos.y;

    DFRect bounds = drag_.widget->bounds();
    bounds.x += dx;
    bounds.y += dy;

    if (hasDragBounds_) {
        if (bounds.width > dragBounds_.width) bounds.width = dragBounds_.width;
        if (bounds.height > dragBounds_.height) bounds.height = dragBounds_.height;
        const float minX = dragBounds_.x;
        const float minY = dragBounds_.y;
        float maxX = dragBounds_.x + dragBounds_.width - bounds.width;
        float maxY = dragBounds_.y + dragBounds_.height - bounds.height;
        if (maxX < minX) maxX = minX;
        if (maxY < minY) maxY = minY;
        bounds.x = std::clamp(bounds.x, minX, maxX);
        bounds.y = std::clamp(bounds.y, minY, maxY);
    }

    drag_.widget->setBounds(bounds);
    drag_.lastPos = mousePos;
}

void DockManager::endDrag()
{
    drag_ = DragData{};
}

void DockManager::closeDockedWidget(DockWidget* widget)
{
    if (!widget || widget->isFloating() || !mainLayout_) {
        return;
    }

    std::unique_ptr<Node> root = mainLayout_->takeRoot();
    if (!root) {
        return;
    }

    std::unique_ptr<Node> extracted;
    if (!RemoveWidgetNode(root, widget, extracted)) {
        mainLayout_->setRoot(std::move(root));
        return;
    }

    NormalizeNode(root);
    mainLayout_->setRoot(std::move(root));
    widget->setBounds({0.0f, 0.0f, 0.0f, 0.0f});
    widget->setTabified(false);
    widget->hostType_ = DockWidget::HostType::None;
    widget->hostWindow_ = nullptr;
}

void DockManager::closeWidget(DockWidget* widget)
{
    if (!widget) {
        return;
    }
    if (widget->isFloating()) {
        if (auto* frame = WindowManager::instance().findWindowByContent(widget)) {
            WindowManager::instance().destroyWindow(frame);
        }
        widget->setBounds({0.0f, 0.0f, 0.0f, 0.0f});
        widget->setTabified(false);
        widget->hostType_ = DockWidget::HostType::None;
        widget->hostWindow_ = nullptr;
        return;
    }
    closeDockedWidget(widget);
}

void DockManager::startUndockDrag(DockWidget* widget, const DFPoint& mousePos)
{
    if (!widget || widget->isFloating() || !mainLayout_) {
        return;
    }

    std::unique_ptr<Node> root = mainLayout_->takeRoot();
    if (!root) {
        return;
    }

    std::unique_ptr<Node> extracted;
    if (!RemoveWidgetNode(root, widget, extracted)) {
        mainLayout_->setRoot(std::move(root));
        return;
    }

    NormalizeNode(root);
    mainLayout_->setRoot(std::move(root));

    if (!extracted || extracted->type != Node::Type::Widget || extracted->widget != widget) {
        return;
    }

    DFRect bounds = widget->bounds();
    if (bounds.width <= 1.0f || bounds.height <= 1.0f) {
        bounds = {mainContainerBounds_.x + 100.0f, mainContainerBounds_.y + 80.0f, 320.0f, 220.0f};
    }
    bounds.width = std::max(bounds.width, 300.0f);
    bounds.height = std::max(bounds.height, 200.0f);

    auto* frame = WindowManager::instance().createFloatingWindow(widget, bounds);
    if (frame) {
        startFloatingDrag(frame, mousePos);
    }
}

bool DockManager::handleEvent(Event& event)
{
    if (draggedFloatingWindow_) {
        switch (event.type) {
        case Event::Type::MouseMove:
            updateFloatingDrag({event.x, event.y});
            event.handled = true;
            return true;
        case Event::Type::MouseUp:
            endFloatingDrag({event.x, event.y});
            event.handled = true;
            return true;
        default:
            return false;
        }
    }

    if (!drag_.active) return false;

    switch (event.type) {
    case Event::Type::MouseMove:
        updateDrag({event.x, event.y});
        event.handled = true;
        return true;
    case Event::Type::MouseUp:
        endDrag();
        event.handled = true;
        return true;
    default:
        return false;
    }
}

void DockManager::setMainLayout(DockLayout* layout, const DFRect& containerBounds)
{
    mainLayout_ = layout;
    mainContainerBounds_ = containerBounds;
}

void DockManager::startFloatingDrag(WindowFrame* window, const DFPoint& mousePos)
{
    if (!window || draggedFloatingWindow_) {
        return;
    }

    draggedFloatingWindow_ = window;
    const DFRect bounds = window->bounds();
    dragGrabOffset_ = {mousePos.x - bounds.x, mousePos.y - bounds.y};
    suppressDockOnNextDrop_ = false;
    overlay_.setVisible(true);
    // Move the real floating widget while dragging; no separate ghost preview.
    overlay_.setDraggedWidget(nullptr);
    overlay_.setPreview({});
}

void DockManager::updateFloatingDrag(const DFPoint& mousePos)
{
    if (!draggedFloatingWindow_) {
        return;
    }
    if (!WindowManager::instance().hasWindow(draggedFloatingWindow_)) {
        cancelFloatingDrag();
        return;
    }

    // Keep the actual floating window synced with the cursor during drag.
    DFRect moved = draggedFloatingWindow_->bounds();
    moved.x = mousePos.x - dragGrabOffset_.x;
    moved.y = mousePos.y - dragGrabOffset_.y;
    const DFRect work = WindowManager::instance().workArea();
    if (work.width > 0.0f && work.height > 0.0f) {
        if (moved.width > work.width) moved.width = work.width;
        if (moved.height > work.height) moved.height = work.height;
        const float minX = work.x;
        const float minY = work.y;
        float maxX = work.x + work.width - moved.width;
        float maxY = work.y + work.height - moved.height;
        if (maxX < minX) maxX = minX;
        if (maxY < minY) maxY = minY;
        moved.x = std::clamp(moved.x, minX, maxX);
        moved.y = std::clamp(moved.y, minY, maxY);
    }
    draggedFloatingWindow_->setBounds(moved);
    overlay_.setPreview({});

    overlay_.clearZones();
    dropCandidates_.clear();
    highlightedCandidateIndex_ = -1;
    if (mainContainerBounds_.width <= 0.0f || mainContainerBounds_.height <= 0.0f) {
        return;
    }

    // Root edge docking excludes the header/tool area.
    const float headerInset = std::clamp(
        rootDockHeaderInsetPx_,
        0.0f,
        std::max(0.0f, mainContainerBounds_.height));
    const DFRect rootContainer{
        mainContainerBounds_.x,
        mainContainerBounds_.y + headerInset,
        mainContainerBounds_.width,
        std::max(0.0f, mainContainerBounds_.height - headerInset)
    };
    if (rootContainer.width <= 1.0f || rootContainer.height <= 1.0f) {
        return;
    }

    // Keep edge hints active even when cursor is slightly outside the client rect.
    // This makes "drag to edge and release" reliable at window boundaries.
    const float edgeMargin = edgeDockActivateDistancePx_ * 1.5f;
    const DFRect expandedContainer{
        rootContainer.x - edgeMargin,
        rootContainer.y - edgeMargin,
        rootContainer.width + edgeMargin * 2.0f,
        rootContainer.height + edgeMargin * 2.0f
    };
    if (!expandedContainer.contains(mousePos)) {
        overlay_.highlightZone(DragOverlay::DropZone::None);
        highlightedCandidateIndex_ = -1;
        return;
    }

    auto addCandidate = [this](DragOverlay::DropZone zone, Node* target, const DFRect& bounds, int depth) {
        if (bounds.width <= 1.0f || bounds.height <= 1.0f) {
            return;
        }
        const size_t overlayIndex = overlay_.addZone(bounds, zone);
        DropCandidate entry;
        entry.zone = zone;
        entry.target = target;
        entry.bounds = bounds;
        entry.overlayIndex = overlayIndex;
        entry.depth = depth;
        dropCandidates_.push_back(entry);
    };

    const float leftDist = std::abs(mousePos.x - rootContainer.x);
    const float rightDist = std::abs((rootContainer.x + rootContainer.width) - mousePos.x);
    const float topDist = std::abs(mousePos.y - rootContainer.y);
    const float bottomDist = std::abs((rootContainer.y + rootContainer.height) - mousePos.y);
    const float minDist = std::min(std::min(leftDist, rightDist), std::min(topDist, bottomDist));

    DragOverlay::DropZone nearestEdge = DragOverlay::DropZone::Left;
    float nearestDist = leftDist;
    if (rightDist < nearestDist) {
        nearestDist = rightDist;
        nearestEdge = DragOverlay::DropZone::Right;
    }
    if (topDist < nearestDist) {
        nearestDist = topDist;
        nearestEdge = DragOverlay::DropZone::Top;
    }
    if (bottomDist < nearestDist) {
        nearestDist = bottomDist;
        nearestEdge = DragOverlay::DropZone::Bottom;
    }
    (void)nearestDist;

    // Edge hints: 3x thicker than before for clearer docking affordance.
    const float sideW = 2.0f * 3.0f;
    const float sideH = 2.0f * 3.0f;
    DFRect edgeBounds{};
    switch (nearestEdge) {
    case DragOverlay::DropZone::Left:
        edgeBounds = {rootContainer.x, rootContainer.y, sideW, rootContainer.height};
        break;
    case DragOverlay::DropZone::Right:
        edgeBounds = {rootContainer.x + rootContainer.width - sideW, rootContainer.y, sideW, rootContainer.height};
        break;
    case DragOverlay::DropZone::Top:
        edgeBounds = {rootContainer.x, rootContainer.y, rootContainer.width, sideH};
        break;
    case DragOverlay::DropZone::Bottom:
        edgeBounds = {rootContainer.x, rootContainer.y + rootContainer.height - sideH, rootContainer.width, sideH};
        break;
    case DragOverlay::DropZone::None:
    case DragOverlay::DropZone::Center:
    case DragOverlay::DropZone::Tab:
    default:
        break;
    }
    addCandidate(nearestEdge, nullptr, edgeBounds, 0);

    DockWidget* movingWidget = draggedFloatingWindow_->content();
    // Tab docking hints: only appear when cursor is inside a real tab/header strip.
    std::function<void(Node*, int)> collectTabTargets = [&](Node* node, int depth) {
        if (!node) {
            return;
        }

        if (node->type == Node::Type::Widget && node->widget && node->widget != movingWidget) {
            const DFRect panelBounds = node->widget->bounds();
            const float headerH = std::clamp(24.0f, 0.0f, std::max(0.0f, panelBounds.height));
            const DFRect headerRect{panelBounds.x, panelBounds.y, panelBounds.width, headerH};
            // Keep tab hints strictly inside the panel header strip and aligned
            // to the bottom edge for a cleaner target.
            const DFRect tabHintRect = MakeBottomEdgeTabHintRect(headerRect);
            if (tabHintRect.width > 1.0f && tabHintRect.height > 1.0f && tabHintRect.contains(mousePos)) {
                addCandidate(DragOverlay::DropZone::Tab, node, tabHintRect, depth);
            }
            return;
        }

        if (node->type == Node::Type::Tab && !node->children.empty()) {
            const float barH = std::clamp(node->tabBarHeight, 0.0f, std::max(0.0f, node->bounds.height));
            const DFRect barRect{node->bounds.x, node->bounds.y, node->bounds.width, barH};
            const DFRect tabHintRect = MakeBottomEdgeTabHintRect(barRect);
            if (tabHintRect.width > 1.0f && tabHintRect.height > 1.0f && tabHintRect.contains(mousePos)) {
                addCandidate(DragOverlay::DropZone::Tab, node, tabHintRect, depth);
            }
        }

        collectTabTargets(node->first.get(), depth + 1);
        collectTabTargets(node->second.get(), depth + 1);
        for (auto& child : node->children) {
            collectTabTargets(child.get(), depth + 1);
        }
    };

    // Inner split docking hints: use fixed-depth edge zones for predictable
    // hit targets across different panel sizes.
    std::function<void(Node*, int, bool)> collectSplitTargets = [&](Node* node, int depth, bool insideTabContainer) {
        if (!node) {
            return;
        }

        const bool childInsideTabContainer = insideTabContainer || (node->type == Node::Type::Tab);
        collectSplitTargets(node->first.get(), depth + 1, childInsideTabContainer);
        collectSplitTargets(node->second.get(), depth + 1, childInsideTabContainer);
        for (auto& child : node->children) {
            collectSplitTargets(child.get(), depth + 1, childInsideTabContainer);
        }

        const bool isDockableWidget =
            (!insideTabContainer && node->type == Node::Type::Widget && node->widget && node->widget != movingWidget);
        const bool isDockableTab = (node->type == Node::Type::Tab && !node->children.empty());
        if (!isDockableWidget && !isDockableTab) {
            return;
        }

        const DFRect b = node->bounds;
        if (b.width < 40.0f || b.height < 40.0f || !b.contains(mousePos)) {
            return;
        }

        const float zoneW = std::min(innerSplitSnapZonePx_, b.width * 0.4f);
        const float zoneH = std::min(innerSplitSnapZonePx_, b.height * 0.4f);
        const float topOffset =
            (node->type == Node::Type::Tab)
                ? std::clamp(node->tabBarHeight, 0.0f, std::max(0.0f, b.height - 1.0f))
                : 0.0f;
        const float topZoneH = std::min(zoneH, std::max(0.0f, b.height - topOffset));
        const DFRect leftZone{b.x, b.y, zoneW, b.height};
        const DFRect rightZone{b.x + b.width - zoneW, b.y, zoneW, b.height};
        const DFRect topZone{b.x, b.y + topOffset, b.width, topZoneH};
        const DFRect bottomZone{b.x, b.y + b.height - zoneH, b.width, zoneH};

        if (leftZone.contains(mousePos)) {
            addCandidate(DragOverlay::DropZone::Left, node, leftZone, depth);
        } else if (rightZone.contains(mousePos)) {
            addCandidate(DragOverlay::DropZone::Right, node, rightZone, depth);
        } else if (topZone.height > 1.0f && topZone.contains(mousePos)) {
            addCandidate(DragOverlay::DropZone::Top, node, topZone, depth);
        } else if (bottomZone.contains(mousePos)) {
            addCandidate(DragOverlay::DropZone::Bottom, node, bottomZone, depth);
        }
    };

    if (mainLayout_) {
        collectTabTargets(mainLayout_->root(), 1);
        collectSplitTargets(mainLayout_->root(), 1, false);
    }

    auto isEdgeZone = [](DragOverlay::DropZone zone) {
        return zone == DragOverlay::DropZone::Left ||
            zone == DragOverlay::DropZone::Right ||
            zone == DragOverlay::DropZone::Top ||
            zone == DragOverlay::DropZone::Bottom;
    };

    const DropCandidate* hovered = nullptr;
    float bestArea = std::numeric_limits<float>::max();
    int bestDepth = -1;
    int bestPriority = -1;
    for (const auto& candidate : dropCandidates_) {
        const bool edgeNearAndMatching = isEdgeZone(candidate.zone) &&
            candidate.zone == nearestEdge &&
            minDist <= edgeDockActivateDistancePx_;
        if (!candidate.bounds.contains(mousePos) && !edgeNearAndMatching) {
            continue;
        }
        const float area = candidate.bounds.width * candidate.bounds.height;
        int priority = 0;
        if (candidate.zone == DragOverlay::DropZone::Tab || candidate.zone == DragOverlay::DropZone::Center) {
            priority = 5;
        } else if (isEdgeZone(candidate.zone)) {
            // Inner split candidates outrank root edge candidates.
            priority = (candidate.depth > 0) ? 4 : 3;
        }
        if (!hovered ||
            priority > bestPriority ||
            (priority == bestPriority && candidate.depth > bestDepth) ||
            (priority == bestPriority && candidate.depth == bestDepth && area < bestArea)) {
            hovered = &candidate;
            bestArea = area;
            bestDepth = candidate.depth;
            bestPriority = priority;
        }
    }
    if (hovered) {
        overlay_.highlightZoneIndex(hovered->overlayIndex);
        highlightedCandidateIndex_ = static_cast<int>(hovered->overlayIndex);
    } else {
        overlay_.highlightZone(DragOverlay::DropZone::None);
        highlightedCandidateIndex_ = -1;
    }
}

void DockManager::endFloatingDrag(const DFPoint& mousePos)
{
    if (!draggedFloatingWindow_) {
        return;
    }
    if (!WindowManager::instance().hasWindow(draggedFloatingWindow_)) {
        cancelFloatingDrag();
        return;
    }
    // Re-evaluate drop hints at mouse-up to guarantee release uses the
    // current highlighted target.
    updateFloatingDrag(mousePos);

    const float headerInset = std::clamp(
        rootDockHeaderInsetPx_,
        0.0f,
        std::max(0.0f, mainContainerBounds_.height));
    DFRect rootContainer{
        mainContainerBounds_.x,
        mainContainerBounds_.y + headerInset,
        mainContainerBounds_.width,
        std::max(0.0f, mainContainerBounds_.height - headerInset)
    };
    if (rootContainer.width <= 1.0f || rootContainer.height <= 1.0f) {
        rootContainer = mainContainerBounds_;
    }

    const float leftDist = std::abs(mousePos.x - rootContainer.x);
    const float rightDist = std::abs((rootContainer.x + rootContainer.width) - mousePos.x);
    const float topDist = std::abs(mousePos.y - rootContainer.y);
    const float bottomDist = std::abs((rootContainer.y + rootContainer.height) - mousePos.y);
    const float minDist = std::min(std::min(leftDist, rightDist), std::min(topDist, bottomDist));

    DragOverlay::DropZone nearestEdge = DragOverlay::DropZone::Left;
    float nearestDist = leftDist;
    if (rightDist < nearestDist) {
        nearestDist = rightDist;
        nearestEdge = DragOverlay::DropZone::Right;
    }
    if (topDist < nearestDist) {
        nearestDist = topDist;
        nearestEdge = DragOverlay::DropZone::Top;
    }
    if (bottomDist < nearestDist) {
        nearestEdge = DragOverlay::DropZone::Bottom;
    }
    (void)nearestDist;

    auto isEdgeZone = [](DragOverlay::DropZone zone) {
        return zone == DragOverlay::DropZone::Left ||
            zone == DragOverlay::DropZone::Right ||
            zone == DragOverlay::DropZone::Top ||
            zone == DragOverlay::DropZone::Bottom;
    };

    const DropCandidate* candidate = nullptr;
    float bestArea = std::numeric_limits<float>::max();
    int bestDepth = -1;
    int bestPriority = -1;
    for (const auto& current : dropCandidates_) {
        const bool edgeNearAndMatching = isEdgeZone(current.zone) &&
            current.zone == nearestEdge &&
            minDist <= edgeDockActivateDistancePx_;
        if (!current.bounds.contains(mousePos) && !edgeNearAndMatching) {
            continue;
        }
        const float area = current.bounds.width * current.bounds.height;
        int priority = 0;
        if (current.zone == DragOverlay::DropZone::Tab || current.zone == DragOverlay::DropZone::Center) {
            priority = 5;
        } else if (isEdgeZone(current.zone)) {
            priority = (current.depth > 0) ? 4 : 3;
        }
        if (!candidate ||
            priority > bestPriority ||
            (priority == bestPriority && current.depth > bestDepth) ||
            (priority == bestPriority && current.depth == bestDepth && area < bestArea)) {
            candidate = &current;
            bestArea = area;
            bestDepth = current.depth;
            bestPriority = priority;
        }
    }

    // Strict tab docking: only allow tabify when mouse-up is inside
    // the tab highlight rectangle.
    if (candidate && candidate->zone == DragOverlay::DropZone::Tab &&
        !candidate->bounds.contains(mousePos)) {
        candidate = nullptr;
    }

    WindowFrame* sourceWindow = draggedFloatingWindow_;
    DockWidget* widget = sourceWindow->content();
    if (!widget) {
        cancelFloatingDrag();
        return;
    }

    if (!mainLayout_) {
        DFRect moved = sourceWindow->bounds();
        moved.x = mousePos.x - dragGrabOffset_.x;
        moved.y = mousePos.y - dragGrabOffset_.y;
        const DFRect work = WindowManager::instance().workArea();
        if (work.width > 0.0f && work.height > 0.0f) {
            if (moved.width > work.width) moved.width = work.width;
            if (moved.height > work.height) moved.height = work.height;
            const float minX = work.x;
            const float minY = work.y;
            float maxX = work.x + work.width - moved.width;
            float maxY = work.y + work.height - moved.height;
            if (maxX < minX) maxX = minX;
            if (maxY < minY) maxY = minY;
            moved.x = std::clamp(moved.x, minX, maxX);
            moved.y = std::clamp(moved.y, minY, maxY);
        }
        sourceWindow->setBounds(moved);
        cancelFloatingDrag();
        return;
    }

    if (suppressDockOnNextDrop_) {
        // Only suppress accidental drops when no explicit drop hint is active.
        const bool allowDockOnHint = (candidate != nullptr);
        if (!allowDockOnHint) {
            DFRect moved = sourceWindow->bounds();
            moved.x = mousePos.x - dragGrabOffset_.x;
            moved.y = mousePos.y - dragGrabOffset_.y;
            const DFRect work = WindowManager::instance().workArea();
            if (work.width > 0.0f && work.height > 0.0f) {
                if (moved.width > work.width) moved.width = work.width;
                if (moved.height > work.height) moved.height = work.height;
                const float minX = work.x;
                const float minY = work.y;
                float maxX = work.x + work.width - moved.width;
                float maxY = work.y + work.height - moved.height;
                if (maxX < minX) maxX = minX;
                if (maxY < minY) maxY = minY;
                moved.x = std::clamp(moved.x, minX, maxX);
                moved.y = std::clamp(moved.y, minY, maxY);
            }
            sourceWindow->setBounds(moved);
            cancelFloatingDrag();
            return;
        }
    }

    if (!candidate) {
        DFRect moved = sourceWindow->bounds();
        moved.x = mousePos.x - dragGrabOffset_.x;
        moved.y = mousePos.y - dragGrabOffset_.y;
        const DFRect work = WindowManager::instance().workArea();
        if (work.width > 0.0f && work.height > 0.0f) {
            if (moved.width > work.width) moved.width = work.width;
            if (moved.height > work.height) moved.height = work.height;
            const float minX = work.x;
            const float minY = work.y;
            float maxX = work.x + work.width - moved.width;
            float maxY = work.y + work.height - moved.height;
            if (maxX < minX) maxX = minX;
            if (maxY < minY) maxY = minY;
            moved.x = std::clamp(moved.x, minX, maxX);
            moved.y = std::clamp(moved.y, minY, maxY);
        }
        sourceWindow->setBounds(moved);
        cancelFloatingDrag();
        return;
    }

    WindowManager::instance().destroyWindow(sourceWindow);

    auto newLeaf = std::make_unique<Node>();
    newLeaf->type = Node::Type::Widget;
    newLeaf->widget = widget;

    std::unique_ptr<Node> root = mainLayout_->takeRoot();
    if (!root) {
        mainLayout_->setRoot(std::move(newLeaf));
        cancelFloatingDrag();
        return;
    }

    Node* targetNode = static_cast<Node*>(candidate->target);
    if (candidate->zone == DragOverlay::DropZone::Center || candidate->zone == DragOverlay::DropZone::Tab) {
        if (targetNode) {
            // If target widget already belongs to a tab group, append into that tab
            // instead of creating nested tab-in-tab structures.
            if (auto* parentTabHandle = FindParentTabHandle(root, targetNode);
                parentTabHandle && *parentTabHandle && (*parentTabHandle)->type == Node::Type::Tab) {
                (*parentTabHandle)->children.push_back(std::move(newLeaf));
                (*parentTabHandle)->activeTab = static_cast<int>((*parentTabHandle)->children.size()) - 1;
                NormalizeNode(root);
                mainLayout_->setRoot(std::move(root));
                cancelFloatingDrag();
                return;
            }

            auto* handle = FindNodeHandle(root, targetNode);
            if (handle && *handle) {
                if ((*handle)->type == Node::Type::Tab) {
                    (*handle)->children.push_back(std::move(newLeaf));
                    (*handle)->activeTab = static_cast<int>((*handle)->children.size()) - 1;
                } else if ((*handle)->type == Node::Type::Widget) {
                    auto existingLeaf = std::make_unique<Node>();
                    existingLeaf->type = Node::Type::Widget;
                    existingLeaf->widget = (*handle)->widget;

                    auto tabNode = std::make_unique<Node>();
                    tabNode->type = Node::Type::Tab;
                    tabNode->tabBarHeight = 24.0f;
                    tabNode->children.push_back(std::move(existingLeaf));
                    tabNode->children.push_back(std::move(newLeaf));
                    tabNode->activeTab = 1;
                    *handle = std::move(tabNode);
                } else {
                    auto tabNode = std::make_unique<Node>();
                    tabNode->type = Node::Type::Tab;
                    tabNode->tabBarHeight = 24.0f;
                    tabNode->children.push_back(std::move(*handle));
                    tabNode->children.push_back(std::move(newLeaf));
                    tabNode->activeTab = 1;
                    *handle = std::move(tabNode);
                }
            } else {
                auto tabNode = std::make_unique<Node>();
                tabNode->type = Node::Type::Tab;
                tabNode->tabBarHeight = 24.0f;
                tabNode->children.push_back(std::move(root));
                tabNode->children.push_back(std::move(newLeaf));
                tabNode->activeTab = 1;
                root = std::move(tabNode);
            }
        } else {
            auto tabNode = std::make_unique<Node>();
            tabNode->type = Node::Type::Tab;
            tabNode->tabBarHeight = 24.0f;
            tabNode->children.push_back(std::move(root));
            tabNode->children.push_back(std::move(newLeaf));
            tabNode->activeTab = 1;
            root = std::move(tabNode);
        }
    } else {
        std::unique_ptr<Node>* handle = nullptr;
        if (targetNode) {
            handle = FindNodeHandle(root, targetNode);
        }
        if (!handle || !*handle) {
            handle = &root;
        }

        std::unique_ptr<Node> existing = std::move(*handle);
        auto split = std::make_unique<Node>();
        split->type = Node::Type::Split;
        split->vertical = (candidate->zone == DragOverlay::DropZone::Left || candidate->zone == DragOverlay::DropZone::Right);
        split->ratio = 0.5f;
        split->minFirstSize = 120.0f;
        split->minSecondSize = 120.0f;

        if (candidate->zone == DragOverlay::DropZone::Left || candidate->zone == DragOverlay::DropZone::Top) {
            split->first = std::move(newLeaf);
            split->second = std::move(existing);
            split->ratio = 0.25f;
        } else {
            split->first = std::move(existing);
            split->second = std::move(newLeaf);
            split->ratio = 0.75f;
        }
        *handle = std::move(split);
    }

    NormalizeNode(root);
    mainLayout_->setRoot(std::move(root));
    cancelFloatingDrag();
}

void DockManager::cancelFloatingDrag()
{
    overlay_.clearZones();
    overlay_.setVisible(false);
    overlay_.setPreview({});
    highlightedCandidateIndex_ = -1;
    suppressDockOnNextDrop_ = false;
    draggedFloatingWindow_ = nullptr;
}

void DockManager::suppressDockForActiveFloatingDrag()
{
    if (draggedFloatingWindow_) {
        suppressDockOnNextDrop_ = true;
    }
}

std::string DockManager::saveState() const
{
    return "{}";
}

bool DockManager::restoreState(const std::string& state)
{
    (void)state;
    return true;
}

} // namespace df
