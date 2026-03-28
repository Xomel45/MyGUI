#pragma once
#include "Theme.h"
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <vector>

struct GLFWwindow;

// ─────────────────────────────────────────────────────────────────────────────

struct EditorTab {
    std::string           label;
    std::filesystem::path filePath;   // empty = not yet saved
    bool                  dirty = false;

    static constexpr size_t kBufSize = 1024 * 64;  // 64 KB
    char buf[kBufSize] = {};

    void        setContent(const std::string& s);
    std::string getContent() const { return buf; }
};

// ─────────────────────────────────────────────────────────────────────────────

class App
{
public:
    bool init();
    void run();
    void shutdown();

private:
    // ── Rendering ─────────────────────────────────────────────────────────
    void render();
    void setupDockLayout(unsigned int dsid);
    void renderMenuBar();
    void renderProjectsPanel();
    void renderEditorPanel();
    void renderPreviewPanel();

    // ── File operations ───────────────────────────────────────────────────
    void actionNew();
    void actionOpen();
    void actionSave();
    void actionSaveAs();

    // ── Theme / tab management ────────────────────────────────────────────
    void loadTheme(const std::filesystem::path& path);
    void applyThemeToEditor();
    int  findOrOpenTab(const std::filesystem::path& path, const std::string& label);

    // ── Preview helpers ───────────────────────────────────────────────────
    void     renderPreviewMain();
    void     renderPreviewSplash();
    unsigned getTexture(const std::filesystem::path& path);
    void     clearTextures();

    // ── State ─────────────────────────────────────────────────────────────
    GLFWwindow*            m_window          = nullptr;
    bool                   m_dockLayoutReady = false;

    std::optional<Theme>   m_theme;
    std::vector<EditorTab> m_tabs;
    int                    m_activeTab       = 0;
    int                    m_pendingFocus    = -1;

    // GLuint == unsigned — no GL header needed in .h
    std::map<std::filesystem::path, unsigned> m_textures;

    std::string            m_statusMsg;
};
