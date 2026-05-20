#pragma once
#include "Theme.h"
#include <optional>
#include <string>

struct GLFWwindow;

class App
{
public:
    bool init();
    void run();
    void shutdown();

private:
    // ── Rendering ─────────────────────────────────────────────────────────
    void render();
    void setupDockLayout(unsigned dsid);
    void renderMenuBar();
    void renderMetaPanel();     // метаданные + действия
    void renderPalettePanel();  // 23 цвета + CSS вкладка
    void renderPreviewPanel();  // live-превью интерфейса Naleystogramm

    // ── Actions ───────────────────────────────────────────────────────────
    void actionNew();
    void actionOpen();
    void actionSave();
    void actionSaveAs();
    void actionExport();   // экспорт в .zip для импорта в Naleystogramm

    // ── Helpers ───────────────────────────────────────────────────────────
    void syncBuffersFromTheme();  // обновить m_nameBuf / m_authorBuf / m_cssBuf

    // ── State ─────────────────────────────────────────────────────────────
    GLFWwindow*          m_window          = nullptr;
    bool                 m_dockLayoutReady = false;

    std::optional<Theme> m_theme;
    bool                 m_dirty           = false;
    std::string          m_statusMsg;

    // Буферы редактирования
    char m_nameBuf[128]   = {};
    char m_authorBuf[128] = {};

    static constexpr std::size_t kCssBufSize = 64 * 1024;
    char m_cssBuf[kCssBufSize] = {};
};
