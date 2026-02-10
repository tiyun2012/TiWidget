#pragma once

#include <algorithm>
#include <cctype>
#include <string>

#include "core_types.h"

namespace df {

struct DockTheme {
    DFColor dockTitleBar{0.30f, 0.30f, 0.35f, 1.0f};
    DFColor dockBackground{0.20f, 0.20f, 0.25f, 1.0f};
    DFColor dockBorder{0.40f, 0.40f, 0.45f, 1.0f};

    DFColor floatingFrame{0.25f, 0.25f, 0.25f, 1.0f};
    DFColor floatingTitleBar{0.40f, 0.40f, 0.40f, 1.0f};
    DFColor floatingCloseButton{1.00f, 0.30f, 0.30f, 1.0f};

    DFColor tabStrip{0.16f, 0.16f, 0.19f, 1.0f};
    DFColor tabActive{0.28f, 0.34f, 0.44f, 1.0f};
    DFColor tabInactive{0.22f, 0.22f, 0.26f, 1.0f};

    DFColor splitter{0.50f, 0.50f, 0.50f, 1.0f};
    DFColor splitterHover{0.75f, 0.75f, 0.82f, 1.0f};
    DFColor splitterDrag{0.30f, 0.60f, 1.00f, 1.0f};

    DFColor overlayPanel{0.06f, 0.06f, 0.09f, 0.85f};
    DFColor overlayAccent{0.30f, 0.78f, 1.00f, 0.95f};
    DFColor overlayAccentSoft{0.15f, 0.55f, 0.85f, 0.20f};
};

inline DockTheme MakeDarkTheme()
{
    return DockTheme{};
}

inline DockTheme MakeLightTheme()
{
    DockTheme theme{};
    theme.dockTitleBar = {0.82f, 0.84f, 0.88f, 1.0f};
    theme.dockBackground = {0.93f, 0.94f, 0.96f, 1.0f};
    theme.dockBorder = {0.66f, 0.68f, 0.72f, 1.0f};
    theme.floatingFrame = {0.89f, 0.90f, 0.93f, 1.0f};
    theme.floatingTitleBar = {0.80f, 0.82f, 0.86f, 1.0f};
    theme.floatingCloseButton = {0.92f, 0.35f, 0.35f, 1.0f};
    theme.tabStrip = {0.84f, 0.86f, 0.89f, 1.0f};
    theme.tabActive = {0.62f, 0.72f, 0.92f, 1.0f};
    theme.tabInactive = {0.75f, 0.77f, 0.82f, 1.0f};
    theme.splitter = {0.58f, 0.60f, 0.64f, 1.0f};
    theme.splitterHover = {0.36f, 0.60f, 0.94f, 1.0f};
    theme.overlayPanel = {0.94f, 0.95f, 0.97f, 0.90f};
    theme.overlayAccent = {0.20f, 0.52f, 0.92f, 0.95f};
    theme.overlayAccentSoft = {0.16f, 0.47f, 0.85f, 0.26f};
    return theme;
}

inline DockTheme MakeSlateTheme()
{
    DockTheme theme{};
    theme.dockTitleBar = {0.20f, 0.25f, 0.28f, 1.0f};
    theme.dockBackground = {0.13f, 0.16f, 0.19f, 1.0f};
    theme.dockBorder = {0.34f, 0.41f, 0.46f, 1.0f};
    theme.floatingFrame = {0.16f, 0.20f, 0.23f, 1.0f};
    theme.floatingTitleBar = {0.22f, 0.28f, 0.33f, 1.0f};
    theme.floatingCloseButton = {0.94f, 0.38f, 0.31f, 1.0f};
    theme.tabStrip = {0.16f, 0.20f, 0.23f, 1.0f};
    theme.tabActive = {0.24f, 0.41f, 0.54f, 1.0f};
    theme.tabInactive = {0.18f, 0.25f, 0.29f, 1.0f};
    theme.splitter = {0.31f, 0.39f, 0.43f, 1.0f};
    theme.splitterHover = {0.47f, 0.67f, 0.78f, 1.0f};
    theme.splitterDrag = {0.34f, 0.78f, 0.97f, 1.0f};
    theme.overlayPanel = {0.08f, 0.11f, 0.14f, 0.88f};
    theme.overlayAccent = {0.34f, 0.78f, 0.97f, 0.95f};
    theme.overlayAccentSoft = {0.20f, 0.61f, 0.78f, 0.24f};
    return theme;
}

// Template preset intended for local customization.
inline DockTheme MakeTemplateTheme()
{
    DockTheme theme = MakeDarkTheme();
    // Example customization points:
    // theme.dockTitleBar = {0.19f, 0.24f, 0.40f, 1.0f};
    // theme.tabActive = {0.18f, 0.54f, 0.82f, 1.0f};
    // theme.overlayAccent = {0.98f, 0.64f, 0.21f, 0.95f};
    return theme;
}

inline std::string NormalizeThemeName(std::string name)
{
    std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return name;
}

inline DockTheme ThemeFromName(const std::string& name)
{
    const std::string key = NormalizeThemeName(name);
    if (key == "light") return MakeLightTheme();
    if (key == "slate") return MakeSlateTheme();
    if (key == "template") return MakeTemplateTheme();
    return MakeDarkTheme();
}

inline DockTheme& MutableTheme()
{
    static DockTheme theme = MakeDarkTheme();
    return theme;
}

inline const DockTheme& CurrentTheme()
{
    return MutableTheme();
}

inline void SetTheme(const DockTheme& theme)
{
    MutableTheme() = theme;
}

inline void SetThemeByName(const std::string& name)
{
    MutableTheme() = ThemeFromName(name);
}

} // namespace df

