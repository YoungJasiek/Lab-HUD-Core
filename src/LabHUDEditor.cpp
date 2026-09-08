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
        out << ind2 << "lua_update \"" << elem.luaOnUpdate << "\"\n";
        out << ind2 << "lua_click \"" << elem.luaOnClick << "\"\n";
        out << ind2 << "lua_custom \"" << elem.luaCustom << "\"\n";
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
            out << "    asset \"" << asset << "\"\n";
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
                std::string rem; std::getline(lss, rem); trimWhitespace(rem); elem.text = stripQuotes(rem);
            } else if (prop == "font") {
                lss >> elem.fontSize >> elem.fontType;
            } else if (prop == "border") {
                lss >> elem.borderColor.x >> elem.borderColor.y >> elem.borderColor.z >> elem.borderWidth >> elem.borderAlpha;
            } else if (prop == "texture") {
                std::string rem; std::getline(lss, rem); trimWhitespace(rem); elem.texturePath = stripQuotes(rem);
            } else if (prop == "binding") {
                std::string rem; std::getline(lss, rem); trimWhitespace(rem); elem.binding = stripQuotes(rem);
            } else if (prop == "lua_update") {
                std::string rem; std::getline(lss, rem); trimWhitespace(rem); elem.luaOnUpdate = stripQuotes(rem);
            } else if (prop == "lua_click") {
                std::string rem; std::getline(lss, rem); trimWhitespace(rem); elem.luaOnClick = stripQuotes(rem);
            } else if (prop == "lua_custom") {
                std::string rem; std::getline(lss, rem); trimWhitespace(rem); elem.luaCustom = stripQuotes(rem);
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
                    ss >> key;
                    if (key == "asset") {
                        std::string rem; std::getline(ss, rem); trimWhitespace(rem);
                        proj->assets.push_back(stripQuotes(rem));
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
}

namespace Lab {
    
    // =========================================================================
    // Coordinate & Hit Test System
    // =========================================================================

    Vec2 LabHUDEditor2D::getElementAbsPos(const ElementRef& ref) const {
        if (!ref.isValid()) return {0.0f,0.0f};
        const HUDElement& root = _project.rootElements[ref.rootIndex];
        Vec2 pos = {root.x, root.y};
        
        float sw = _project.resolutionW;
        float sh = _project.resolutionH;
        switch(root.anchor) {
            case HUDAnchor::TopLeft: break;
            case HUDAnchor::TopCenter: pos.x += sw/2; break;
            case HUDAnchor::TopRight: pos.x += sw; break;
            case HUDAnchor::CenterLeft: pos.y += sh/2; break;
            case HUDAnchor::Center: pos.x += sw/2; pos.y += sh/2; break;
            case HUDAnchor::CenterRight: pos.x += sw; pos.y += sh/2; break;
            case HUDAnchor::BottomLeft: pos.y += sh; break;
            case HUDAnchor::BottomCenter: pos.x += sw/2; pos.y += sh; break;
            case HUDAnchor::BottomRight: pos.x += sw; pos.y += sh; break;
        }
        if (ref.isChild()) {
            const HUDElement& child = root.children[ref.childIndex];
            pos.x += child.x;
            pos.y += child.y;
        }
        return pos;
    }

    void LabHUDEditor2D::setElementAbsPos(const ElementRef& ref, const Vec2& targetAbs) {
        if (!ref.isValid()) return;
        HUDElement& root = _project.rootElements[ref.rootIndex];
        if (ref.isRoot()) {
            Vec2 anchorPos = {0.0f,0.0f};
            float sw = _project.resolutionW;
            float sh = _project.resolutionH;
            switch(root.anchor) {
                case HUDAnchor::TopLeft: break;
                case HUDAnchor::TopCenter: anchorPos.x += sw/2; break;
                case HUDAnchor::TopRight: anchorPos.x += sw; break;
                case HUDAnchor::CenterLeft: anchorPos.y += sh/2; break;
                case HUDAnchor::Center: anchorPos.x += sw/2; anchorPos.y += sh/2; break;
                case HUDAnchor::CenterRight: anchorPos.x += sw; anchorPos.y += sh/2; break;
                case HUDAnchor::BottomLeft: anchorPos.y += sh; break;
                case HUDAnchor::BottomCenter: anchorPos.x += sw/2; anchorPos.y += sh; break;
                case HUDAnchor::BottomRight: anchorPos.x += sw; anchorPos.y += sh; break;
            }
            root.x = targetAbs.x - anchorPos.x;
            root.y = targetAbs.y - anchorPos.y;
        } else {
            HUDElement& child = root.children[ref.childIndex];
            Vec2 rootAbs = getElementAbsPos({ref.rootIndex, -1});
            child.x = targetAbs.x - rootAbs.x;
            child.y = targetAbs.y - rootAbs.y;
        }
    }
    
    ElementRef LabHUDEditor2D::hitTest(float cx, float cy) const {
        for (int i = (int)_project.rootElements.size() - 1; i >= 0; --i) {
            const auto& root = _project.rootElements[i];
            if (!root.visible) continue;
            for (int j = (int)root.children.size() - 1; j >= 0; --j) {
                if (!root.children[j].visible) continue;
                Vec2 absP = getElementAbsPos({i, j});
                if (cx >= absP.x && cx <= absP.x + root.children[j].w &&
                    cy >= absP.y && cy <= absP.y + root.children[j].h) {
                    return {i, j};
                }
            }
            Vec2 absP = getElementAbsPos({i, -1});
            if (cx >= absP.x && cx <= absP.x + root.w &&
                cy >= absP.y && cy <= absP.y + root.h) {
                return {i, -1};
            }
        }
        return {-1, -1};
    }

    int LabHUDEditor2D::hitTestResizeHandle(float cx, float cy, const ElementRef& ref) const {
        // Mock resize hit test
        return -1;
    }
    
    Vec2 LabHUDEditor2D::getElementSize(const ElementRef& ref) const {
        if (ref.isRoot()) return {_project.rootElements[ref.rootIndex].w, _project.rootElements[ref.rootIndex].h};
        return {_project.rootElements[ref.rootIndex].children[ref.childIndex].w, _project.rootElements[ref.rootIndex].children[ref.childIndex].h};
    }

    HUDElement* LabHUDEditor2D::getElement(const ElementRef& ref) {
        if (!ref.isValid()) return nullptr;
        if (ref.isRoot()) return &_project.rootElements[ref.rootIndex];
        return &_project.rootElements[ref.rootIndex].children[ref.childIndex];
    }

    const HUDElement* LabHUDEditor2D::getElement(const ElementRef& ref) const {
        if (!ref.isValid()) return nullptr;
        if (ref.isRoot()) return &_project.rootElements[ref.rootIndex];
        return &_project.rootElements[ref.rootIndex].children[ref.childIndex];
    }
    
}

namespace Lab {

    // =========================================================================
    // LabHUDEditor2D
    // =========================================================================

    LabHUDEditor2D::LabHUDEditor2D() { initWidgetTemplates(); }
    LabHUDEditor2D::~LabHUDEditor2D() { shutdown(); }

    void LabHUDEditor2D::init() {
        scanProjects();
        scanAssets();
        log("HUDEditor2D initialized.");
    }

    void LabHUDEditor2D::shutdown() { _assetTextures.clear(); }

    void LabHUDEditor2D::log(const std::string& msg) { _consoleLogs.push_back(msg); }
    void LabHUDEditor2D::pushUndoState() {}
    void LabHUDEditor2D::undo() {}
    void LabHUDEditor2D::redo() {}
    
    void LabHUDEditor2D::deleteSelected() {
        if (!_selectedRefs.empty()) {
            auto ref = _selectedRefs[0];
            if (ref.isRoot()) {
                _project.rootElements.erase(_project.rootElements.begin() + ref.rootIndex);
            } else {
                _project.rootElements[ref.rootIndex].children.erase(_project.rootElements[ref.rootIndex].children.begin() + ref.childIndex);
            }
            _selectedRefs.clear();
        }
    }
    
    void LabHUDEditor2D::duplicateSelected() {
        if (!_selectedRefs.empty()) {
            auto ref = _selectedRefs[0];
            if (ref.isRoot()) {
                auto copy = _project.rootElements[ref.rootIndex];
                copy.x += 20; copy.y += 20;
                _project.rootElements.push_back(copy);
                _selectedRefs = {{(int)_project.rootElements.size()-1, -1}};
            }
        }
    }
    
    float LabHUDEditor2D::screenToCanvasX(float sx) const { return (sx - _canvasPanX) / _canvasZoom; }
    float LabHUDEditor2D::screenToCanvasY(float sy) const { return (sy - _canvasPanY) / _canvasZoom; }
    float LabHUDEditor2D::canvasToScreenX(float cx) const { return cx * _canvasZoom + _canvasPanX; }
    float LabHUDEditor2D::canvasToScreenY(float cy) const { return cy * _canvasZoom + _canvasPanY; }
    float LabHUDEditor2D::snapToGrid(float v) const { return _gridSnap > 0.0f ? std::round(v / _gridSnap) * _gridSnap : v; }

    void LabHUDEditor2D::update(float dt, float mouseX, float mouseY, bool lmbPressed, bool rmbPressed, float scrollDelta) {
        _lastMouseX = _mouseX; _lastMouseY = _mouseY;
        _mouseX = mouseX; _mouseY = mouseY;
        _lastLmb = _lmbPressed; _lastRmb = _rmbPressed;
        _lmbPressed = lmbPressed; _rmbPressed = rmbPressed;
        _lmbClicked = _lmbPressed && !_lastLmb;
        _rmbClicked = _rmbPressed && !_lastRmb;

        if (_mode == HUDEditorMode::ProjectSelect) {
            updateProjectSelect(dt);
            return;
        }
        
        if (_modalType != ModalType::None) {
            return; // Modal blocks interaction
        }
        
        if (_contextMenuOpen) {
            if (_lmbClicked && (_mouseX < _contextMenuX || _mouseX > _contextMenuX + 200 || _mouseY < _contextMenuY || _mouseY > _contextMenuY + _contextMenuItems.size()*25)) {
                _contextMenuOpen = false;
            }
            return;
        }

        if (_rmbClicked) {
            _rmbPressStartX = _mouseX;
            _rmbPressStartY = _mouseY;
            _panStartX = _mouseX;
            _panStartY = _mouseY;
            _panStartCanvasX = _canvasPanX;
            _panStartCanvasY = _canvasPanY;
            _isPanning = true;
        } else if (!_rmbPressed && _lastRmb) {
            _isPanning = false;
            float dist = std::sqrt(std::pow(_mouseX - _rmbPressStartX, 2) + std::pow(_mouseY - _rmbPressStartY, 2));
            if (dist < 6.0f) {
                float cx = screenToCanvasX(_mouseX);
                float cy = screenToCanvasY(_mouseY);
                ElementRef hit = hitTest(cx, cy);
                openContextMenu(_mouseX, _mouseY, hit.isValid(), hit);
            }
        }
        
        if (_isPanning) {
            _canvasPanX = _panStartCanvasX + (_mouseX - _panStartX);
            _canvasPanY = _panStartCanvasY + (_mouseY - _panStartY);
        }

        if (_mouseX > _viewportX && _mouseX < _viewportX + _viewportW && _mouseY > _viewportY && _mouseY < _viewportY + _viewportH) {
            if (scrollDelta != 0.0f) {
                float oldCX = screenToCanvasX(_mouseX);
                float oldCY = screenToCanvasY(_mouseY);
                _canvasZoom = std::clamp(_canvasZoom * (1.0f + scrollDelta * 0.1f), 0.1f, 10.0f);
                _canvasPanX += (screenToCanvasX(_mouseX) - oldCX) * _canvasZoom;
                _canvasPanY += (screenToCanvasY(_mouseY) - oldCY) * _canvasZoom;
            }

            float cx = screenToCanvasX(_mouseX);
            float cy = screenToCanvasY(_mouseY);

            if (_lmbClicked && !_isPanning) {
                ElementRef hit = hitTest(cx, cy);
                if (hit.isValid()) {
                    _selectedRefs.clear();
                    _selectedRefs.push_back(hit);
                    _isDragging = true;
                    _dragStartMouseCanvasX = cx;
                    _dragStartMouseCanvasY = cy;
                    _dragStartElemAbsPos = getElementAbsPos(hit);
                } else {
                    _selectedRefs.clear();
                }
            } else if (_lmbPressed && _isDragging && !_selectedRefs.empty()) {
                Vec2 currentMouseCanvas = {cx, cy};
                Vec2 targetAbs = {
                    _dragStartElemAbsPos.x + (currentMouseCanvas.x - _dragStartMouseCanvasX),
                    _dragStartElemAbsPos.y + (currentMouseCanvas.y - _dragStartMouseCanvasY)
                };
                if (_gridSnap > 0) {
                    targetAbs.x = snapToGrid(targetAbs.x);
                    targetAbs.y = snapToGrid(targetAbs.y);
                }
                setElementAbsPos(_selectedRefs[0], targetAbs);
            } else {
                _isDragging = false;
            }
        }
    }

    void LabHUDEditor2D::handleKeyDown(int key, bool ctrl, bool shift) {
        if (_modalType != ModalType::None) {
            if (key == GLFW_KEY_ESCAPE) {
                closeModal();
            } else if (key == GLFW_KEY_ENTER || key == GLFW_KEY_KP_ENTER) {
                if (_modalOnConfirm) _modalOnConfirm(_modalBuffer);
                closeModal();
            } else if (key == GLFW_KEY_BACKSPACE && !_modalBuffer.empty()) {
                _modalBuffer.pop_back();
            } else if (key >= 32 && key <= 126) {
                _modalBuffer += (char)key;
            }
        }
    }

}

namespace Lab {

    // =========================================================================
    // Rendering
    // =========================================================================

    void LabHUDEditor2D::renderHUDElementRecursive(const HUDElement& elem, const Vec2& parentAbsPos, float vx, float vy, const ElementRef& ref) {
        if (!elem.visible) return;
        
        float sx = canvasToScreenX(parentAbsPos.x);
        float sy = canvasToScreenY(parentAbsPos.y);
        float sw = elem.w * _canvasZoom;
        float sh = elem.h * _canvasZoom;

        Renderer::drawRect(sx, sy, sw, sh, elem.color);
        
        if (elem.type == HUDElementType::Label || elem.type == HUDElementType::Button) {
            float fontScale = elem.fontSize * _canvasZoom;
            float th = LabFont::getTextHeight(fontScale, (LabFontType)elem.fontType);
            float ty = sy + std::max(0.0f, (sh - th) * 0.5f); // Centered vertically
            float tw = LabFont::getTextWidth(elem.text, fontScale, (LabFontType)elem.fontType);
            float tx = sx + std::max(0.0f, (sw - tw) * 0.5f);
            LabFont::drawText(tx, ty, elem.text, fontScale, Vec3(1,1,1), (LabFontType)elem.fontType);
        }

        for (int i = 0; i < (int)elem.children.size(); ++i) {
            Vec2 childAbs = {parentAbsPos.x + elem.children[i].x, parentAbsPos.y + elem.children[i].y};
            renderHUDElementRecursive(elem.children[i], childAbs, vx, vy, {ref.rootIndex, i});
        }
    }

    void LabHUDEditor2D::render(int screenWidth, int screenHeight) {
        _screenWidth = screenWidth; _screenHeight = screenHeight;
        Renderer::beginUI(screenWidth, screenHeight);
        
        if (_mode == HUDEditorMode::ProjectSelect) {
            renderProjectSelect(screenWidth, screenHeight);
        } else {
            renderEditorUI(screenWidth, screenHeight);
        }

        if (_modalType != ModalType::None) renderModalDialog();
        if (_contextMenuOpen) renderContextMenu();
        
        Renderer::endUI();
    }
    
    void LabHUDEditor2D::renderEditorUI(int w, int h) {
        Renderer::drawRect(0, 0, w, h, Vec3(0.12f, 0.12f, 0.12f));
        _viewportX = 260; _viewportY = 76;
        _viewportW = w - 560; _viewportH = h - 76 - 28;

        // Render Canvas
        Renderer::drawRect(_viewportX, _viewportY, _viewportW, _viewportH, Vec3(0.08f, 0.08f, 0.08f));
        
        for (int i = 0; i < (int)_project.rootElements.size(); ++i) {
            Vec2 absP = getElementAbsPos({i, -1});
            renderHUDElementRecursive(_project.rootElements[i], absP, _viewportX, _viewportY, {i, -1});
        }

        renderLeftSidebar(0, 32, 260, h - 32 - 28);
        renderRightPropertyPanel(w - 300, 32, 300, h - 32 - 28);
    }

    void LabHUDEditor2D::renderModalDialog() {
        Renderer::drawRect(0, 0, _screenWidth, _screenHeight, Vec3(0,0,0)); // Dim overlay, alpha would be nice
        float mw = 400, mh = 200;
        float mx = _screenWidth/2 - mw/2;
        float my = _screenHeight/2 - mh/2;
        
        drawHammerPanel(mx, my, mw, mh, _modalTitle);
        LabFont::drawText(mx + 20, my + 40, _modalPrompt, 1.2f, Vec3(1,1,1), LabFontType::System);
        
        Renderer::drawRect(mx + 20, my + 80, mw - 40, 30, Vec3(0.1f, 0.1f, 0.1f));
        drawHammerBevel(mx + 20, my + 80, mw - 40, 30, true);
        LabFont::drawText(mx + 25, my + 85, _modalBuffer + "_", 1.2f, Vec3(0.98f,0.78f,0.08f), LabFontType::System);
        
        if (drawHammerButton(mx + 20, my + 140, 100, 30, "Cancel")) {
            closeModal();
        }
        if (drawHammerButton(mx + mw - 120, my + 140, 100, 30, "Confirm")) {
            if (_modalOnConfirm) _modalOnConfirm(_modalBuffer);
            closeModal();
        }
    }
    
    void LabHUDEditor2D::renderContextMenu() {
        float mw = 200;
        float mh = _contextMenuItems.size() * 25.0f;
        Renderer::drawRect(_contextMenuX, _contextMenuY, mw, mh, Vec3(0.18f, 0.18f, 0.18f));
        drawHammerBevel(_contextMenuX, _contextMenuY, mw, mh, false);
        
        float py = _contextMenuY;
        for (const auto& item : _contextMenuItems) {
            bool hover = _mouseX >= _contextMenuX && _mouseX <= _contextMenuX + mw && _mouseY >= py && _mouseY < py + 25;
            if (drawHammerDropdownItem(_contextMenuX, py, mw, 25, item.label, hover)) {
                if (item.action) item.action();
                _contextMenuOpen = false;
            }
            py += 25;
        }
    }

    void LabHUDEditor2D::openModal(ModalType type, const std::string& title, const std::string& prompt, const std::string& initialVal, std::function<void(const std::string&)> onConfirm) {
        _modalType = type;
        _modalTitle = title;
        _modalPrompt = prompt;
        _modalBuffer = initialVal;
        _modalOnConfirm = onConfirm;
    }
    void LabHUDEditor2D::closeModal() { _modalType = ModalType::None; }
    
    void LabHUDEditor2D::openContextMenu(float sx, float sy, bool onElement, const ElementRef& targetRef) {
        _contextMenuOpen = true;
        _contextMenuX = sx; _contextMenuY = sy;
        _contextMenuItems.clear();
        
        if (onElement) {
            _contextMenuItems.push_back({"Cut", nullptr});
            _contextMenuItems.push_back({"Copy", nullptr});
            _contextMenuItems.push_back({"Duplicate", [this](){ duplicateSelected(); }});
            _contextMenuItems.push_back({"Delete", [this](){ deleteSelected(); }});
            _contextMenuItems.push_back({"-", nullptr});
            _contextMenuItems.push_back({"Edit Text...", [this, targetRef](){
                if (auto e = getElement(targetRef)) {
                    openModal(ModalType::EditText, "Edit Text", "Enter new text:", e->text, [this, targetRef](const std::string& v){
                        if (auto el = getElement(targetRef)) el->text = v;
                    });
                }
            }});
        } else {
            _contextMenuItems.push_back({"Paste", nullptr});
            _contextMenuItems.push_back({"Reset View", nullptr});
            _contextMenuItems.push_back({"Toggle Grid", nullptr});
        }
    }

}

namespace Lab {

    void LabHUDEditor2D::importCustomAsset() {
        std::string path = LabDialogs::openFileDialog(_window, "Image Files\0*.png;*.jpg;*.jpeg;*.tga\0All Files\0*.*\0", "");
        if (!path.empty()) {
            std::filesystem::path p(path);
            std::string filename = p.filename().string();
            std::string dest = "assets/hud_assets/" + filename;
            try {
                std::filesystem::create_directories("assets/hud_assets");
                std::filesystem::copy_file(path, dest, std::filesystem::copy_options::overwrite_existing);
                _assetFiles.push_back(dest);
                _project.assets.push_back(dest);
            } catch(...) {}
        }
    }

    void LabHUDEditor2D::renderLeftSidebar(float x, float y, float w, float h) {
        drawHammerPanel(x, y, w, h, "");
        
        float tabW = w / 3.0f;
        if (drawHammerButton(x, y, tabW, 30, "WIDGETS", _sidebarTab == HUDSidebarTab::Widgets)) _sidebarTab = HUDSidebarTab::Widgets;
        if (drawHammerButton(x + tabW, y, tabW, 30, "HIERARCHY", _sidebarTab == HUDSidebarTab::Hierarchy)) _sidebarTab = HUDSidebarTab::Hierarchy;
        if (drawHammerButton(x + tabW * 2, y, tabW, 30, "ASSETS", _sidebarTab == HUDSidebarTab::Assets)) _sidebarTab = HUDSidebarTab::Assets;
        
        float py = y + 35;
        if (_sidebarTab == HUDSidebarTab::Widgets) {
            for (const auto& tmpl : _widgetTemplates) {
                if (drawHammerButton(x + 10, py, w - 20, 30, tmpl.name)) {
                    HUDElement el;
                    el.type = tmpl.type; el.id = tmpl.name; el.w = tmpl.defaultW; el.h = tmpl.defaultH;
                    el.color = tmpl.defaultColor; el.alpha = tmpl.defaultAlpha; el.text = tmpl.defaultText;
                    _project.rootElements.push_back(el);
                    _selectedRefs = {{(int)_project.rootElements.size() - 1, -1}};
                }
                py += 35;
            }
        } else if (_sidebarTab == HUDSidebarTab::Hierarchy) {
            for (int i = 0; i < (int)_project.rootElements.size(); ++i) {
                bool sel = (!_selectedRefs.empty() && _selectedRefs[0].rootIndex == i && _selectedRefs[0].childIndex == -1);
                if (drawHammerButton(x + 10, py, w - 20, 25, _project.rootElements[i].id, sel)) {
                    _selectedRefs = {{i, -1}};
                }
                py += 28;
                for (int j = 0; j < (int)_project.rootElements[i].children.size(); ++j) {
                    bool csel = (!_selectedRefs.empty() && _selectedRefs[0].rootIndex == i && _selectedRefs[0].childIndex == j);
                    if (drawHammerButton(x + 30, py, w - 40, 25, _project.rootElements[i].children[j].id, csel)) {
                        _selectedRefs = {{i, j}};
                    }
                    py += 28;
                }
            }
            if (drawHammerButton(x + 10, h - 40, w - 20, 30, "+ Add Child")) {
                if (!_selectedRefs.empty() && _selectedRefs[0].isRoot()) {
                    HUDElement child; child.id = "Child"; child.w = 50; child.h = 50; child.type = HUDElementType::Rect;
                    _project.rootElements[_selectedRefs[0].rootIndex].children.push_back(child);
                }
            }
        } else if (_sidebarTab == HUDSidebarTab::Assets) {
            if (drawHammerButton(x + 10, py, w - 20, 40, "+ IMPORT ASSET FROM DISK...")) {
                importCustomAsset();
            }
            py += 50;
            for (const auto& a : _assetFiles) {
                if (drawHammerButton(x + 10, py, w - 20, 25, a)) {
                    if (!_selectedRefs.empty()) {
                        if (auto e = getElement(_selectedRefs[0])) e->texturePath = a;
                    }
                }
                py += 28;
            }
        }
    }

    void LabHUDEditor2D::renderRightPropertyPanel(float x, float y, float w, float h) {
        drawHammerPanel(x, y, w, h, "PROPERTIES");
        if (_selectedRefs.empty()) return;
        
        HUDElement* elem = getElement(_selectedRefs[0]);
        if (!elem) return;
        
        float py = y + 25;
        LabFont::drawText(x + 10, py, "ID: " + elem->id, 1.2f, Vec3(1,1,1), LabFontType::System); py += 25;
        
        drawHammerSlider(x + 10, py, w - 20, 20, "W", elem->w, 0, 2000, "%.0f"); py += 25;
        drawHammerSlider(x + 10, py, w - 20, 20, "H", elem->h, 0, 2000, "%.0f"); py += 25;
        
        if (elem->type == HUDElementType::Label || elem->type == HUDElementType::Button) {
            if (drawHammerButton(x + 10, py, w - 20, 30, "Edit Text...")) {
                openModal(ModalType::EditText, "Edit Text", "Enter text:", elem->text, [elem](const std::string& v){ elem->text = v; });
            }
            py += 35;
            
            LabFont::drawText(x + 10, py, "Font Type:", 1.2f, Vec3(1,1,1), LabFontType::System); py += 20;
            float fw = (w - 30) / 3.0f;
            if (drawHammerButton(x + 10, py, fw, 30, "GeoSans", elem->fontType == 0)) elem->fontType = 0;
            if (drawHammerButton(x + 10 + fw, py, fw, 30, "System", elem->fontType == 1)) elem->fontType = 1;
            if (drawHammerButton(x + 10 + fw*2, py, fw, 30, "DotMatrix", elem->fontType == 2)) elem->fontType = 2;
            py += 35;
        }

        py += 10;
        Renderer::drawRect(x + 5, py, w - 10, 1, Vec3(0.3f, 0.3f, 0.3f)); py += 10;
        LabFont::drawText(x + 10, py, "LUA SCRIPTING", 1.2f, Vec3(1, 0.5f, 0), LabFontType::System); py += 20;
        
        if (drawHammerButton(x + 10, py, w - 20, 30, "Edit Lua Script...")) {
            openModal(ModalType::EditLua, "Edit Lua", "Enter script:", elem->luaCustom, [elem](const std::string& v){ elem->luaCustom = v; });
        }
        py += 35;
        
        LabFont::drawText(x + 10, py, "Presets:", 1.2f, Vec3(0.8f,0.8f,0.8f), LabFontType::System); py += 20;
        if (drawHammerButton(x + 10, py, w - 20, 25, "[Player Health]")) {
            elem->binding = "Player:getHealth()";
            elem->luaOnUpdate = "elem.text = tostring(Player:getHealth()); if Player:getHealth() < 30 then elem.color = {1,0.2,0.2} end";
        } py += 28;
        if (drawHammerButton(x + 10, py, w - 20, 25, "[Player Armor]")) {
            elem->binding = "Player:getArmor()";
            elem->luaOnUpdate = "elem.text = Player:getArmor() .. '%'";
        } py += 28;
        if (drawHammerButton(x + 10, py, w - 20, 25, "[Weapon Ammo]")) {
            elem->binding = "Weapon:getAmmo()";
            elem->luaOnUpdate = "elem.text = Weapon:getClip() .. ' / ' .. Weapon:getReserve()";
        } py += 28;
        if (drawHammerButton(x + 10, py, w - 20, 25, "[Match Timer]")) {
            elem->binding = "Match:getTimer()";
            elem->luaOnUpdate = "elem.text = tostring(math.floor(Match:getTimer()))";
        } py += 28;
        if (drawHammerButton(x + 10, py, w - 20, 25, "[Respawn Button]")) {
            elem->luaOnClick = "Player:respawn(); HUD:hide()";
        } py += 28;
    }

}

namespace Lab {

    void LabHUDEditor2D::initWidgetTemplates() {
        _widgetTemplates = {
            {"Rect",        HUDElementType::Rect,        0, 100, 100, {0.8f, 0.8f, 0.8f}, 1.0f, ""},
            {"Label",       HUDElementType::Label,       1, 120,  30, {0.98f,0.78f,0.08f}, 1.0f, "Label"},
            {"Panel",       HUDElementType::Panel,       2, 200, 150, {0.18f,0.18f,0.18f}, 1.0f, ""},
            {"Card",        HUDElementType::Card,        3, 180,  60, {0.16f,0.17f,0.19f}, 0.85f, ""},
            {"HealthBar",   HUDElementType::HealthBar,   4, 250,  30, {1.0f, 0.2f, 0.2f}, 1.0f, ""},
            {"AmmoCounter", HUDElementType::AmmoCounter, 5, 100,  80, {1.0f, 0.8f, 0.2f}, 1.0f, ""},
            {"Crosshair",   HUDElementType::Crosshair,   6,  40,  40, {0.2f, 1.0f, 0.2f}, 1.0f, ""},
            {"Icon",        HUDElementType::Icon,        7,  32,  32, {1.0f, 1.0f, 1.0f}, 1.0f, ""},
            {"Image",       HUDElementType::Image,       8, 128, 128, {1.0f, 1.0f, 1.0f}, 1.0f, ""},
            {"ProgressBar", HUDElementType::ProgressBar, 9, 200,  20, {0.2f, 0.6f, 1.0f}, 1.0f, ""},
            {"Button",      HUDElementType::Button,     10, 150,  34, {0.3f, 0.3f, 0.3f}, 1.0f, "Button"}
        };
    }

    void LabHUDEditor2D::drawHammerBevel(float x, float y, float w, float h, bool sunken) {
        Vec3 hl = sunken ? Vec3(0.1f, 0.1f, 0.1f) : Vec3(0.4f, 0.4f, 0.4f);
        Vec3 sh = sunken ? Vec3(0.4f, 0.4f, 0.4f) : Vec3(0.1f, 0.1f, 0.1f);
        Renderer::drawRect(x, y, w, 1, hl);
        Renderer::drawRect(x, y, 1, h, hl);
        Renderer::drawRect(x, y + h - 1, w, 1, sh);
        Renderer::drawRect(x + w - 1, y, 1, h, sh);
    }

    void LabHUDEditor2D::drawHammerPanel(float x, float y, float w, float h, const std::string& title) {
        Renderer::drawRect(x, y, w, h, Vec3(0.18f, 0.18f, 0.18f));
        drawHammerBevel(x, y, w, h, false);
        if (!title.empty()) {
            Renderer::drawRect(x + 2, y + 2, w - 4, 18, Vec3(0.12f, 0.12f, 0.12f));
            LabFont::drawText(x + 5, y + 2, title, 1.9f, Vec3(0.98f, 0.78f, 0.08f), LabFontType::System);
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

        float tw = LabFont::getTextWidth(label, 1.5f, LabFontType::System);
        float th = LabFont::getTextHeight(1.5f, LabFontType::System);
        float ty = y + std::max(0.0f, (h - th) * 0.5f);
        LabFont::drawText(x + (w - tw) * 0.5f, ty, label, 1.5f, active ? Vec3(0,0,0) : Vec3(0.9f, 0.9f, 0.9f), LabFontType::System);

        return clicked;
    }

    bool LabHUDEditor2D::drawHammerSlider(float x, float y, float w, float h, const std::string& label, float& value, float minVal, float maxVal, const std::string& format) {
        bool changed = false;
        LabFont::drawText(x, y, label, 1.5f, Vec3(0.8f, 0.8f, 0.8f), LabFontType::System);
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
        LabFont::drawText(sx + 5, y, buf, 1.5f, Vec3(1,1,1), LabFontType::System);

        return changed;
    }

    bool LabHUDEditor2D::drawHammerDropdownItem(float x, float y, float w, float h, const std::string& label, bool hovered, bool separator, bool disabled) {
        if (separator || label == "-") {
            Renderer::drawRect(x + 5, y + h/2, w - 10, 1, Vec3(0.4f, 0.4f, 0.4f));
            return false;
        }
        if (hovered && !disabled) {
            Renderer::drawRect(x, y, w, h, Vec3(1.0f, 0.55f, 0.1f));
        }
        LabFont::drawText(x + 10, y + 2, label, 1.5f, disabled ? Vec3(0.5f,0.5f,0.5f) : (hovered ? Vec3(0,0,0) : Vec3(0.9f, 0.9f, 0.9f)), LabFontType::System);
        return hovered && _lmbClicked && !disabled;
    }
    
    void LabHUDEditor2D::renderGrid(float vx, float vy, float vw, float vh) {
        float scaledSnap = _gridSnap * _canvasZoom;
        if (scaledSnap < 6.0f) return;

        int startX = (int)(- _canvasPanX / scaledSnap) - 1;
        int endX = (int)((vw - _canvasPanX) / scaledSnap) + 1;
        for (int i = startX; i <= endX; ++i) {
            float gx = vx + _canvasPanX + static_cast<float>(i) * scaledSnap;
            if (gx >= vx && gx <= vx + vw) {
                bool major = (i % 5 == 0);
                Renderer::drawRect(gx, vy, 1.0f, vh, major ? Vec3(0.22f, 0.22f, 0.22f) : Vec3(0.12f, 0.12f, 0.12f));
            }
        }

        int startY = (int)(- _canvasPanY / scaledSnap) - 1;
        int endY = (int)((vh - _canvasPanY) / scaledSnap) + 1;
        for (int i = startY; i <= endY; ++i) {
            float gy = vy + _canvasPanY + static_cast<float>(i) * scaledSnap;
            if (gy >= vy && gy <= vy + vh) {
                bool major = (i % 5 == 0);
                Renderer::drawRect(vx, gy, vw, 1.0f, major ? Vec3(0.22f, 0.22f, 0.22f) : Vec3(0.12f, 0.12f, 0.12f));
            }
        }
    }

    void LabHUDEditor2D::renderAlignmentGuides(float vx, float vy) {
        float cx = vx + _canvasPanX;
        float cy = vy + _canvasPanY;
        float resW = _project.resolutionW * _canvasZoom;
        float resH = _project.resolutionH * _canvasZoom;
        Vec3 guideCol(0.1f, 0.7f, 0.9f);
        Renderer::drawRect(cx, cy, resW, 1.5f, guideCol);
        Renderer::drawRect(cx, cy, 1.5f, resH, guideCol);
        Renderer::drawRect(cx, cy + resH - 1.5f, resW, 1.5f, guideCol);
        Renderer::drawRect(cx + resW - 1.5f, cy, 1.5f, resH, guideCol);
    }

    void LabHUDEditor2D::renderSelectionOutlineAndHandles(const ElementRef& ref, float vx, float vy) {
        const HUDElement* elem = getElement(ref);
        if (!elem) return;
        Vec2 absP = getElementAbsPos(ref);
        float ex = vx + _canvasPanX + absP.x * _canvasZoom;
        float ey = vy + _canvasPanY + absP.y * _canvasZoom;
        float ew = elem->w * _canvasZoom;
        float eh = elem->h * _canvasZoom;

        Vec3 outlineCol(0.2f, 0.6f, 1.0f);
        Renderer::drawRect(ex - 1.0f, ey - 1.0f, ew + 2.0f, 1.5f, outlineCol);
        Renderer::drawRect(ex - 1.0f, ey - 1.0f, 1.5f, eh + 2.0f, outlineCol);
        Renderer::drawRect(ex - 1.0f, ey + eh - 0.5f, ew + 2.0f, 1.5f, outlineCol);
        Renderer::drawRect(ex + ew - 0.5f, ey - 1.0f, 1.5f, eh + 2.0f, outlineCol);

        float hw = 7.0f;
        auto drawHandle = [&](float hx, float hy) {
            Renderer::drawRect(hx - hw * 0.5f, hy - hw * 0.5f, hw, hw, Vec3(1.0f, 1.0f, 1.0f));
            Renderer::drawRect(hx - hw * 0.5f + 1.0f, hy - hw * 0.5f + 1.0f, hw - 2.0f, hw - 2.0f, outlineCol);
        };
        drawHandle(ex, ey);
        drawHandle(ex + ew * 0.5f, ey);
        drawHandle(ex + ew, ey);
        drawHandle(ex + ew, ey + eh * 0.5f);
        drawHandle(ex + ew, ey + eh);
        drawHandle(ex + ew * 0.5f, ey + eh);
        drawHandle(ex, ey + eh);
        drawHandle(ex, ey + eh * 0.5f);
    }

    HUDElement* LabHUDEditor2D::getParentElement(const ElementRef& ref) {
        if (!ref.isChild() || ref.rootIndex < 0 || ref.rootIndex >= (int)_project.rootElements.size()) return nullptr;
        return &_project.rootElements[ref.rootIndex];
    }

    void LabHUDEditor2D::moveZOrder(int delta) {
        if (auto el = getElement(_selectedRef)) {
            pushUndoState();
            el->zOrder += delta;
            sortRootByZOrder();
            _projectDirty = true;
        }
    }

    void LabHUDEditor2D::sortRootByZOrder() {
        std::stable_sort(_project.rootElements.begin(), _project.rootElements.end(), [](const HUDElement& a, const HUDElement& b) {
            return a.zOrder < b.zOrder;
        });
    }

    void LabHUDEditor2D::selectElement(const ElementRef& ref, bool addToSelection) {
        if (!addToSelection) _selectedRefs.clear();
        _selectedRef = ref;
        if (ref.isValid() && std::find(_selectedRefs.begin(), _selectedRefs.end(), ref) == _selectedRefs.end()) {
            _selectedRefs.push_back(ref);
        }
    }

    void LabHUDEditor2D::clearSelection() {
        _selectedRef.invalidate();
        _selectedRefs.clear();
    }

    bool LabHUDEditor2D::isSelected(const ElementRef& ref) const {
        if (!ref.isValid()) return false;
        return std::find(_selectedRefs.begin(), _selectedRefs.end(), ref) != _selectedRefs.end() || _selectedRef == ref;
    }

    Texture* LabHUDEditor2D::getAssetTexture(const std::string& path) {
        if (path.empty()) return nullptr;
        auto it = _assetTextures.find(path);
        if (it != _assetTextures.end()) return it->second.get();
        if (std::filesystem::exists(path)) {
            try {
                auto tex = std::make_unique<Texture>(path);
                Texture* ptr = tex.get();
                _assetTextures[path] = std::move(tex);
                return ptr;
            } catch (...) {}
        }
        return nullptr;
    }

    HUDElement LabHUDEditor2D::createFromTemplate(int templateIndex, float absX, float absY) const {
        if (templateIndex < 0 || templateIndex >= (int)_widgetTemplates.size()) return {};
        const auto& t = _widgetTemplates[templateIndex];
        HUDElement el;
        el.id = t.name + "_" + std::to_string(rand() % 900 + 100);
        el.type = t.type;
        el.x = absX;
        el.y = absY;
        el.w = t.defaultW;
        el.h = t.defaultH;
        el.color = t.defaultColor;
        el.alpha = t.defaultAlpha;
        el.text = t.defaultText;
        el.anchor = HUDAnchor::TopLeft;
        el.fontSize = 2.0f;
        el.fontType = 0;
        return el;
    }

    std::string LabHUDEditor2D::resolveHUDPath(const std::string& path) {
        if (std::filesystem::exists(path)) return path;
        std::string fname = std::filesystem::path(path).filename().string();
        if (std::filesystem::exists("assets/hud_projects/" + fname)) return "assets/hud_projects/" + fname;
        if (std::filesystem::exists("../assets/hud_projects/" + fname)) return "../assets/hud_projects/" + fname;
        if (std::filesystem::exists("../../assets/hud_projects/" + fname)) return "../../assets/hud_projects/" + fname;
        return path;
    }

    void LabHUDEditor2D::scanProjects() {
        _discoveredProjects.clear();
        std::vector<std::string> dirs = {"assets/hud_projects", "../assets/hud_projects", "../../assets/hud_projects"};
        for (const auto& d : dirs) {
            if (std::filesystem::exists(d)) {
                for (const auto& entry : std::filesystem::directory_iterator(d)) {
                    if (entry.path().extension() == ".labhud") {
                        _discoveredProjects.push_back(entry.path().string());
                    }
                }
            }
        }
    }

    void LabHUDEditor2D::scanAssets() {
        _assetFiles.clear();
        std::vector<std::string> dirs = {"assets/hud_assets", "../assets/hud_assets", "../../assets/hud_assets"};
        for (const auto& d : dirs) {
            if (std::filesystem::exists(d)) {
                for (const auto& entry : std::filesystem::directory_iterator(d)) {
                    auto ext = entry.path().extension().string();
                    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                    if (ext == ".bmp" || ext == ".png" || ext == ".jpg" || ext == ".tga") {
                        _assetFiles.push_back(entry.path().string());
                    }
                }
            }
        }
    }

    void LabHUDEditor2D::renderProjectSelect(int w, int h) {
        float fw = static_cast<float>(w);
        float fh = static_cast<float>(h);
        Renderer::drawRect(0, 0, fw, fh, Vec3(0.11f, 0.12f, 0.14f));

        float bannerW = 720.0f;
        float bannerH = 500.0f;
        float bx = fw * 0.5f - bannerW * 0.5f;
        float by = fh * 0.5f - bannerH * 0.5f;

        drawHammerPanel(bx, by, bannerW, bannerH, "LAB ENGINE 2026 - HUD / GUI WORKSPACE LAUNCHER");

        LabFont::drawText(bx + 40.0f, by + 40.0f, "PROJECT SELECTOR", 2.2f, Vec3(1.0f, 0.55f, 0.1f), LabFontType::System);
        LabFont::drawText(bx + 40.0f, by + 75.0f, "Choose an existing .labhud interface project or author a new layout:", 1.5f, Vec3(0.7f, 0.7f, 0.7f), LabFontType::System);

        float py = by + 110.0f;

        if (_discoveredProjects.empty()) {
            scanProjects();
        }

        for (int i = 0; i < (int)_discoveredProjects.size() && i < 4; ++i) {
            std::string pName = std::filesystem::path(_discoveredProjects[i]).stem().string();
            std::string btnText = "[PROJECT] " + pName + " (" + _discoveredProjects[i] + ")";
            if (drawHammerButton(bx + 40.0f, py, bannerW - 80.0f, 38.0f, btnText, false, true)) {
                auto loaded = HUDProject::loadFromFile(_discoveredProjects[i]);
                if (loaded) {
                    _project = *loaded;
                    _projectFilePath = _discoveredProjects[i];
                    _mode = HUDEditorMode::Editor;
                    _projectDirty = false;
                    scanAssets();
                }
            }
            py += 46.0f;
        }

        py = by + bannerH - 75.0f;
        if (drawHammerButton(bx + 40.0f, py, 190.0f, 40.0f, "+ NEW PROJECT")) {
            _project = HUDProject();
            _project.name = "New HUD Layout";
            _projectFilePath = "assets/hud_projects/new_hud.labhud";
            _mode = HUDEditorMode::Editor;
            _projectDirty = true;
            scanAssets();
        }
        if (drawHammerButton(bx + 265.0f, py, 190.0f, 40.0f, "OPEN FILE...")) {
            std::string chosen = LabDialogs::openFileDialog(_window, "Lab HUD Files (*.labhud)\0*.labhud\0All Files (*.*)\0*.*\0", "assets\\hud_projects");
            if (!chosen.empty()) {
                auto loaded = HUDProject::loadFromFile(chosen);
                if (loaded) {
                    _project = *loaded;
                    _projectFilePath = chosen;
                    _mode = HUDEditorMode::Editor;
                    _projectDirty = false;
                    scanAssets();
                }
            }
        }
        if (drawHammerButton(bx + 490.0f, py, 190.0f, 40.0f, "EXIT")) {
            _requestExit = true;
        }

        LabFont::drawText(20.0f, fh - 24.0f, "Lab 2D/HUD Editor | Architecture: LabStudio Hammer Theme | Lua 5.4 | YoungJasiek", 1.4f, Vec3(0.5f, 0.5f, 0.5f), LabFontType::System);
    }

    void LabHUDEditor2D::updateProjectSelect(float dt) {
        (void)dt;
    }

    void LabHUDEditor2D::renderTopMenuBar(float w) {
        Renderer::drawRect(0, 0, w, 32.0f, Vec3(0.18f, 0.18f, 0.18f));
        drawHammerBevel(0, 0, w, 32.0f, false);

        std::vector<std::string> menus = {"File", "Edit", "View", "Insert", "Assets", "Help"};
        float mx = 10.0f;
        for (int i = 0; i < (int)menus.size(); ++i) {
            bool active = (_activeDropdown == (HUDEditorDropdown)i);
            if (drawHammerButton(mx, 3.0f, 68.0f, 26.0f, menus[i], active)) {
                if (active) _activeDropdown = HUDEditorDropdown::None;
                else _activeDropdown = (HUDEditorDropdown)i;
            }
            mx += 72.0f;
        }
    }

    void LabHUDEditor2D::renderToolbar(float w) {
        Renderer::drawRect(0, 32.0f, w, 40.0f, Vec3(0.20f, 0.20f, 0.20f));
        drawHammerBevel(0, 32.0f, w, 40.0f, false);

        float tx = 10.0f;
        auto drawTool = [&](int iconId, const std::string& tooltip, bool active = false) -> bool {
            (void)tooltip;
            bool clicked = false;
            bool hover = (_mouseX >= tx && _mouseX <= tx + 32.0f && _mouseY >= 36.0f && _mouseY <= 68.0f);
            bool pressed = hover && _lmbPressed;
            Vec3 bg = active ? Vec3(1.0f, 0.55f, 0.1f) : (pressed ? Vec3(0.14f, 0.14f, 0.14f) : (hover ? Vec3(0.26f, 0.26f, 0.26f) : Vec3(0.20f, 0.20f, 0.20f)));
            Renderer::drawRect(tx, 36.0f, 32.0f, 32.0f, bg);
            drawHammerBevel(tx, 36.0f, 32.0f, 32.0f, pressed || active);
            HammerIcons::drawToolbarIcon(iconId, tx + 4.0f, 40.0f, active ? Vec3(1,1,1) : Vec3(0.85f, 0.85f, 0.85f), bg);
            if (hover && _lmbClicked) clicked = true;
            tx += 36.0f;
            return clicked;
        };
        auto drawSep = [&]() {
            Renderer::drawRect(tx + 3.0f, 38.0f, 1.0f, 28.0f, Vec3(0.12f, 0.12f, 0.12f));
            Renderer::drawRect(tx + 4.0f, 38.0f, 1.0f, 28.0f, Vec3(0.35f, 0.35f, 0.35f));
            tx += 10.0f;
        };

        if (drawTool(0, "New Layout")) { _project = HUDProject(); _selectedRef.invalidate(); _selectedRefs.clear(); _projectDirty = true; }
        if (drawTool(1, "Open...")) {
            std::string chosen = LabDialogs::openFileDialog(_window, "Lab HUD Files (*.labhud)\0*.labhud\0All Files (*.*)\0*.*\0", "assets\\hud_projects");
            if (!chosen.empty()) {
                auto loaded = HUDProject::loadFromFile(chosen);
                if (loaded) { _project = *loaded; _projectFilePath = chosen; _projectDirty = false; }
            }
        }
        if (drawTool(2, "Save")) {
            if (_projectFilePath.empty()) _projectFilePath = "assets/hud_projects/my_hud.labhud";
            _project.saveToFile(_projectFilePath);
            _projectDirty = false;
        }
        if (drawTool(3, "Save As...")) {
            std::string savePath = LabDialogs::saveFileDialog(_window, "Lab HUD Files (*.labhud)\0*.labhud\0All Files (*.*)\0*.*\0", "custom_hud.labhud", "assets\\hud_projects");
            if (!savePath.empty()) {
                _project.saveToFile(savePath);
                _projectFilePath = savePath;
                _projectDirty = false;
            }
        }
        drawSep();
        if (drawTool(4, "Undo")) undo();
        drawSep();
        if (drawTool(5, "Delete")) deleteSelected();
        if (drawTool(6, "Duplicate")) duplicateSelected();
        drawSep();
        if (drawTool(8, "Import Asset from Disk")) importCustomAsset();
    }

    void LabHUDEditor2D::renderStatusBar(float w, float h) {
        Renderer::drawRect(0, h - 28.0f, w, 28.0f, Vec3(0.14f, 0.14f, 0.14f));
        drawHammerBevel(0, h - 28.0f, w, 28.0f, false);

        char buf[256];
        snprintf(buf, sizeof(buf), "Tool: Select | Zoom: %.2fx | Snap: %.0fpx | Elements: %zu | Project: %s%s",
            _canvasZoom, _gridSnap, _project.rootElements.size(), _project.name.c_str(), _projectDirty ? " *" : "");
        LabFont::drawText(12.0f, h - 22.0f, buf, 1.4f, Vec3(0.85f, 0.85f, 0.85f), LabFontType::System);
    }

    void LabHUDEditor2D::renderDropdownMenus(float w, float h) {
        (void)w; (void)h;
        if (_activeDropdown == HUDEditorDropdown::None) return;

        float mx = 10.0f + static_cast<float>(static_cast<int>(_activeDropdown)) * 72.0f;
        std::vector<std::string> items;

        if (_activeDropdown == HUDEditorDropdown::File) {
            items = {"New Project", "Open Project...", "Save", "Save As...", "-", "Close to Launcher", "Exit"};
        } else if (_activeDropdown == HUDEditorDropdown::Edit) {
            items = {"Undo (Ctrl+Z)", "Redo (Ctrl+Y)", "-", "Delete (Del)", "Duplicate (Ctrl+D)", "Select All (Ctrl+A)"};
        } else if (_activeDropdown == HUDEditorDropdown::View) {
            items = {"Zoom In (+)", "Zoom Out (-)", "Reset Zoom & Pan", "-", "Toggle Grid", "Toggle Guides"};
        } else if (_activeDropdown == HUDEditorDropdown::Insert) {
            for (const auto& t : _widgetTemplates) items.push_back(t.name);
        } else if (_activeDropdown == HUDEditorDropdown::Assets) {
            items = {"Import Asset from Disk...", "Refresh Assets Folder"};
        } else if (_activeDropdown == HUDEditorDropdown::Help) {
            items = {"Documentation (VDC Online)", "About Lab HUD"};
        }

        float mw = 220.0f;
        float mh = static_cast<float>(items.size()) * 28.0f + 8.0f;
        Renderer::drawRect(mx, 32.0f, mw, mh, Vec3(0.18f, 0.18f, 0.18f));
        drawHammerBevel(mx, 32.0f, mw, mh, false);

        float py = 36.0f;
        for (const auto& item : items) {
            bool hover = (_mouseX >= mx && _mouseX <= mx + mw && _mouseY >= py && _mouseY < py + 28.0f);
            if (drawHammerDropdownItem(mx, py, mw, 28.0f, item, hover)) {
                _activeDropdown = HUDEditorDropdown::None;
                if (item == "Exit") _requestExit = true;
                if (item == "Close to Launcher") { _mode = HUDEditorMode::ProjectSelect; scanProjects(); }
                if (item == "Save") { _project.saveToFile(_projectFilePath); _projectDirty = false; }
                if (item == "Undo (Ctrl+Z)") undo();
                if (item == "Redo (Ctrl+Y)") redo();
                if (item == "Delete (Del)") deleteSelected();
                if (item == "Duplicate (Ctrl+D)") duplicateSelected();
                if (item == "Select All (Ctrl+A)") {
                    _selectedRefs.clear();
                    for (int i = 0; i < (int)_project.rootElements.size(); ++i) _selectedRefs.push_back({i, -1});
                    if (!_selectedRefs.empty()) _selectedRef = _selectedRefs[0];
                }
                if (item == "Import Asset from Disk...") importCustomAsset();
                if (item == "Refresh Assets Folder") scanAssets();
                if (item == "Toggle Grid") _showGrid = !_showGrid;
                if (item == "Toggle Guides") _showGuides = !_showGuides;
                if (item == "Zoom In (+)") _canvasZoom = std::min(5.0f, _canvasZoom * 1.25f);
                if (item == "Zoom Out (-)") _canvasZoom = std::max(0.1f, _canvasZoom / 1.25f);
                if (item == "Reset Zoom & Pan") { _canvasZoom = 0.75f; _canvasPanX = 20.0f; _canvasPanY = 20.0f; }
            }
            py += 28.0f;
        }
    }

} // namespace Lab

