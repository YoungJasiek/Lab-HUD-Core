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
    // Element Reference (Points to root element or nested child)
    // =========================================================================

    struct ElementRef {
        int rootIndex = -1;
        int childIndex = -1; // -1 if root element

        bool isValid() const { return rootIndex >= 0; }
        bool isChild() const { return rootIndex >= 0 && childIndex >= 0; }
        bool isRoot() const { return rootIndex >= 0 && childIndex == -1; }
        bool operator==(const ElementRef& o) const { return rootIndex == o.rootIndex && childIndex == o.childIndex; }
        bool operator!=(const ElementRef& o) const { return !(*this == o); }
        void invalidate() { rootIndex = -1; childIndex = -1; }
    };

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

        // Lua Scripting & Dynamic Game Logic
        std::string luaOnUpdate; // Called each tick to update text/visibility/state
        std::string luaOnClick;  // Called on click event
        std::string luaCustom;   // Arbitrary custom Lua snippet or script path

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
    // Editor Enums & UI Modes
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

    enum class HUDSidebarTab : int {
        Widgets = 0,
        Hierarchy = 1,
        Assets = 2
    };

    enum class ModalType : int {
        None = 0,
        EditText = 1,
        EditLua = 2,
        Rename = 3
    };

    struct ContextMenuItem {
        std::string label;
        std::function<void()> action;
        bool isSeparator = false;
        bool disabled = false;
    };

    struct HUDWidgetTemplate {
        std::string name;
        HUDElementType type;
        int iconId;
        float defaultW, defaultH;
        Vec3 defaultColor;
        float defaultAlpha;
        std::string defaultText;
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

        void setWindow(GLFWwindow* window) { _window = window; }
        GLFWwindow* getWindow() const { return _window; }

        bool requestExit() const { return _requestExit; }
        void clearRequestExit() { _requestExit = false; }

        void pushUndoState();
        void undo();
        void redo();

    private:
        // Modes & Tools
        HUDEditorMode _mode = HUDEditorMode::ProjectSelect;
        HUDEditorTool _currentTool = HUDEditorTool::Select;
        HUDEditorDropdown _activeDropdown = HUDEditorDropdown::None;
        HUDSidebarTab _sidebarTab = HUDSidebarTab::Widgets;

        // Project
        HUDProject _project;
        std::string _projectFilePath;
        bool _projectDirty = false;
        std::vector<std::string> _discoveredProjects;

        // Selection & Hierarchy
        ElementRef _selectedRef;
        std::vector<ElementRef> _selectedRefs;
        ElementRef _hoveredRef;

        bool _isDragging = false;
        bool _isResizing = false;
        int _resizeHandle = -1; // 0-7
        float _dragStartMouseCanvasX = 0.0f, _dragStartMouseCanvasY = 0.0f;
        Vec2 _dragStartElemAbsPos{0.0f, 0.0f};

        // Marquee
        bool _isMarqueeSelecting = false;
        float _marqueeStartX = 0.0f, _marqueeStartY = 0.0f;
        float _marqueeEndX = 0.0f, _marqueeEndY = 0.0f;

        // Canvas Zoom & Pan
        float _canvasZoom = 0.75f;
        float _canvasPanX = 20.0f, _canvasPanY = 20.0f;
        float _gridSnap = 10.0f;
        bool _showGrid = true;
        bool _showGuides = true;
        bool _showSafeZone = true;
        bool _clampToSafeZone = true;
        bool _isPanning = false;
        float _panStartX = 0.0f, _panStartY = 0.0f;
        float _panStartCanvasX = 0.0f, _panStartCanvasY = 0.0f;
        float _rmbPressStartX = 0.0f, _rmbPressStartY = 0.0f;

        // Viewport bounds in screen space
        float _viewportX = 260.0f, _viewportY = 76.0f;
        float _viewportW = 960.0f, _viewportH = 796.0f;

        // Widget Templates
        std::vector<HUDWidgetTemplate> _widgetTemplates;
        int _paletteHovered = -1;
        float _paletteScrollY = 0.0f;
        float _hierarchyScrollY = 0.0f;

        // Property Panel State
        float _propertyScrollY = 0.0f;

        // Custom Assets
        std::vector<std::string> _assetFiles;
        float _assetScrollY = 0.0f;
        std::unordered_map<std::string, std::unique_ptr<Texture>> _assetTextures;

        // Context Menu
        bool _contextMenuOpen = false;
        float _contextMenuX = 0.0f, _contextMenuY = 0.0f;
        std::vector<ContextMenuItem> _contextMenuItems;

        // In-Editor Modal (Text, Lua, Renaming)
        ModalType _modalType = ModalType::None;
        std::string _modalTitle;
        std::string _modalPrompt;
        std::string _modalBuffer;
        std::function<void(const std::string&)> _modalOnConfirm;

        // Undo / Redo
        struct EditorSnapshot {
            std::vector<HUDElement> elements;
        };
        std::vector<EditorSnapshot> _undoStack;
        std::vector<EditorSnapshot> _redoStack;

        // Logging & Diagnostics
        std::vector<std::string> _consoleLogs;
        void log(const std::string& msg);

        // Mouse & Window State
        float _mouseX = 0.0f, _mouseY = 0.0f;
        float _lastMouseX = 0.0f, _lastMouseY = 0.0f;
        bool _lmbPressed = false, _lmbClicked = false;
        bool _rmbPressed = false, _rmbClicked = false;
        bool _lastLmb = false, _lastRmb = false;
        int _screenWidth = 1600, _screenHeight = 900;
        GLFWwindow* _window = nullptr;
        bool _requestExit = false;

        // Clipboard
        std::unique_ptr<HUDElement> _clipboardElement;

        // =====================================================================
        // Rendering Functions
        // =====================================================================
        void renderProjectSelect(int w, int h);
        void updateProjectSelect(float dt);

        void renderEditorUI(int w, int h);
        void renderTopMenuBar(float w);
        void renderToolbar(float w);
        void renderLeftSidebar(float x, float y, float w, float h);
        void renderCanvas(float x, float y, float w, float h);
        void renderRightPropertyPanel(float x, float y, float w, float h);
        void renderStatusBar(float w, float h);
        void renderDropdownMenus(float w, float h);
        void renderContextMenu();
        void renderModalDialog();

        // Canvas Rendering Subsystems
        void renderGrid(float vx, float vy, float vw, float vh);
        void renderAlignmentGuides(float vx, float vy);
        void renderHUDElementRecursive(const HUDElement& elem, const Vec2& parentAbsPos, float vx, float vy, const ElementRef& ref);
        void renderSelectionOutlineAndHandles(const ElementRef& ref, float vx, float vy);

        // Hammer UI Widgets
        bool drawHammerButton(float x, float y, float w, float h, const std::string& label, bool active = false, bool highlighted = false);
        bool drawHammerSlider(float x, float y, float w, float h, const std::string& label, float& value, float minVal, float maxVal, const std::string& format = "%.2f");
        void drawHammerPanel(float x, float y, float w, float h, const std::string& title = "");
        void drawHammerBevel(float x, float y, float w, float h, bool sunken = false);
        bool drawHammerDropdownItem(float x, float y, float w, float h, const std::string& label, bool hovered, bool separator = false, bool disabled = false);

        // Coordinate System & Transformation Maths
        float screenToCanvasX(float sx) const;
        float screenToCanvasY(float sy) const;
        float canvasToScreenX(float cx) const;
        float canvasToScreenY(float cy) const;
        float snapToGrid(float v) const;

        // Hierarchical Geometry Calculation
        Vec2 getElementAbsPos(const ElementRef& ref) const;
        void setElementAbsPos(const ElementRef& ref, const Vec2& targetAbs);
        Vec2 getElementSize(const ElementRef& ref) const;

        // Element Access & Mutators
        HUDElement* getElement(const ElementRef& ref);
        const HUDElement* getElement(const ElementRef& ref) const;
        HUDElement* getParentElement(const ElementRef& ref);

        // Hit Testing
        ElementRef hitTest(float cx, float cy) const;
        int hitTestResizeHandle(float cx, float cy, const ElementRef& ref) const;

        // Element Management Operations
        void deleteSelected();
        void duplicateSelected();
        void moveZOrder(int delta);
        void sortRootByZOrder();
        void selectElement(const ElementRef& ref, bool addToSelection = false);
        void clearSelection();
        bool isSelected(const ElementRef& ref) const;

        // Context Menu Management
        void openContextMenu(float sx, float sy, bool onElement, const ElementRef& targetRef);
        void closeContextMenu();

        // Modals
        void openModal(ModalType type, const std::string& title, const std::string& prompt, const std::string& initialVal, std::function<void(const std::string&)> onConfirm);
        void closeModal();

        // Asset Management
        void scanAssets();
        void scanProjects();
        void importCustomAsset();
        Texture* getAssetTexture(const std::string& path);

        // Palette Management
        void initWidgetTemplates();
        HUDElement createFromTemplate(int templateIndex, float absX, float absY) const;

        // Project File Resolution
        static std::string resolveHUDPath(const std::string& path);
    };

} // namespace Lab

#endif // LAB_HUD_EDITOR_H
