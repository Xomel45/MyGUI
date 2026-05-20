# MyGUI — Редактор тем для Naleystogramm

**v0.2.0** · C++23 · Dear ImGui · OpenGL 3.3

Визуальный редактор кастомных тем для мессенджера [Naleystogramm](https://github.com/xomel45/naleystogramm).
Позволяет редактировать 23 цвета палитры и CSS-переопределение с live-превью интерфейса.

## Возможности

- Редактирование 23 цветов палитры через цветовые пикеры
- CSS-редактор (встроен в отдельную вкладку)
- Live-превью интерфейса Naleystogramm: список контактов + чат
- Открытие / сохранение `theme.json` через нативные диалоги
- Сохранение `css/main.css` рядом с темой

## Формат темы

```json
{
    "name": "MyTheme",
    "author": "xomel45",
    "palette": {
        "bg":            "#080810",
        "bgSurface":     "#0e0e1c",
        "bgElevated":    "#13132a",
        "bgInput":       "#18182e",
        "bgBubbleOut":   "#2a1f5e",
        "bgBubbleIn":    "#161628",
        "border":        "#1e1e3a",
        "borderFocus":   "#6c5ce7",
        "textPrimary":   "#ece9ff",
        "textSecondary": "#b8b4d8",
        "textMuted":     "#5a5880",
        "textOnAccent":  "#ffffff",
        "accent":        "#6c5ce7",
        "accentHover":   "#8677ff",
        "accentPressed": "#5449c4",
        "online":        "#00cba9",
        "offline":       "#3a3a5c",
        "danger":        "#ff4d6d",
        "success":       "#00cba9",
        "bannerBg":      "#1e1b3a",
        "bannerBorder":  "#6c5ce7",
        "bannerText":    "#e2e2f0",
        "bannerBtnHover":"#8075e5"
    }
}
```

## Сборка

Требования: **CMake ≥ 3.20**, **GCC/Clang с C++23**, **OpenGL**, **GTK3** (Linux).  
Все зависимости скачиваются автоматически через `FetchContent`.

```bash
cmake -B build-release -DCMAKE_BUILD_TYPE=Release
cmake --build build-release --parallel
./build-release/MyGUI
```

### Зависимости (auto-fetched)

| Библиотека | Назначение |
|---|---|
| [Dear ImGui](https://github.com/ocornut/imgui) (docking) | GUI |
| [GLFW](https://github.com/glfw/glfw) | Окно / OpenGL контекст |
| [nlohmann/json](https://github.com/nlohmann/json) | Парсинг JSON |
| [nativefiledialog-extended](https://github.com/btzy/nativefiledialog-extended) | Нативные диалоги |
| [stb](https://github.com/nothings/stb) | Утилиты изображений |

## Деплой

```bash
./deploy.sh beta --build              # ELF + assets → builds/beta/
./deploy.sh release linux-all --build # AppImage + .pkg + .deb + .rpm
```

| Артефакт | Дистрибутив |
|---|---|
| `.AppImage` | Любой Linux (самодостаточный) |
| `.pkg.tar.zst` | Arch / Manjaro / SteamOS |
| `.deb` | Debian / Ubuntu / Mint |
| `.rpm` | Fedora / RHEL / openSUSE |

## Горячие клавиши

| Сочетание | Действие |
|---|---|
| `Ctrl+N` | Новая тема |
| `Ctrl+O` | Открыть тему |
| `Ctrl+S` | Сохранить |
| `Ctrl+Shift+S` | Сохранить как |

## Лицензия

GNU General Public License v3.0 — см. [LICENSE](LICENSE).
