# Lab HUD Editor Core (LabHUDEditorCore.dll)

[![Engine: Lab](https://img.shields.io/badge/Engine-Lab-blue?style=for-the-badge)](https://github.com/YoungJasiek/Lab)
[![Online Documentation](https://img.shields.io/badge/Docs-Online-green?style=for-the-badge)](https://youngjasiek.github.io/Lab/module-hud.html)
[![Repository](https://img.shields.io/badge/GitHub-Lab-HUD-Core-blueviolet?style=for-the-badge)](https://github.com/YoungJasiek/Lab-HUD-Core)

Lab HUD Editor subsystem module providing 2D / HUD / GUI canvas management, widget definitions, transparency support, and `.labhud` serialization engine backend.

---

## 📦 Exported Headers
- `LabHUDEditor.h`

---

## 🔗 Dependencies
- `LabCore`
- `LabRender`
- `LabAnimation`
- `LabWorld`

---

## ⚙️ Standalone Build Instructions
Compile this module independently into `LabHUDEditorCore.dll`:
```powershell
mkdir build
cd build
cmake .. -A x64
cmake --build . --config Debug
```

---

## 👤 Author & Acknowledgments
* **Creator & Architect:** [YoungJasiek](https://github.com/YoungJasiek)
* **Special Thanks:** Sincere gratitude to **Valve Corporation** for their iconic Source Engine and Hammer tool philosophy inspiring LabStudio and Lab HUD.
