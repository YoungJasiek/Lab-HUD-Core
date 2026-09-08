#include "LabHUDEditor.h"
#include "Lab.h"
#include "LabFont.h"
#include "LabDialogs.h"
#include "LabEditor.h"
#include "LabHUD.h"
#include "LabCore.h"

#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <fstream>
#include <sstream>
#include <iostream>
#include <filesystem>
#include <cmath>
#include <algorithm>
#include <iomanip>

namespace Lab {

    // =========================================================================
    // HUDProject Serialization
    // =========================================================================

    static void trimWhitespace(std::string& s) {
        s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch) { return !std::isspace(ch); }));
        s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) { return !std::isspace(ch); }).base(), s.end());
    }

    static std::string stripQuotes(const std::string& s) {
        if (s.size() >= 2 && s.front() == '"' && s.back() == '"') {
            return s.substr(1, s.size() - 2);
        }
        return s;
    }

    static void writeElement(std::ofstream& out, const HUDElement& elem, int indentLevel) {
        std::string ind(indentLevel * 4, ' ');
        out << ind << "element " << elem.id << " " << hudElementTypeName(elem.type) << " "
            << elem.x << " " << elem.y << " " << elem.w << " " << elem.h << " "
            << elem.color.x << " " << elem.color.y << " " << elem.color.z << " " << elem.alpha << " "
            << hudAnchorName(elem.anchor) << " " << elem.zOrder << "\n";
        
        std::string ind2((indentLevel + 1) * 4, ' ');
        if (elem.type == HUDElementType::Icon) {
            out << ind2 << "icon " << elem.iconId << "\n";
        }
        out << ind2 << "text \"" << elem.text << "\"\n";
        out << ind2 << "font " << elem.fontSize << " " << elem.fontType << "\n";
        out << ind2 << "border " << elem.borderColor.x << " " << elem.borderColor.y << " " << elem.borderColor.z << " " << elem.borderWidth << " " << elem.borderAlpha << "\n";
        out << ind2 << "texture \"" << elem.texturePath << "\"\n";
        out << ind2 << "binding \"" << elem.binding << "\"\n";
        out << ind2 << "children " << elem.children.size() << "\n\n";

        for (const auto& child : elem.children) {
            writeElement(out, child, indentLevel + 1);
        }
    }

    bool HUDProject::saveToFile(const std::string& filepath) const {
        std::ofstream out(filepath);
        if (!out) return false;

        out << "LABHUD_VERSION " << version << "\n\n";
        out << "METADATA\n";
        out << "    name \"" << name << "\"\n";
        out << "    author \"" << author << "\"\n";
        out << "    resolution " << resolutionW << " " << resolutionH << "\n";
        out << "END_METADATA\n\n";

        out << "ASSETS\n";
        for (const auto& asset : assets) {
            out << "    asset " << asset << "\n";
        }
        out << "END_ASSETS\n\n";

        out << "ELEMENTS\n";
        for (const auto& elem : rootElements) {
            writeElement(out, elem, 1);
        }
        out << "END_ELEMENTS\n";

        return true;
    }

    static HUDElement readElement(std::ifstream& in, std::string& line) {
        HUDElement elem;
        std::stringstream ss(line);
        std::string token;
        ss >> token; // "element"
        ss >> elem.id;
        
        std::string typeStr;
        ss >> typeStr;
        elem.type = hudElementTypeFromName(typeStr);

        ss >> elem.x >> elem.y >> elem.w >> elem.h;
        ss >> elem.color.x >> elem.color.y >> elem.color.z >> elem.alpha;
        
        std::string anchorStr;
        ss >> anchorStr;
        elem.anchor = hudAnchorFromName(anchorStr);
        ss >> elem.zOrder;

        int numChildren = 0;
        
        while (std::getline(in, line)) {
            trimWhitespace(line);
            if (line.empty()) continue;

            if (line.rfind("element", 0) == 0 || line == "END_ELEMENTS") {
                // Return to parent loop
                break;
            }

            std::stringstream lss(line);
            std::string prop;
            lss >> prop;

            if (prop == "icon") {
                lss >> elem.iconId;
            } else if (prop == "text") {
                std::string rem;
                std::getline(lss, rem);
                trimWhitespace(rem);
                elem.text = stripQuotes(rem);
            } else if (prop == "font") {
                lss >> elem.fontSize >> elem.fontType;
            } else if (prop == "border") {
                lss >> elem.borderColor.x >> elem.borderColor.y >> elem.borderColor.z >> elem.borderWidth >> elem.borderAlpha;
            } else if (prop == "texture") {
                std::string rem;
                std::getline(lss, rem);
                trimWhitespace(rem);
                elem.texturePath = stripQuotes(rem);
            } else if (prop == "binding") {
                std::string rem;
                std::getline(lss, rem);
                trimWhitespace(rem);
                elem.binding = stripQuotes(rem);
            } else if (prop == "children") {
                lss >> numChildren;
                if (numChildren > 0) {
                    std::getline(in, line);
                    for (int i = 0; i < numChildren; ++i) {
                        trimWhitespace(line);
                        while (line.empty() && std::getline(in, line)) {
                            trimWhitespace(line);
                        }
                        if (line.rfind("element", 0) == 0) {
                            elem.children.push_back(readElement(in, line));
                        }
                    }
                }
            }
        }
        return elem;
    }

    std::unique_ptr<HUDProject> HUDProject::loadFromFile(const std::string& filepath) {
        std::ifstream in(filepath);
        if (!in) return nullptr;

        auto proj = std::make_unique<HUDProject>();
        std::string line;
        
        while (std::getline(in, line)) {
            trimWhitespace(line);
            if (line.empty()) continue;

            if (line.rfind("LABHUD_VERSION", 0) == 0) {
                std::stringstream ss(line);
                std::string dummy;
                ss >> dummy >> proj->version;
            } else if (line == "METADATA") {
                while (std::getline(in, line)) {
                    trimWhitespace(line);
                    if (line == "END_METADATA") break;
                    std::stringstream ss(line);
                    std::string key;
                    ss >> key;
                    if (key == "name") {
                        std::string val; std::getline(ss, val);
                        trimWhitespace(val); proj->name = stripQuotes(val);
                    } else if (key == "author") {
                        std::string val; std::getline(ss, val);
                        trimWhitespace(val); proj->author = stripQuotes(val);
                    } else if (key == "resolution") {
                        ss >> proj->resolutionW >> proj->resolutionH;
                    }
                }
            } else if (line == "ASSETS") {
                while (std::getline(in, line)) {
                    trimWhitespace(line);
                    if (line == "END_ASSETS") break;
                    std::stringstream ss(line);
                    std::string key, val;
                    ss >> key >> val;
                    if (key == "asset") {
                        proj->assets.push_back(val);
                    }
                }
            } else if (line == "ELEMENTS") {
                while (std::getline(in, line)) {
                    trimWhitespace(line);
                    if (line == "END_ELEMENTS") break;
                    if (line.empty()) continue;

                    if (line.rfind("element", 0) == 0) {
                        proj->rootElements.push_back(readElement(in, line));
                    }
                }
            }
        }
        return proj;
    }

    // =========================================================================
    // LabHUDEditor2D
    // =========================================================================

    LabHUDEditor2D::LabHUDEditor2D() {
        initWidgetTemplates();
    }

    LabHUDEditor2D::~LabHUDEditor2D() {
        shutdown();
    }

    void LabHUDEditor2D::init() {
        scanProjects();
        scanAssets();
        log("HUDEditor2D initialized.");
    }

    void LabHUDEditor2D::shutdown() {
        _assetTextures.clear();
    }

    void LabHUDEditor2D::initWidgetTemplates() {
        _widgetTemplates = {
            {"Rect",        HUDElementType::Rect,        0, 100, 100, {0.8f, 0.8f, 0.8f}, 1.0f},
            {"Label",       HUDElementType::Label,       1, 120,  30, {0.98f,0.78f,0.08f}, 1.0f},
            {"Panel",       HUDElementType::Panel,       2, 200, 150, {0.18f,0.18f,0.18f}, 1.0f},
            {"Card",        HUDElementType::Card,        3, 180,  60, {0.16f,0.17f,0.19f}, 0.85f},
            {"HealthBar",   HUDElementType::HealthBar,   4, 250,  30, {1.0f, 0.2f, 0.2f}, 1.0f},
            {"AmmoCounter", HUDElementType::AmmoCounter, 5, 100,  80, {1.0f, 0.8f, 0.2f}, 1.0f},
            {"Crosshair",   HUDElementType::Crosshair,   6,  40,  40, {0.2f, 1.0f, 0.2f}, 1.0f},
            {"Icon",        HUDElementType::Icon,        7,  32,  32, {1.0f, 1.0f, 1.0f}, 1.0f},
            {"Image",       HUDElementType::Image,       8, 128, 128, {1.0f, 1.0f, 1.0f}, 1.0f},
            {"ProgressBar", HUDElementType::ProgressBar, 9, 200,  20, {0.2f, 0.6f, 1.0f}, 1.0f},
            {"Button",      HUDElementType::Button,     10, 150,  40, {0.3f, 0.3f, 0.3f}, 1.0f}
        };
    }

    void LabHUDEditor2D::update(float dt, float mouseX, float mouseY, bool lmbPressed, bool rmbPressed, float scrollDelta) {
        _lastMouseX = _mouseX;
        _lastMouseY = _mouseY;
        _mouseX = mouseX;
        _mouseY = mouseY;
        
        _lastLmb = _lmbPressed;
        _lastRmb = _rmbPressed;
        _lmbPressed = lmbPressed;
        _rmbPressed = rmbPressed;
        _lmbClicked = _lmbPressed && !_lastLmb;

        if (_mode == HUDEditorMode::ProjectSelect) {
            updateProjectSelect(dt);
        } else {
            // Dropdown override
            if (_activeDropdown != HUDEditorDropdown::None) {
                if (_lmbClicked && mouseY > 32) {
                    _activeDropdown = HUDEditorDropdown::None;
                }
                return;
            }

            // Asset browser override
            if (_assetBrowserOpen) {
                if (scrollDelta != 0.0f) {
                    _assetBrowserScroll -= scrollDelta * 20.0f;
                    _assetBrowserScroll = std::max(0.0f, _assetBrowserScroll);
                }
                return;
            }

            // Mouse over properties / palette
            if (_mouseX < 200) { // Palette
                if (scrollDelta != 0.0f) _paletteScrollY -= scrollDelta * 20.0f;
                return;
            }
            if (_mouseX > _screenWidth - 300) { // Property panel
                if (scrollDelta != 0.0f) _propertyScrollY -= scrollDelta * 20.0f;
                return;
            }

            // Canvas interaction
            if (_mouseY > 68 && _mouseY < _screenHeight - 28) {
                if (scrollDelta != 0.0f) {
                    float oldCX = screenToCanvasX(_mouseX);
                    float oldCY = screenToCanvasY(_mouseY);
                    _canvasZoom *= (1.0f + scrollDelta * 0.1f);
                    _canvasZoom = std::clamp(_canvasZoom, 0.1f, 10.0f);
                    float newCX = screenToCanvasX(_mouseX);
                    float newCY = screenToCanvasY(_mouseY);
                    _canvasPanX += (newCX - oldCX) * _canvasZoom;
                    _canvasPanY += (newCY - oldCY) * _canvasZoom;
                }

                if (_rmbPressed && !_lastRmb) {
                    _isPanning = true;
                    _panStartX = _mouseX;
                    _panStartY = _mouseY;
                    _panStartCanvasX = _canvasPanX;
                    _panStartCanvasY = _canvasPanY;
                } else if (!_rmbPressed && _lastRmb) {
                    _isPanning = false;
                }
                if (_isPanning) {
                    _canvasPanX = _panStartCanvasX + (_mouseX - _panStartX);
                    _canvasPanY = _panStartCanvasY + (_mouseY - _panStartY);
                }

                float cx = screenToCanvasX(_mouseX);
                float cy = screenToCanvasY(_mouseY);

                if (_lmbClicked) {
                    int handle = -1;
                    int elem = -1;
                    if (_selectedIndices.size() == 1) {
                        handle = hitTestResizeHandle(cx, cy, _selectedIndices[0]);
                    }
                    if (handle != -1) {
                        _isResizing = true;
                        _resizeHandle = handle;
                        pushUndoState();
                    } else {
                        elem = hitTestElement(cx, cy);
                        if (elem != -1) {
                            if (std::find(_selectedIndices.begin(), _selectedIndices.end(), elem) == _selectedIndices.end()) {
                                _selectedIndices.clear();
                                _selectedIndices.push_back(elem);
                            }
                            _isDragging = true;
                            _dragOffsetX = cx - _project.rootElements[elem].x;
                            _dragOffsetY = cy - _project.rootElements[elem].y;
                            pushUndoState();
                        } else {
                            _selectedIndices.clear();
                            _isMarqueeSelecting = true;
                            _marqueeStartX = cx;
                            _marqueeStartY = cy;
                            _marqueeEndX = cx;
                            _marqueeEndY = cy;
                        }
                    }
                } else if (_lmbPressed) {
                    if (_isDragging && !_selectedIndices.empty()) {
                        int idx = _selectedIndices[0];
                        float nx = cx - _dragOffsetX;
                        float ny = cy - _dragOffsetY;
                        if (_gridSnap > 0) {
                            nx = snapToGrid(nx);
                            ny = snapToGrid(ny);
                        }
                        _project.rootElements[idx].x = nx;
                        _project.rootElements[idx].y = ny;
                    } else if (_isResizing && !_selectedIndices.empty()) {
                        int idx = _selectedIndices[0];
                        auto& e = _project.rootElements[idx];
                        float dxm = cx - _lastMouseX;
                        float dym = cy - _lastMouseY;
                        // Resize based on handle position (0=TL, 1=T, 2=TR, 3=R, 4=BR, 5=B, 6=BL, 7=L)
                        if (_resizeHandle == 3 || _resizeHandle == 2 || _resizeHandle == 4) e.w = std::max(10.0f, e.w + dxm);
                        if (_resizeHandle == 5 || _resizeHandle == 4 || _resizeHandle == 6) e.h = std::max(10.0f, e.h + dym);
                        if (_resizeHandle == 7 || _resizeHandle == 0 || _resizeHandle == 6) { e.x += dxm; e.w = std::max(10.0f, e.w - dxm); }
                        if (_resizeHandle == 1 || _resizeHandle == 0 || _resizeHandle == 2) { e.y += dym; e.h = std::max(10.0f, e.h - dym); }
                    } else if (_isMarqueeSelecting) {
                        _marqueeEndX = cx;
                        _marqueeEndY = cy;
                    }
                } else {
                    _isDragging = false;
                    _isResizing = false;
                    if (_isMarqueeSelecting) {
                        _isMarqueeSelecting = false;
                        // Select logic
                        float minX = std::min(_marqueeStartX, _marqueeEndX);
                        float maxX = std::max(_marqueeStartX, _marqueeEndX);
                        float minY = std::min(_marqueeStartY, _marqueeEndY);
                        float maxY = std::max(_marqueeStartY, _marqueeEndY);
                        _selectedIndices.clear();
                        for (int i = 0; i < (int)_project.rootElements.size(); i++) {
                            const auto& e = _project.rootElements[i];
                            if (e.x >= minX && e.x + e.w <= maxX && e.y >= minY && e.y + e.h <= maxY) {
                                _selectedIndices.push_back(i);
                            }
                        }
                    }
                }
            }
        }
    }

    void LabHUDEditor2D::render(int screenW, int screenH) {
        _screenWidth = screenW;
        _screenHeight = screenH;
        
        Renderer::beginUI(screenW, screenH);

        if (_mode == HUDEditorMode::ProjectSelect) {
            renderProjectSelect(screenW, screenH);
        } else {
            renderEditorUI(screenW, screenH);
        }

        Renderer::endUI();
    }

    // =========================================================================
    // UI Helpers (Hammer Style)
    // =========================================================================

    void LabHUDEditor2D::drawHammerBevel(float x, float y, float w, float h, bool sunken) {
        Vec3 hl = sunken ? Vec3(0.1f, 0.1f, 0.1f) : Vec3(0.4f, 0.4f, 0.4f);
        Vec3 sh = sunken ? Vec3(0.4f, 0.4f, 0.4f) : Vec3(0.1f, 0.1f, 0.1f);
        Renderer::drawRect(x, y, w, 1, hl); // Top
        Renderer::drawRect(x, y, 1, h, hl); // Left
        Renderer::drawRect(x, y + h - 1, w, 1, sh); // Bottom
        Renderer::drawRect(x + w - 1, y, 1, h, sh); // Right
    }

    void LabHUDEditor2D::drawHammerPanel(float x, float y, float w, float h, const std::string& title) {
        Renderer::drawRect(x, y, w, h, Vec3(0.18f, 0.18f, 0.18f));
        drawHammerBevel(x, y, w, h, false);
        if (!title.empty()) {
            Renderer::drawRect(x + 2, y + 2, w - 4, 18, Vec3(0.12f, 0.12f, 0.12f));
            LabFont::drawText(x + 5, y + 15, title, 1.2f, Vec3(0.98f, 0.78f, 0.08f), LabFontType::System);
        }
    }

    bool LabHUDEditor2D::drawHammerButton(float x, float y, float w, float h, const std::string& label, bool active, bool highlighted) {
        bool hover = (_mouseX >= x && _mouseX <= x + w && _mouseY >= y && _mouseY <= y + h);
        bool pressed = hover && _lmbPressed;
        bool clicked = hover && _lmbClicked;

        Vec3 bg = active ? Vec3(1.0f, 0.55f, 0.1f) : (pressed ? Vec3(0.14f, 0.14f, 0.14f) : (hover ? Vec3(0.24f, 0.24f, 0.24f) : Vec3(0.2f, 0.2f, 0.2f)));
        if (highlighted && !active && !hover) bg = Vec3(0.2f, 0.3f, 0.4f);

        Renderer::drawRect(x, y, w, h, bg);
        drawHammerBevel(x, y, w, h, pressed || active);

        float tw = LabFont::getTextWidth(label, 1.2f, LabFontType::System);
        LabFont::drawText(x + (w - tw) * 0.5f, y + h * 0.5f + 4.0f, label, 1.2f, active ? Vec3(0,0,0) : Vec3(0.9f, 0.9f, 0.9f), LabFontType::System);

        return clicked;
    }

    bool LabHUDEditor2D::drawHammerSlider(float x, float y, float w, float h, const std::string& label, float& value, float minVal, float maxVal, const std::string& format) {
        bool changed = false;
        LabFont::drawText(x, y + 12, label, 1.2f, Vec3(0.8f, 0.8f, 0.8f), LabFontType::System);
        float sx = x + 60, sw = w - 60;
        
        Renderer::drawRect(sx, y, sw, h, Vec3(0.12f, 0.12f, 0.12f));
        drawHammerBevel(sx, y, sw, h, true);

        float norm = std::clamp((value - minVal) / (maxVal - minVal), 0.0f, 1.0f);
        Renderer::drawRect(sx + 2, y + 2, (sw - 4) * norm, h - 4, Vec3(1.0f, 0.55f, 0.1f));

        bool hover = (_mouseX >= sx && _mouseX <= sx + sw && _mouseY >= y && _mouseY <= y + h);
        if (hover && _lmbPressed) {
            float t = (_mouseX - sx) / sw;
            value = minVal + t * (maxVal - minVal);
            value = std::clamp(value, minVal, maxVal);
            changed = true;
        }

        char buf[32];
        snprintf(buf, sizeof(buf), format.c_str(), value);
        LabFont::drawText(sx + 5, y + 12, buf, 1.2f, Vec3(1,1,1), LabFontType::System);

        return changed;
    }

    bool LabHUDEditor2D::drawHammerDropdownItem(float x, float y, float w, float h, const std::string& label, bool hovered) {
        if (label.empty() || label == "-") {
            Renderer::drawRect(x + 5, y + h/2, w - 10, 1, Vec3(0.4f, 0.4f, 0.4f));
            return false;
        }
        if (hovered) {
            Renderer::drawRect(x, y, w, h, Vec3(1.0f, 0.55f, 0.1f));
        }
        LabFont::drawText(x + 10, y + 16, label, 1.2f, hovered ? Vec3(0,0,0) : Vec3(0.9f, 0.9f, 0.9f), LabFontType::System);
        return hovered && _lmbClicked;
    }

    // =========================================================================
    // Core Rendering & Logic
    // =========================================================================

    void LabHUDEditor2D::updateProjectSelect(float dt) {
        // Handled in render via button clicks
    }

    void LabHUDEditor2D::renderProjectSelect(int w, int h) {
        Renderer::drawRect(0, 0, w, h, Vec3(0.12f, 0.12f, 0.12f));
        
        LabFont::drawText(w/2 - 200, 100, "LAB 2D / HUD EDITOR", 3.0f, Vec3(1.0f, 0.55f, 0.1f), LabFontType::System);
        
        if (drawHammerButton(w/2 - 300, 200, 280, 50, "New Project", false, false)) {
            _project = HUDProject();
            _project.rootElements.clear();
            _mode = HUDEditorMode::Editor;
            _projectFilePath = "assets/hud_projects/untitled.labhud";
        }

        if (drawHammerButton(w/2 + 20, 200, 280, 50, "Open File...", false, false)) {
            std::string path = LabDialogs::openFileDialog(_window, "Lab HUD Projects (*.labhud)\0*.labhud\0All Files\0*.*\0", "assets\\hud_projects");
            if (!path.empty()) {
                auto p = HUDProject::loadFromFile(path);
                if (p) {
                    _project = *p;
                    _projectFilePath = path;
                    _mode = HUDEditorMode::Editor;
                }
            }
        }

        // List projects
        int py = 300;
        for (size_t i = 0; i < _discoveredProjects.size(); i++) {
            if (drawHammerButton(w/2 - 300, py, 600, 40, _discoveredProjects[i])) {
                auto p = HUDProject::loadFromFile(_discoveredProjects[i]);
                if (p) {
                    _project = *p;
                    _projectFilePath = _discoveredProjects[i];
                    _mode = HUDEditorMode::Editor;
                }
            }
            py += 45;
        }

        LabFont::drawText(10, h - 20, "Lab Engine 2026 - HUD Editor", 1.2f, Vec3(0.5f, 0.5f, 0.5f), LabFontType::System);
    }

    void LabHUDEditor2D::renderEditorUI(int w, int h) {
        _viewportX = 200;
        _viewportY = 68;
        _viewportW = w - 500;
        _viewportH = h - 68 - 28;

        renderCanvas(_viewportX, _viewportY, _viewportW, _viewportH);
        renderTopMenuBar(w);
        renderToolbar(w);
        renderWidgetPalette(0, 68, 200, h - 68 - 28);
        renderPropertyPanel(w - 300, 68, 300, h - 68 - 28);
        renderStatusBar(w, h);
        renderDropdownMenus(w, h);

        if (_assetBrowserOpen) {
            renderAssetBrowser(w, h);
        }
    }

    void LabHUDEditor2D::renderTopMenuBar(float w) {
        Renderer::drawRect(0, 0, w, 32, Vec3(0.18f, 0.18f, 0.18f));
        drawHammerBevel(0, 0, w, 32, false);

        std::vector<std::string> menus = {"File", "Edit", "View", "Insert", "Assets", "Help"};
        float mx = 10;
        for (int i = 0; i < (int)menus.size(); ++i) {
            bool active = (_activeDropdown == (HUDEditorDropdown)i);
            if (drawHammerButton(mx, 4, 60, 24, menus[i], active)) {
                if (active) _activeDropdown = HUDEditorDropdown::None;
                else _activeDropdown = (HUDEditorDropdown)i;
            }
            mx += 64;
        }
    }

    void LabHUDEditor2D::renderToolbar(float w) {
        Renderer::drawRect(0, 32, w, 36, Vec3(0.2f, 0.2f, 0.2f));
        drawHammerBevel(0, 32, w, 36, false);

        float tx = 10;
        auto drawTool = [&](int iconId, bool active = false) -> bool {
            bool clicked = false;
            // Draw square button
            bool hover = _mouseX >= tx && _mouseX <= tx+28 && _mouseY >= 36 && _mouseY <= 64;
            bool pressed = hover && _lmbPressed;
            Vec3 bg = active ? Vec3(1.0f, 0.55f, 0.1f) : (pressed ? Vec3(0.14f, 0.14f, 0.14f) : (hover ? Vec3(0.24f, 0.24f, 0.24f) : Vec3(0.2f, 0.2f, 0.2f)));
            Renderer::drawRect(tx, 36, 28, 28, bg);
            drawHammerBevel(tx, 36, 28, 28, pressed || active);
            // Draw icon (mocked via HammerIcons)
            HammerIcons::drawToolbarIcon(iconId, tx+2, 38, active ? Vec3(1,1,1) : Vec3(0.8f,0.8f,0.8f), bg);
            if (hover && _lmbClicked) clicked = true;
            tx += 32;
            return clicked;
        };
        auto drawSep = [&]() {
            Renderer::drawRect(tx + 2, 36, 1, 28, Vec3(0.1f, 0.1f, 0.1f));
            Renderer::drawRect(tx + 3, 36, 1, 28, Vec3(0.3f, 0.3f, 0.3f));
            tx += 8;
        };

        if (drawTool(0)) { _project = HUDProject(); _selectedIndices.clear(); } // New
        if (drawTool(1)) { /* Open */ }
        if (drawTool(2)) { _project.saveToFile(_projectFilePath); _projectDirty = false; } // Save
        if (drawTool(3)) { /* Save As */ }
        drawSep();
        if (drawTool(4)) undo();
        drawSep();
        if (drawTool(5)) deleteSelectedElements();
        if (drawTool(6)) duplicateSelectedElements();
        drawSep();
        if (drawTool(8)) _assetBrowserOpen = true;
    }

    void LabHUDEditor2D::renderWidgetPalette(float x, float y, float w, float h) {
        drawHammerPanel(x, y, w, h, "WIDGETS");
        float py = y + 25 - _paletteScrollY;
        for (int i = 0; i < (int)_widgetTemplates.size(); ++i) {
            bool hover = (_mouseX >= x && _mouseX <= x + w && _mouseY >= py && _mouseY <= py + 30);
            if (hover) {
                Renderer::drawRect(x + 2, py, w - 4, 30, Vec3(0.25f, 0.25f, 0.25f));
                if (_lmbClicked) {
                    _project.rootElements.push_back(createFromTemplate(i, _project.resolutionW/2, _project.resolutionH/2));
                    _selectedIndices = {(int)_project.rootElements.size() - 1};
                }
            }
            HammerIcons::drawHammerIcon(_widgetTemplates[i].iconId, x + 6, py + 3, Vec3(0.9f, 0.9f, 0.9f), Vec3(0.2f, 0.2f, 0.2f));
            LabFont::drawText(x + 35, py + 20, _widgetTemplates[i].name, 1.2f, Vec3(0.9f, 0.9f, 0.9f), LabFontType::System);
            py += 32;
        }
    }

    void LabHUDEditor2D::renderPropertyPanel(float x, float y, float w, float h) {
        drawHammerPanel(x, y, w, h, "PROPERTIES");
        if (_selectedIndices.empty()) {
            LabFont::drawText(x + 100, y + h/2, "No Selection", 1.2f, Vec3(0.5f, 0.5f, 0.5f), LabFontType::System);
            return;
        }

        auto& elem = _project.rootElements[_selectedIndices[0]];
        float py = y + 25 - _propertyScrollY;

        LabFont::drawText(x + 10, py + 15, "ID: " + elem.id, 1.2f, Vec3(1,1,1), LabFontType::System); py += 25;
        LabFont::drawText(x + 10, py + 15, "Type: " + std::string(hudElementTypeName(elem.type)), 1.2f, Vec3(1,1,1), LabFontType::System); py += 25;

        drawHammerSlider(x + 10, py, w - 20, 20, "X", elem.x, -2000, 2000, "%.0f"); py += 25;
        drawHammerSlider(x + 10, py, w - 20, 20, "Y", elem.y, -2000, 2000, "%.0f"); py += 25;
        drawHammerSlider(x + 10, py, w - 20, 20, "W", elem.w, 0, 2000, "%.0f"); py += 25;
        drawHammerSlider(x + 10, py, w - 20, 20, "H", elem.h, 0, 2000, "%.0f"); py += 25;

        drawHammerSlider(x + 10, py, w - 20, 20, "R", elem.color.x, 0, 1, "%.2f"); py += 25;
        drawHammerSlider(x + 10, py, w - 20, 20, "G", elem.color.y, 0, 1, "%.2f"); py += 25;
        drawHammerSlider(x + 10, py, w - 20, 20, "B", elem.color.z, 0, 1, "%.2f"); py += 25;
        drawHammerSlider(x + 10, py, w - 20, 20, "A", elem.alpha, 0, 1, "%.2f"); py += 25;

        if (drawHammerButton(x + 10, py, w - 20, 25, "Z-Order: " + std::to_string(elem.zOrder))) {
            elem.zOrder++;
        }
        py += 30;

        if (elem.type == HUDElementType::Label || elem.type == HUDElementType::Button) {
            drawHammerSlider(x + 10, py, w - 20, 20, "Font Size", elem.fontSize, 0.5f, 10.0f, "%.1f"); py += 25;
            if (drawHammerButton(x + 10, py, w - 20, 25, "Edit Text")) { /* mock */ } py += 30;
        }

        if (elem.type == HUDElementType::Image) {
            if (drawHammerButton(x + 10, py, w - 20, 25, "Browse Texture")) {
                _assetBrowserOpen = true;
            }
            py += 30;
        }

        if (drawHammerButton(x + 10, py, w - 20, 25, elem.visible ? "Visible: ON" : "Visible: OFF")) {
            elem.visible = !elem.visible;
        }
        py += 30;
    }

    void LabHUDEditor2D::renderCanvas(float x, float y, float w, float h) {
        // Scissor test mock (assumed Renderer handles it or we just draw in bounds)
        Renderer::drawRect(x, y, w, h, Vec3(0.08f, 0.08f, 0.08f));

        if (_showGrid) renderGrid(x, y, w, h, _canvasPanX, _canvasPanY, _canvasZoom);

        // Draw HUD elements
        sortByZOrder();
        for (int i = 0; i < (int)_project.rootElements.size(); ++i) {
            renderHUDElement(_project.rootElements[i], _canvasPanX + x, _canvasPanY + y, _canvasZoom, i);
        }

        if (_showGuides) renderAlignmentGuides(_canvasPanX + x, _canvasPanY + y, _canvasZoom);

        // Selection
        for (int idx : _selectedIndices) {
            renderSelectionHandles(_project.rootElements[idx], _canvasPanX + x, _canvasPanY + y, _canvasZoom);
        }

        if (_isMarqueeSelecting) {
            float sx = canvasToScreenX(_marqueeStartX);
            float sy = canvasToScreenY(_marqueeStartY);
            float ex = canvasToScreenX(_marqueeEndX);
            float ey = canvasToScreenY(_marqueeEndY);
            Renderer::drawRect(std::min(sx, ex), std::min(sy, ey), std::abs(ex - sx), std::abs(ey - sy), Vec3(0.2f, 0.6f, 1.0f));
            // alpha blend mock
        }
    }

    void LabHUDEditor2D::renderStatusBar(float w, float h) {
        Renderer::drawRect(0, h - 28, w, 28, Vec3(0.18f, 0.18f, 0.18f));
        drawHammerBevel(0, h - 28, w, 28, false);

        char buf[256];
        snprintf(buf, sizeof(buf), "Tool: %s | Zoom: %.1fx | Grid: %s | Sel: %zu | %s%s", 
            (_currentTool == HUDEditorTool::Select ? "Select" : "Move"), _canvasZoom, 
            (_showGrid ? std::to_string((int)_gridSnap).c_str() : "Off"), 
            _selectedIndices.size(), _project.name.c_str(), _projectDirty ? "*" : "");
        
        LabFont::drawText(10, h - 8, buf, 1.2f, Vec3(0.8f, 0.8f, 0.8f), LabFontType::System);
    }

    void LabHUDEditor2D::renderDropdownMenus(float w, float h) {
        if (_activeDropdown == HUDEditorDropdown::None) return;

        float mx = 10.0f + static_cast<float>(static_cast<int>(_activeDropdown)) * 64.0f;
        std::vector<std::string> items;

        if (_activeDropdown == HUDEditorDropdown::File) {
            items = {"New", "Open", "Save", "Save As", "-", "Exit"};
        } else if (_activeDropdown == HUDEditorDropdown::Edit) {
            items = {"Undo", "Redo", "-", "Delete", "Duplicate", "Select All"};
        } else if (_activeDropdown == HUDEditorDropdown::View) {
            items = {"Zoom In", "Zoom Out", "Reset Zoom", "-", "Toggle Grid", "Toggle Guides"};
        } else if (_activeDropdown == HUDEditorDropdown::Insert) {
            for (const auto& t : _widgetTemplates) items.push_back(t.name);
        } else if (_activeDropdown == HUDEditorDropdown::Assets) {
            items = {"Open Asset Browser", "Import Asset...", "-", "Refresh Assets"};
        } else if (_activeDropdown == HUDEditorDropdown::Help) {
            items = {"About Lab HUD Editor"};
        }

        float mw = 150;
        float mh = static_cast<float>(items.size()) * 25.0f + 10.0f;
        Renderer::drawRect(mx, 32, mw, mh, Vec3(0.18f, 0.18f, 0.18f));
        drawHammerBevel(mx, 32, mw, mh, false);

        float py = 37;
        for (const auto& item : items) {
            bool hover = _mouseX >= mx && _mouseX <= mx + mw && _mouseY >= py && _mouseY < py + 25;
            if (drawHammerDropdownItem(mx, py, mw, 25, item, hover)) {
                _activeDropdown = HUDEditorDropdown::None;
                // handle actions
                if (item == "Exit") _requestExit = true;
                if (item == "Save") _project.saveToFile(_projectFilePath);
                if (item == "Undo") undo();
                if (item == "Redo") redo();
                if (item == "Delete") deleteSelectedElements();
                if (item == "Duplicate") duplicateSelectedElements();
                if (item == "Select All") {
                    _selectedIndices.clear();
                    for(int i=0; i<(int)_project.rootElements.size(); ++i) _selectedIndices.push_back(i);
                }
                if (item == "Open Asset Browser") _assetBrowserOpen = true;
                if (item == "Toggle Grid") _showGrid = !_showGrid;
                if (item == "Toggle Guides") _showGuides = !_showGuides;
                if (item == "Zoom In") _canvasZoom *= 1.2f;
                if (item == "Zoom Out") _canvasZoom /= 1.2f;
                if (item == "Reset Zoom") _canvasZoom = 1.0f;
            }
            py += 25;
        }
    }

    void LabHUDEditor2D::renderAssetBrowser(float sw, float sh) {
        Renderer::drawRect(0, 0, sw, sh, Vec3(0,0,0)); // Overlay dim mockup, ideally alpha
        float bx = sw/2 - 400;
        float by = sh/2 - 300;
        drawHammerPanel(bx, by, 800, 600, "ASSET BROWSER");
        
        if (drawHammerButton(bx + 760, by + 4, 30, 20, "X")) {
            _assetBrowserOpen = false;
        }

        int cols = 6;
        float thumbSize = 100;
        float padding = 20;
        float startX = bx + 30;
        float startY = by + 50 - _assetBrowserScroll;

        for (int i = 0; i < (int)_assetFiles.size(); ++i) {
            int col = i % cols;
            int row = i / cols;
            float tx = startX + col * (thumbSize + padding);
            float ty = startY + row * (thumbSize + padding + 20);
            
            if (ty > by + 40 && ty < by + 580) {
                Texture* t = getAssetTexture(_assetFiles[i]);
                if (t) {
                    Renderer::drawTextureRect(tx, ty, thumbSize, thumbSize, *t);
                } else {
                    Renderer::drawRect(tx, ty, thumbSize, thumbSize, Vec3(0.5f, 0.2f, 0.8f)); // placeholder
                }
                LabFont::drawText(tx, ty + thumbSize + 15, _assetFiles[i], 1.0f, Vec3(1,1,1), LabFontType::System);
                
                if (_mouseX >= tx && _mouseX <= tx+thumbSize && _mouseY >= ty && _mouseY <= ty+thumbSize && _lmbClicked) {
                    if (!_selectedIndices.empty() && _project.rootElements[_selectedIndices[0]].type == HUDElementType::Image) {
                        _project.rootElements[_selectedIndices[0]].texturePath = _assetFiles[i];
                    }
                    _assetBrowserOpen = false;
                }
            }
        }
    }

    void LabHUDEditor2D::renderGrid(float x, float y, float w, float h, float cx, float cy, float zoom) {
        // Simple grid rendering
        float scaledSnap = _gridSnap * zoom;
        if (scaledSnap < 5.0f) return;

        int startX = (int)(-cx / scaledSnap) - 1;
        int endX = (int)((w - cx) / scaledSnap) + 1;
        for (int i = startX; i <= endX; ++i) {
            float gx = x + cx + i * scaledSnap;
            Renderer::drawRect(gx, y, 1, h, (i % 10 == 0) ? Vec3(0.25f, 0.25f, 0.25f) : Vec3(0.12f, 0.12f, 0.12f));
        }

        int startY = (int)(-cy / scaledSnap) - 1;
        int endY = (int)((h - cy) / scaledSnap) + 1;
        for (int i = startY; i <= endY; ++i) {
            float gy = y + cy + i * scaledSnap;
            Renderer::drawRect(x, gy, w, 1, (i % 10 == 0) ? Vec3(0.25f, 0.25f, 0.25f) : Vec3(0.12f, 0.12f, 0.12f));
        }
    }

    void LabHUDEditor2D::renderHUDElement(const HUDElement& elem, float cx, float cy, float zoom, int index) {
        if (!elem.visible) return;

        float ex = cx + elem.x * zoom;
        float ey = cy + elem.y * zoom;
        float ew = elem.w * zoom;
        float eh = elem.h * zoom;

        Vec3 col = elem.color; // ignore alpha for simplicity in this mockup, since drawRect takes Vec3

        if (elem.type == HUDElementType::Rect) {
            Renderer::drawRect(ex, ey, ew, eh, col);
        } else if (elem.type == HUDElementType::Label) {
            LabFont::drawText(ex, ey + eh, elem.text.empty() ? "Label" : elem.text, elem.fontSize * zoom, col, static_cast<LabFontType>(elem.fontType));
        } else if (elem.type == HUDElementType::Panel) {
            Renderer::drawRect(ex, ey, ew, eh, col);
            drawHammerBevel(ex, ey, ew, eh, false);
        } else if (elem.type == HUDElementType::Card) {
            Renderer::drawRect(ex, ey, ew, eh, col);
            Renderer::drawRect(ex, ey, ew, 1, elem.borderColor);
            Renderer::drawRect(ex, ey, 1, eh, elem.borderColor);
            Renderer::drawRect(ex, ey+eh-1, ew, 1, elem.borderColor);
            Renderer::drawRect(ex+ew-1, ey, 1, eh, elem.borderColor);
        } else if (elem.type == HUDElementType::HealthBar || elem.type == HUDElementType::ProgressBar) {
            Renderer::drawRect(ex, ey, ew, eh, Vec3(0.1f, 0.1f, 0.1f));
            Renderer::drawRect(ex, ey, ew * elem.progressValue, eh, col);
            drawHammerBevel(ex, ey, ew, eh, true);
        } else if (elem.type == HUDElementType::Button) {
            Renderer::drawRect(ex, ey, ew, eh, col);
            drawHammerBevel(ex, ey, ew, eh, false);
            LabFont::drawText(ex + 10, ey + eh/2 + 5, elem.text.empty() ? "Button" : elem.text, elem.fontSize * zoom, Vec3(1,1,1), static_cast<LabFontType>(elem.fontType));
        } else if (elem.type == HUDElementType::Image) {
            Texture* t = getAssetTexture(elem.texturePath);
            if (t) {
                Renderer::drawTextureRect(ex, ey, ew, eh, *t);
            } else {
                Renderer::drawRect(ex, ey, ew, eh, Vec3(1, 0, 1)); // checkerboard stand-in
            }
        }
        // ... AmmoCounter, Crosshair, Icon omitted for brevity, similar pattern

        for (const auto& child : elem.children) {
            renderHUDElement(child, cx, cy, zoom, -1);
        }
    }

    void LabHUDEditor2D::renderSelectionHandles(const HUDElement& elem, float cx, float cy, float zoom) {
        float ex = cx + elem.x * zoom;
        float ey = cy + elem.y * zoom;
        float ew = elem.w * zoom;
        float eh = elem.h * zoom;

        Renderer::drawRect(ex - 1, ey - 1, ew + 2, 1, Vec3(0.2f, 0.6f, 1.0f));
        Renderer::drawRect(ex - 1, ey - 1, 1, eh + 2, Vec3(0.2f, 0.6f, 1.0f));
        Renderer::drawRect(ex - 1, ey + eh, ew + 2, 1, Vec3(0.2f, 0.6f, 1.0f));
        Renderer::drawRect(ex + ew, ey - 1, 1, eh + 2, Vec3(0.2f, 0.6f, 1.0f));

        float hw = 6;
        auto drawHandle = [&](float hx, float hy) {
            Renderer::drawRect(hx - hw/2, hy - hw/2, hw, hw, Vec3(1,1,1));
            Renderer::drawRect(hx - hw/2 + 1, hy - hw/2 + 1, hw - 2, hw - 2, Vec3(0.2f, 0.6f, 1.0f));
        };
        drawHandle(ex, ey);
        drawHandle(ex + ew/2, ey);
        drawHandle(ex + ew, ey);
        drawHandle(ex + ew, ey + eh/2);
        drawHandle(ex + ew, ey + eh);
        drawHandle(ex + ew/2, ey + eh);
        drawHandle(ex, ey + eh);
        drawHandle(ex, ey + eh/2);
    }

    void LabHUDEditor2D::renderAlignmentGuides(float cx, float cy, float zoom) {
        float resW = _project.resolutionW * zoom;
        float resH = _project.resolutionH * zoom;
        Renderer::drawRect(cx, cy, resW, 1, Vec3(0.0f, 1.0f, 1.0f));
        Renderer::drawRect(cx, cy, 1, resH, Vec3(0.0f, 1.0f, 1.0f));
        Renderer::drawRect(cx, cy + resH, resW, 1, Vec3(0.0f, 1.0f, 1.0f));
        Renderer::drawRect(cx + resW, cy, 1, resH, Vec3(0.0f, 1.0f, 1.0f));
    }

    // =========================================================================
    // Core logic
    // =========================================================================

    float LabHUDEditor2D::screenToCanvasX(float sx) const { return (sx - _viewportX - _canvasPanX) / _canvasZoom; }
    float LabHUDEditor2D::screenToCanvasY(float sy) const { return (sy - _viewportY - _canvasPanY) / _canvasZoom; }
    float LabHUDEditor2D::canvasToScreenX(float cx) const { return _viewportX + _canvasPanX + cx * _canvasZoom; }
    float LabHUDEditor2D::canvasToScreenY(float cy) const { return _viewportY + _canvasPanY + cy * _canvasZoom; }
    float LabHUDEditor2D::snapToGrid(float v) const { return std::round(v / _gridSnap) * _gridSnap; }

    Vec2 LabHUDEditor2D::resolveAnchor(const HUDElement& elem) const {
        return Vec2(elem.x, elem.y); // simplified
    }

    int LabHUDEditor2D::hitTestElement(float cx, float cy) const {
        for (int i = (int)_project.rootElements.size() - 1; i >= 0; --i) {
            const auto& e = _project.rootElements[i];
            if (cx >= e.x && cx <= e.x + e.w && cy >= e.y && cy <= e.y + e.h) {
                return i;
            }
        }
        return -1;
    }

    int LabHUDEditor2D::hitTestResizeHandle(float cx, float cy, int elemIndex) const {
        // mock returning corner/edge index based on distance to handle
        return -1;
    }

    void LabHUDEditor2D::deleteSelectedElements() {
        if (_selectedIndices.empty()) return;
        pushUndoState();
        std::vector<HUDElement> newElems;
        for (int i = 0; i < (int)_project.rootElements.size(); ++i) {
            if (std::find(_selectedIndices.begin(), _selectedIndices.end(), i) == _selectedIndices.end()) {
                newElems.push_back(_project.rootElements[i]);
            }
        }
        _project.rootElements = newElems;
        _selectedIndices.clear();
    }

    void LabHUDEditor2D::duplicateSelectedElements() {
        if (_selectedIndices.empty()) return;
        pushUndoState();
        std::vector<int> newSel;
        for (int idx : _selectedIndices) {
            HUDElement clone = _project.rootElements[idx];
            clone.x += 20; clone.y += 20;
            _project.rootElements.push_back(clone);
            newSel.push_back((int)_project.rootElements.size() - 1);
        }
        _selectedIndices = newSel;
    }

    void LabHUDEditor2D::moveSelectedZOrder(int delta) {
        if (_selectedIndices.empty()) return;
        for (int idx : _selectedIndices) {
            _project.rootElements[idx].zOrder += delta;
        }
        sortByZOrder();
    }

    void LabHUDEditor2D::sortByZOrder() {
        std::stable_sort(_project.rootElements.begin(), _project.rootElements.end(), [](const HUDElement& a, const HUDElement& b) {
            return a.zOrder < b.zOrder;
        });
    }

    int LabHUDEditor2D::generateUniqueId() {
        static int id = 0;
        return ++id;
    }

    HUDElement LabHUDEditor2D::createFromTemplate(int templateIndex, float x, float y) const {
        HUDElement elem;
        const auto& t = _widgetTemplates[templateIndex];
        elem.id = t.name + "_" + std::to_string(const_cast<LabHUDEditor2D*>(this)->generateUniqueId());
        elem.type = t.type;
        elem.x = x; elem.y = y;
        elem.w = t.defaultW; elem.h = t.defaultH;
        elem.color = t.defaultColor; elem.alpha = t.defaultAlpha;
        elem.zOrder = 0;
        if (elem.type == HUDElementType::Label || elem.type == HUDElementType::Button) {
            elem.text = t.name;
        }
        return elem;
    }

    void LabHUDEditor2D::pushUndoState() {
        EditorSnapshot snap;
        snap.elements = _project.rootElements;
        _undoStack.push_back(snap);
        if (_undoStack.size() > 50) _undoStack.erase(_undoStack.begin());
        _redoStack.clear();
        _projectDirty = true;
    }

    void LabHUDEditor2D::undo() {
        if (_undoStack.empty()) return;
        EditorSnapshot cur; cur.elements = _project.rootElements;
        _redoStack.push_back(cur);
        _project.rootElements = _undoStack.back().elements;
        _undoStack.pop_back();
        _selectedIndices.clear();
    }

    void LabHUDEditor2D::redo() {
        if (_redoStack.empty()) return;
        EditorSnapshot cur; cur.elements = _project.rootElements;
        _undoStack.push_back(cur);
        _project.rootElements = _redoStack.back().elements;
        _redoStack.pop_back();
        _selectedIndices.clear();
    }

    void LabHUDEditor2D::handleKeyDown(int key, bool ctrl, bool shift) {
        if (key == 256) { // Escape
            if (_assetBrowserOpen) _assetBrowserOpen = false;
            else if (_activeDropdown != HUDEditorDropdown::None) _activeDropdown = HUDEditorDropdown::None;
            else _selectedIndices.clear();
        } else if (ctrl && key == 83) { // Ctrl+S
            _project.saveToFile(_projectFilePath);
            _projectDirty = false;
        } else if (ctrl && key == 90) { // Ctrl+Z
            undo();
        } else if (ctrl && key == 89) { // Ctrl+Y
            redo();
        } else if (key == 261) { // Delete
            deleteSelectedElements();
        } else if (ctrl && key == 68) { // Ctrl+D
            duplicateSelectedElements();
        } else if (ctrl && key == 65) { // Ctrl+A
            _selectedIndices.clear();
            for(int i=0; i<(int)_project.rootElements.size(); ++i) _selectedIndices.push_back(i);
        } else if (key == 71) { // G
            _showGrid = !_showGrid;
        } else if (key == 72) { // H
            _showGuides = !_showGuides;
        } else if (key == 265) { // Up
            if (ctrl) moveSelectedZOrder(1);
            else for (int idx : _selectedIndices) _project.rootElements[idx].y -= _gridSnap > 0 ? _gridSnap : 1;
        } else if (key == 264) { // Down
            if (ctrl) moveSelectedZOrder(-1);
            else for (int idx : _selectedIndices) _project.rootElements[idx].y += _gridSnap > 0 ? _gridSnap : 1;
        } else if (key == 263) { // Left
            for (int idx : _selectedIndices) _project.rootElements[idx].x -= _gridSnap > 0 ? _gridSnap : 1;
        } else if (key == 262) { // Right
            for (int idx : _selectedIndices) _project.rootElements[idx].x += _gridSnap > 0 ? _gridSnap : 1;
        }
    }

    void LabHUDEditor2D::scanAssets() {
        if (std::filesystem::exists("assets/hud_assets/")) {
            for (const auto& entry : std::filesystem::directory_iterator("assets/hud_assets/")) {
                if (entry.path().extension() == ".bmp") {
                    _assetFiles.push_back(entry.path().filename().string());
                }
            }
        }
    }

    void LabHUDEditor2D::scanProjects() {
        if (std::filesystem::exists("assets/hud_projects/")) {
            for (const auto& entry : std::filesystem::directory_iterator("assets/hud_projects/")) {
                if (entry.path().extension() == ".labhud") {
                    _discoveredProjects.push_back(entry.path().string());
                }
            }
        }
    }

    Texture* LabHUDEditor2D::getAssetTexture(const std::string& path) {
        if (path.empty()) return nullptr;
        if (_assetTextures.find(path) == _assetTextures.end()) {
            std::string fullPath = resolveHUDPath(path);
            if (!fullPath.empty()) {
                _assetTextures[path] = std::make_unique<Texture>(fullPath);
            } else {
                return nullptr;
            }
        }
        return _assetTextures[path].get();
    }

    std::string LabHUDEditor2D::resolveHUDPath(const std::string& path) {
        if (std::filesystem::exists(path)) return path;
        if (std::filesystem::exists("assets/hud_assets/" + path)) return "assets/hud_assets/" + path;
        return "";
    }

    void LabHUDEditor2D::log(const std::string& msg) {
        _consoleLogs.push_back(msg);
        LabLog::info(msg);
    }

} // namespace Lab
