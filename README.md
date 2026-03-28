# MyGUI — Theme Editor

A cross-platform GUI tool for creating and editing themes for your applications.
Themes are defined as **JSON + CSS** pairs: JSON stores metadata and file mappings, CSS stores visual styles.

## Features

- Edit `theme.json` and CSS files in a built-in tabbed editor
- Live UI preview: Main Screen (background + buttons) and Splash Screen (background + cycling texts)
- File tree showing the full structure of the loaded theme
- Native file dialogs (open / save)
- Plugin system *(coming soon)* — extend with support for new applications

## Theme format

```json
{
    "name": "MyTheme",
    "description": "A theme description.",
    "version": "1.0.0",
    "language": "En",
    "main": {
        "buttons": {
            "play":         "Play",
            "settings":     "Settings",
            "login":        "Login",
            "successfully": "Success",
            "logs":         "Logs",
            "themes":       "Themes"
        },
        "backgrounds": { "main":   "./backgrounds/bg_main.png" },
        "css":         { "main":   "./css/main.css" }
    },
    "splash": {
        "texts": [ "Welcome!", "Loading..." ],
        "backgrounds": { "splash": "./backgrounds/bg_splash.png" },
        "css":         { "splash": "./css/splash.css" }
    }
}
```

## Building

Requirements: **CMake ≥ 3.20**, **C++20 compiler**, **OpenGL**, **GTK3** (Linux).
All other dependencies are fetched automatically via `FetchContent`.

```bash
cmake -B build
cmake --build build --parallel
./build/MyGUI
```

### Dependencies (auto-fetched)

| Library | Purpose |
|---|---|
| [Dear ImGui](https://github.com/ocornut/imgui) (docking branch) | GUI |
| [GLFW](https://github.com/glfw/glfw) | Window / OpenGL context |
| [nlohmann/json](https://github.com/nlohmann/json) | JSON parsing |
| [nativefiledialog-extended](https://github.com/btzy/nativefiledialog-extended) | Native file dialogs |
| [stb_image](https://github.com/nothings/stb) | Image loading |

## Keyboard shortcuts

| Shortcut | Action |
|---|---|
| `Ctrl+N` | New theme |
| `Ctrl+O` | Open theme |
| `Ctrl+S` | Save |

## License

This project is licensed under the **GNU General Public License v3.0**.
See [LICENSE](LICENSE) for the full text.
