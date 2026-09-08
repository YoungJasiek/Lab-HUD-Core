#pragma once
#ifndef LAB_HUD_EDITOR_H
#define LAB_HUD_EDITOR_H
#include <string>
#include <vector>
#include <array>
#include <memory>
#include <unordered_map>
#include <functional>
#include <algorithm>
#include "LabMath.h"
#include "LabRenderer.h"
#include "LabFont.h"

struct GLFWwindow;

namespace Lab {

    // =========================================================================
    // HUD Element Types & Enums
    // =========================================================================

    enum class HUDElementType : int {
        Rect = 0,
        Label = 1,
        Panel = 2,
        Card = 3,
        HealthBar = 4,
        AmmoCounter = 5,
        Crosshair = 6,
        Icon = 7,
        Image = 8,
        ProgressBar = 9,
        Button = 10
    };

    inline const char* hudElementTypeName(HUDElementType t) {
        switch (t) {
            case HUDElementType::Rect:         return "Rect";
            case HUDElementType::Label:        return "Label";
            case HUDElementType::Panel:        return "Panel";
            case HUDElementType::Card:         return "Card";
            case HUDElementType::HealthBar:    return "HealthBar";
            case HUDElementType::AmmoCounter:  return "AmmoCounter";
            case HUDElementType::Crosshair:    return "Crosshair";
            case HUDElementType::Icon:         return "Icon";
            case HUDElementType::Image:        return "Image";
            case HUDElementType::ProgressBar:  return "ProgressBar";
            case HUDElementType::Button:       return "Button";
            default:                           return "Rect";
        }
    }

    inline HUDElementType hudElementTypeFromName(const std::string& name) {
        if (name == "Rect")         return HUDElementType::Rect;
        if (name == "Label")        return HUDElementType::Label;
        if (name == "Panel")        return HUDElementType::Panel;
        if (name == "Card")         return HUDElementType::Card;
        if (name == "HealthBar")    return HUDElementType::HealthBar;
        if (name == "AmmoCounter")  return HUDElementType::AmmoCounter;
        if (name == "Crosshair")    return HUDElementType::Crosshair;
        if (name == "Icon")         return HUDElementType::Icon;
        if (name == "Image")        return HUDElementType::Image;
        if (name == "ProgressBar")  return HUDElementType::ProgressBar;
        if (name == "Button")       return HUDElementType::Button;
        return HUDElementType::Rect;
    }

    enum class HUDAnchor : int {
        TopLeft = 0,
        TopCenter = 1,
        TopRight = 2,
        CenterLeft = 3,
        Center = 4,
        CenterRight = 5,
        BottomLeft = 6,
        BottomCenter = 7,
        BottomRight = 8
    };

    inline const char* hudAnchorName(HUDAnchor a) {
        switch (a) {
            case HUDAnchor::TopLeft:      return "TopLeft";
            case HUDAnchor::TopCenter:    return "TopCenter";
            case HUDAnchor::TopRight:     return "TopRight";
            case HUDAnchor::CenterLeft:   return "CenterLeft";
            case HUDAnchor::Center:       return "Center";
            case HUDAnchor::CenterRight:  return "CenterRight";
            case HUDAnchor::BottomLeft:   return "BottomLeft";
            case HUDAnchor::BottomCenter: return "BottomCenter";
            case HUDAnchor::BottomRight:  return "BottomRight";
            default:                      return "TopLeft";
        }
    }

    inline HUDAnchor hudAnchorFromName(const std::string& name) {
        if (name == "TopLeft")      return HUDAnchor::TopLeft;
        if (name == "TopCenter")    return HUDAnchor::TopCenter;
        if (name == "TopRight")     return HUDAnchor::TopRight;
        if (name == "CenterLeft")   return HUDAnchor::CenterLeft;
        if (name == "Center")       return HUDAnchor::Center;
        if (name == "CenterRight")  return HUDAnchor::CenterRight;
        if (name == "BottomLeft")   return HUDAnchor::BottomLeft;
        if (name == "BottomCenter") return HUDAnchor::BottomCenter;
        if (name == "BottomRight")  return HUDAnchor::BottomRight;
        return HUDAnchor::TopLeft;
    }

    // =========================================================================
    // HUD Element — single 2D widget on the canvas
    // =========================================================================

    struct HUDElement {
        std::string id = "element";
        HUDElementType type = HUDElementType::Rect;
        float x = 0.0f, y = 0.0f;
        float w = 100.0f, h = 40.0f;
        Vec3 color{1.0f, 1.0f, 1.0f};
        float alpha = 1.0f;
        HUDAnchor anchor = HUDAnchor::TopLeft;
        int zOrder = 0;
        bool visible = true;

        // Text properties (Label, Button)
        std::string text;
        float fontSize = 2.0f;
        int fontType = 0; // 0=GeoSans, 1=System, 2=DotMatrix

        // Asset / texture (Image, Panel background)
        std::string texturePath;

        // Data binding (runtime: "player.health", "player.ammo", etc.)
        std::string binding;

        // Icon ID (for Icon type, maps to HammerIcons)
        int iconId = -1;

        // Border
        Vec3 borderColor{0.23f, 0.24f, 0.27f};
        float borderWidth = 1.0f;
        float borderAlpha = 1.0f;

        // ProgressBar value (design-time preview)
        float progressValue = 0.75f;

        // Hierarchy
        std::string parentId;
        std::vector<HUDElement> children;
    };

    // =========================================================================
    // HUD Project — full .labhud document
    // =========================================================================

    struct HUDProject {
        std::string name = "Untitled HUD";
        std::string author = "Unknown";
        float resolutionW = 1920.0f;
        float resolutionH = 1080.0f;
        int version = 1;
        std::vector<std::string> assets; // registered asset filenames
        std::vector<HUDElement> rootElements;

        bool saveToFile(const std::string& filepath) const;
        static std::unique_ptr<HUDProject> loadFromFile(const std::string& filepath);
    };

    // =========================================================================
    // Editor Mode / State
    // =========================================================================

    enum class HUDEditorMode : int {
        ProjectSelect = 0,
        Editor = 1
    };

    enum class HUDEditorTool : int {
        Select = 0,
        Move = 1,
        Resize = 2
    };

    enum class HUDEditorDropdown : int {
        None = -1,
        File = 0,
        Edit = 1,
        View = 2,
        Insert = 3,
        Assets = 4,
        Help = 5
    };

    // =========================================================================
    // Widget Library Entry (palette item)
    // =========================================================================

    struct HUDWidgetTemplate {
        std::string name;
        HUDElementType type;
        int iconId;
        float defaultW, defaultH;
        Vec3 defaultColor;
        float defaultAlpha;
    };

    // =========================================================================
    // LabHUDEditor2D — Main editor class
    // =========================================================================

    class LabHUDEditor2D {
    public:
        LabHUDEditor2D();
        ~LabHUDEditor2D();

        void init();
        void shutdown();

        void update(float dt, float mouseX, float mouseY, bool lmbPressed, bool rmbPressed, float scrollDelta = 0.0f);
        void render(int screenWidth, int screenHeight);

        void handleKeyDown(int key, bool ctrl, bool shift);

        // Window reference for file dialogs
        void setWindow(GLFWwindow* window) { _window = window; }
        GLFWwindow* getWindow() const { return _window; }

        bool requestExit() const { return _requestExit; }
        void clearRequestExit() { _requestExit = false; }

        // Undo / Redo
        void pushUndoState();
        void undo();
        void redo();

    private:
        // ---- Modes ----
        HUDEditorMode _mode = HUDEditorMode::ProjectSelect;
        HUDEditorTool _currentTool = HUDEditorTool::Select;
        HUDEditorDropdown _activeDropdown = HUDEditorDropdown::None;

        // ---- Project ----
        HUDProject _project;
        std::string _projectFilePath;
        bool _projectDirty = false;
        std::vector<std::string> _discoveredProjects; // .labhud file paths

        // ---- Selection ----
        std::vector<int> _selectedIndices; // indices into _project.rootElements
        int _hoveredIndex = -1;
        bool _isDragging = false;
        bool _isResizing = false;
        int _resizeHandle = -1; // 0-7: corners and edges
        float _dragOffsetX = 0.0f, _dragOffsetY = 0.0f;

        // ---- Marquee selection ----
        bool _isMarqueeSelecting = false;
        float _marqueeStartX = 0.0f, _marqueeStartY = 0.0f;
        float _marqueeEndX = 0.0f, _marqueeEndY = 0.0f;

        // ---- Canvas ----
        float _canvasZoom = 1.0f;
        float _canvasPanX = 0.0f, _canvasPanY = 0.0f;
        float _gridSnap = 10.0f;
        bool _showGrid = true;
        bool _showGuides = true;
        bool _isPanning = false;
        float _panStartX = 0.0f, _panStartY = 0.0f;
        float _panStartCanvasX = 0.0f, _panStartCanvasY = 0.0f;

        // Canvas viewport bounds (in screen space)
        float _viewportX = 0.0f, _viewportY = 0.0f;
        float _viewportW = 0.0f, _viewportH = 0.0f;

        // ---- Widget palette ----
        std::vector<HUDWidgetTemplate> _widgetTemplates;
        int _paletteHovered = -1;
        bool _isDraggingFromPalette = false;
        int _paletteDragIndex = -1;
        float _paletteScrollY = 0.0f;

        // ---- Property panel ----
        int _activePropertySlider = -1;
        int _propertyScrollY = 0;

        // ---- Asset browser ----
        bool _assetBrowserOpen = false;
        std::vector<std::string> _assetFiles; // files in assets/hud_assets/
        int _assetBrowserHovered = -1;
        float _assetBrowserScroll = 0.0f;
        std::unordered_map<std::string, std::unique_ptr<Texture>> _assetTextures;

        // ---- Undo/Redo ----
        struct EditorSnapshot {
            std::vector<HUDElement> elements;
        };
        std::vector<EditorSnapshot> _undoStack;
        std::vector<EditorSnapshot> _redoStack;

        // ---- Console ----
        std::vector<std::string> _consoleLogs;
        void log(const std::string& msg);

        // ---- Mouse state ----
        float _mouseX = 0.0f, _mouseY = 0.0f;
        float _lastMouseX = 0.0f, _lastMouseY = 0.0f;
        bool _lmbPressed = false, _lmbClicked = false;
        bool _rmbPressed = false;
        bool _lastLmb = false, _lastRmb = false;
        int _screenWidth = 1600, _screenHeight = 900;

        // ---- Window / Exit ----
        GLFWwindow* _window = nullptr;
        bool _requestExit = false;

        // ==== RENDERING ====

        // Project Selection Screen
        void renderProjectSelect(int w, int h);
        void updateProjectSelect(float dt);

        // Editor UI (Hammer style)
        void renderEditorUI(int w, int h);
        void renderTopMenuBar(float w);
        void renderToolbar(float w);
        void renderWidgetPalette(float x, float y, float w, float h);
        void renderCanvas(float x, float y, float w, float h);
        void renderPropertyPanel(float x, float y, float w, float h);
        void renderStatusBar(float w, float h);
        void renderDropdownMenus(float w, float h);
        void renderAssetBrowser(float screenW, float screenH);

        // Canvas element rendering
        void renderHUDElement(const HUDElement& elem, float canvasX, float canvasY, float zoom, int index);
        void renderSelectionHandles(const HUDElement& elem, float canvasX, float canvasY, float zoom);
        void renderAlignmentGuides(float canvasX, float canvasY, float zoom);
        void renderGrid(float x, float y, float w, float h, float canvasX, float canvasY, float zoom);

        // UI Helpers (Hammer-style widgets replicating LabStudio look)
        bool drawHammerButton(float x, float y, float w, float h, const std::string& label, bool active = false, bool highlighted = false);
        bool drawHammerSlider(float x, float y, float w, float h, const std::string& label, float& value, float minVal, float maxVal, const std::string& format = "%.2f");
        void drawHammerPanel(float x, float y, float w, float h, const std::string& title = "");
        void drawHammerBevel(float x, float y, float w, float h, bool sunken = false);
        bool drawHammerDropdownItem(float x, float y, float w, float h, const std::string& label, bool hovered);

        // Canvas coordinate conversions
        float screenToCanvasX(float sx) const;
        float screenToCanvasY(float sy) const;
        float canvasToScreenX(float cx) const;
        float canvasToScreenY(float cy) const;
        float snapToGrid(float v) const;

        // Anchor resolution
        Vec2 resolveAnchor(const HUDElement& elem) const;

        // Hit-testing
        int hitTestElement(float canvasX, float canvasY) const;
        int hitTestResizeHandle(float canvasX, float canvasY, int elemIndex) const;

        // Element operations
        void deleteSelectedElements();
        void duplicateSelectedElements();
        void moveSelectedZOrder(int delta);
        void sortByZOrder();
        int generateUniqueId();

        // Palette
        void initWidgetTemplates();
        HUDElement createFromTemplate(int templateIndex, float x, float y) const;

        // Asset scanning
        void scanAssets();
        void scanProjects();
        Texture* getAssetTexture(const std::string& path);

        // Serialization helpers
        static std::string resolveHUDPath(const std::string& path);
    };

} // namespace Lab

#endif // LAB_HUD_EDITOR_H
