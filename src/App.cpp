#include "App.h"

#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include <GLFW/glfw3.h>
#include <nfd.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>

// ─────────────────────────────────────────────────────────────────────────────
//  Colour helpers
// ─────────────────────────────────────────────────────────────────────────────

static void hexToFloat(const std::string& hex, float* c)
{
    if (hex.size() < 7 || hex[0] != '#') { c[0]=c[1]=c[2]=1.f; return; }
    try {
        c[0] = std::stoi(hex.substr(1,2),nullptr,16) / 255.f;
        c[1] = std::stoi(hex.substr(3,2),nullptr,16) / 255.f;
        c[2] = std::stoi(hex.substr(5,2),nullptr,16) / 255.f;
    } catch (...) { c[0]=c[1]=c[2]=1.f; }
}

static std::string floatToHex(const float* c)
{
    char buf[8];
    std::snprintf(buf, sizeof(buf), "#%02x%02x%02x",
        static_cast<int>(std::round(c[0]*255.f)),
        static_cast<int>(std::round(c[1]*255.f)),
        static_cast<int>(std::round(c[2]*255.f)));
    return buf;
}

// hex → ImVec4 (alpha=1)
static ImVec4 hv(const std::string& hex)
{
    float c[3]; hexToFloat(hex, c);
    return {c[0], c[1], c[2], 1.f};
}

// ImVec4 → ImU32
static ImU32 vc(const ImVec4& v)
{
    return ImGui::ColorConvertFloat4ToU32(v);
}

static ImU32 hu(const std::string& hex) { return vc(hv(hex)); }

// ─────────────────────────────────────────────────────────────────────────────
//  Init / Shutdown
// ─────────────────────────────────────────────────────────────────────────────

bool App::init()
{
    if (!glfwInit()) return false;

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    m_window = glfwCreateWindow(1400, 800, "Naleystogramm — Редактор тем", nullptr, nullptr);
    if (!m_window) { glfwTerminate(); return false; }

    glfwMakeContextCurrent(m_window);
    glfwSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    // Загружаем Roboto с кириллицей (fallback на встроенный шрифт если файл не найден)
    {
        static constexpr ImWchar kRanges[] = {
            0x0020, 0x00FF,  // Latin + Basic Latin
            0x0400, 0x04FF,  // Кириллица
            0,
        };
        ImFontConfig cfg;
        cfg.OversampleH = 2;
        cfg.OversampleV = 2;
        ImFont* f = io.Fonts->AddFontFromFileTTF(
            "assets/fonts/Roboto-Regular.ttf", 14.f, &cfg, kRanges);
        if (!f) io.Fonts->AddFontDefault();
    }

    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForOpenGL(m_window, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    NFD_Init();
    return true;
}

void App::shutdown()
{
    NFD_Quit();
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(m_window);
    glfwTerminate();
}

// ─────────────────────────────────────────────────────────────────────────────
//  Main loop
// ─────────────────────────────────────────────────────────────────────────────

void App::run()
{
    while (!glfwWindowShouldClose(m_window))
    {
        glfwPollEvents();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        render();

        ImGui::Render();

        int w, h;
        glfwGetFramebufferSize(m_window, &w, &h);
        glViewport(0, 0, w, h);
        glClearColor(0.08f, 0.08f, 0.10f, 1.f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(m_window);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Layout
// ─────────────────────────────────────────────────────────────────────────────

void App::render()
{
    ImGuiIO& io = ImGui::GetIO();
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_N, false)) actionNew();
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_O, false)) actionOpen();
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S, false)) actionSave();

    ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->WorkPos);
    ImGui::SetNextWindowSize(vp->WorkSize);
    ImGui::SetNextWindowViewport(vp->ID);

    constexpr ImGuiWindowFlags kHostFlags =
        ImGuiWindowFlags_NoTitleBar     | ImGuiWindowFlags_NoCollapse    |
        ImGuiWindowFlags_NoResize       | ImGuiWindowFlags_NoMove        |
        ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus |
        ImGuiWindowFlags_MenuBar        | ImGuiWindowFlags_NoDocking;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding,   0.f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,    ImVec2(0,0));
    ImGui::Begin("##Host", nullptr, kHostFlags);
    ImGui::PopStyleVar(3);

    renderMenuBar();

    const ImGuiID dsid = ImGui::GetID("##MainDS");
    ImGui::DockSpace(dsid, ImVec2(0,0));

    if (!m_dockLayoutReady) {
        setupDockLayout(dsid);
        m_dockLayoutReady = true;
    }

    ImGui::End();

    renderMetaPanel();
    renderPalettePanel();
    renderPreviewPanel();
}

void App::setupDockLayout(unsigned dsid)
{
    ImGui::DockBuilderRemoveNode(dsid);
    ImGui::DockBuilderAddNode(dsid, ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(dsid, ImGui::GetMainViewport()->WorkSize);

    ImGuiID left, center, right;
    ImGui::DockBuilderSplitNode(dsid,   ImGuiDir_Left,  0.20f, &left,   &center);
    ImGui::DockBuilderSplitNode(center, ImGuiDir_Right, 0.42f, &right,  &center);

    ImGui::DockBuilderDockWindow("Тема",    left);
    ImGui::DockBuilderDockWindow("Палитра", center);
    ImGui::DockBuilderDockWindow("Превью",  right);
    ImGui::DockBuilderFinish(dsid);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Menu bar
// ─────────────────────────────────────────────────────────────────────────────

void App::renderMenuBar()
{
    if (!ImGui::BeginMenuBar()) return;

    if (ImGui::BeginMenu("Файл"))
    {
        if (ImGui::MenuItem("Новая тема",      "Ctrl+N")) actionNew();
        if (ImGui::MenuItem("Открыть...",      "Ctrl+O")) actionOpen();
        ImGui::Separator();
        if (ImGui::MenuItem("Сохранить",       "Ctrl+S", false, m_theme.has_value())) actionSave();
        if (ImGui::MenuItem("Сохранить как...", nullptr,  false, m_theme.has_value())) actionSaveAs();
        ImGui::Separator();
        if (ImGui::MenuItem("Экспорт .zip...", nullptr,  false, m_theme.has_value())) actionExport();
        ImGui::Separator();
        if (ImGui::MenuItem("Выход"))
            glfwSetWindowShouldClose(m_window, true);
        ImGui::EndMenu();
    }

    // Правый край: статус
    if (!m_statusMsg.empty()) {
        const float msgW = ImGui::CalcTextSize(m_statusMsg.c_str()).x + 8.f;
        ImGui::SetCursorPosX(ImGui::GetContentRegionMax().x - msgW);
        ImGui::TextDisabled("%s", m_statusMsg.c_str());
    }

    ImGui::EndMenuBar();
}

// ─────────────────────────────────────────────────────────────────────────────
//  Meta panel — имя, автор, кнопки
// ─────────────────────────────────────────────────────────────────────────────

void App::renderMetaPanel()
{
    ImGui::Begin("Тема");

    if (!m_theme) {
        ImGui::TextDisabled("(нет темы)");
        ImGui::Spacing();
        if (ImGui::Button("Новая тема", ImVec2(-1,0))) actionNew();
        if (ImGui::Button("Открыть...", ImVec2(-1,0))) actionOpen();
        ImGui::End();
        return;
    }

    ImGui::SeparatorText("Метаданные");

    ImGui::TextDisabled("Название");
    ImGui::SetNextItemWidth(-1);
    if (ImGui::InputText("##name", m_nameBuf, sizeof(m_nameBuf))) {
        m_theme->name = m_nameBuf;
        m_dirty = true;
    }

    ImGui::Spacing();
    ImGui::TextDisabled("Автор");
    ImGui::SetNextItemWidth(-1);
    if (ImGui::InputText("##author", m_authorBuf, sizeof(m_authorBuf))) {
        m_theme->author = m_authorBuf;
        m_dirty = true;
    }

    ImGui::Spacing();
    ImGui::SeparatorText("Файл");

    if (!m_theme->jsonPath.empty()) {
        ImGui::TextDisabled("%s", m_theme->jsonPath.filename().string().c_str());
        ImGui::TextDisabled("%s", m_theme->rootDir.string().c_str());
    } else {
        ImGui::TextDisabled("(не сохранено)");
    }

    ImGui::Spacing();

    const char* saveLabel = m_dirty ? "Сохранить *" : "Сохранить";
    if (ImGui::Button(saveLabel,         ImVec2(-1,0))) actionSave();
    if (ImGui::Button("Сохранить как...",ImVec2(-1,0))) actionSaveAs();
    ImGui::Spacing();
    if (ImGui::Button("Экспорт .zip...", ImVec2(-1,0))) actionExport();

    ImGui::Spacing();
    ImGui::SeparatorText("Справка");
    ImGui::TextWrapped(
        "Тема совместима с Naleystogramm.\n"
        "После экспорта импортируй .zip через\n"
        "Настройки → Интерфейс → Импорт темы."
    );

    ImGui::End();
}

// ─────────────────────────────────────────────────────────────────────────────
//  Palette panel — 23 colour pickers + CSS tab
// ─────────────────────────────────────────────────────────────────────────────

void App::renderPalettePanel()
{
    ImGui::Begin("Палитра");

    if (!m_theme) {
        ImGui::TextDisabled("(нет темы — Файл > Новая или Открыть)");
        ImGui::End();
        return;
    }

    auto& p = m_theme->palette;
    bool changed = false;

    // Редактируем один цвет: [swatch+picker] label
    auto row = [&](const char* label, std::string& hex) {
        float c[3]; hexToFloat(hex, c);
        ImGui::PushID(label);
        if (ImGui::ColorEdit3("", c, ImGuiColorEditFlags_NoInputs)) {
            hex = floatToHex(c);
            changed = true;
        }
        ImGui::SameLine();
        // Hex input
        char buf[8];
        std::strncpy(buf, hex.c_str(), 7); buf[7] = '\0';
        ImGui::SetNextItemWidth(72.f);
        if (ImGui::InputText("##h", buf, 8,
            ImGuiInputTextFlags_CharsHexadecimal | ImGuiInputTextFlags_AutoSelectAll))
        {
            // Принимаем только если введён полный #rrggbb
            std::string s = buf;
            if (s.size() == 7 && s[0] == '#') { hex = s; changed = true; }
        }
        ImGui::SameLine();
        ImGui::TextUnformatted(label);
        ImGui::PopID();
    };

    if (ImGui::BeginTabBar("##palTabs"))
    {
        if (ImGui::BeginTabItem("Палитра"))
        {
            ImGui::Spacing();

            if (ImGui::TreeNodeEx("Фоны", ImGuiTreeNodeFlags_DefaultOpen)) {
                row("bg",          p.bg);
                row("bgSurface",   p.bgSurface);
                row("bgElevated",  p.bgElevated);
                row("bgInput",     p.bgInput);
                row("bgBubbleOut", p.bgBubbleOut);
                row("bgBubbleIn",  p.bgBubbleIn);
                ImGui::TreePop();
            }
            ImGui::Spacing();

            if (ImGui::TreeNodeEx("Границы", ImGuiTreeNodeFlags_DefaultOpen)) {
                row("border",      p.border);
                row("borderFocus", p.borderFocus);
                ImGui::TreePop();
            }
            ImGui::Spacing();

            if (ImGui::TreeNodeEx("Текст", ImGuiTreeNodeFlags_DefaultOpen)) {
                row("textPrimary",   p.textPrimary);
                row("textSecondary", p.textSecondary);
                row("textMuted",     p.textMuted);
                row("textOnAccent",  p.textOnAccent);
                ImGui::TreePop();
            }
            ImGui::Spacing();

            if (ImGui::TreeNodeEx("Акцент", ImGuiTreeNodeFlags_DefaultOpen)) {
                row("accent",        p.accent);
                row("accentHover",   p.accentHover);
                row("accentPressed", p.accentPressed);
                ImGui::TreePop();
            }
            ImGui::Spacing();

            if (ImGui::TreeNodeEx("Статус", ImGuiTreeNodeFlags_DefaultOpen)) {
                row("online",  p.online);
                row("offline", p.offline);
                row("danger",  p.danger);
                row("success", p.success);
                ImGui::TreePop();
            }
            ImGui::Spacing();

            if (ImGui::TreeNodeEx("Баннер", ImGuiTreeNodeFlags_DefaultOpen)) {
                row("bannerBg",       p.bannerBg);
                row("bannerBorder",   p.bannerBorder);
                row("bannerText",     p.bannerText);
                row("bannerBtnHover", p.bannerBtnHover);
                ImGui::TreePop();
            }

            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("CSS"))
        {
            ImGui::TextDisabled("css/main.css — опциональный QSS-переопределитель");
            ImGui::Spacing();
            if (ImGui::InputTextMultiline("##css", m_cssBuf, kCssBufSize,
                ImVec2(-1,-1), ImGuiInputTextFlags_AllowTabInput))
            {
                m_theme->customCss = m_cssBuf;
                changed = true;
            }
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }

    if (changed) m_dirty = true;

    ImGui::End();
}

// ─────────────────────────────────────────────────────────────────────────────
//  Preview panel — симуляция интерфейса Naleystogramm
// ─────────────────────────────────────────────────────────────────────────────

void App::renderPreviewPanel()
{
    ImGui::Begin("Превью");

    if (!m_theme) {
        ImGui::TextDisabled("(откройте или создайте тему)");
        ImGui::End();
        return;
    }

    const ThemePalette& p = m_theme->palette;

    const ImVec2 sz  = ImGui::GetContentRegionAvail();
    if (sz.x < 40 || sz.y < 40) { ImGui::End(); return; }

    const ImVec2 origin = ImGui::GetCursorScreenPos();
    ImDrawList* dl = ImGui::GetWindowDrawList();

    const float W = sz.x, H = sz.y;

    // Пропорции окна Naleystogramm
    const float sideW = W * 0.30f;
    const float chatW = W - sideW - 1.f;
    const float hdrH  = std::max(42.f, H * 0.072f);
    const float inpH  = std::max(46.f, H * 0.085f);
    const float msgH  = H - hdrH - inpH;

    // Общий фон
    dl->AddRectFilled(origin, {origin.x + W, origin.y + H}, hu(p.bg));

    // ══ ЛЕВАЯ ПАНЕЛЬ ══════════════════════════════════════════════════════

    const float sx = origin.x, sy = origin.y;

    dl->AddRectFilled({sx, sy}, {sx + sideW, sy + H}, hu(p.bgSurface));
    dl->AddLine({sx + sideW, sy}, {sx + sideW, sy + H}, hu(p.border), 1.f);

    // Хедер левой панели
    dl->AddRectFilled({sx, sy}, {sx + sideW, sy + hdrH}, hu(p.bgElevated));
    dl->AddLine({sx, sy + hdrH}, {sx + sideW, sy + hdrH}, hu(p.border), 1.f);

    // Мой аватар
    const float myAvR = hdrH * 0.34f;
    const ImVec2 myAvC = {sx + myAvR + 8.f, sy + hdrH * 0.5f};
    dl->AddCircleFilled(myAvC, myAvR, hu(p.accent));
    dl->AddText({myAvC.x + myAvR + 6.f, sy + hdrH * 0.5f - 6.f}, hu(p.textPrimary), "Вы");

    // Список контактов
    struct Contact { const char* name; const char* lastMsg; bool online; bool selected; };
    constexpr Contact kContacts[] = {
        {"Алекс",  "Привет!",       true,  true },
        {"Мария",  "Окей",          false, false},
        {"Дима",   "Давай завтра",  true,  false},
        {"Катя",   "Спасибо :)",    false, false},
    };

    float cy = sy + hdrH + 2.f;
    const float iH = std::min(52.f, (H - hdrH - 52.f) / 4.f);

    for (const auto& c : kContacts) {
        if (c.selected)
            dl->AddRectFilled({sx + 3.f, cy}, {sx + sideW - 3.f, cy + iH},
                              hu(p.bgElevated), 8.f);

        const float cR = iH * 0.34f;
        const ImVec2 cC = {sx + cR + 8.f, cy + iH * 0.5f};
        dl->AddCircleFilled(cC, cR, hu(p.bgInput));

        // Статус-точка
        const float dotR = cR * 0.35f;
        dl->AddCircleFilled({cC.x + cR * 0.65f, cC.y + cR * 0.65f}, dotR,
                            c.online ? hu(p.online) : hu(p.offline));

        // Имя
        dl->AddText({cC.x + cR + 6.f, cy + iH * 0.22f - 6.f},
                    hu(p.textPrimary), c.name);
        // Последнее сообщение
        dl->AddText({cC.x + cR + 6.f, cy + iH * 0.57f - 6.f},
                    hu(p.textMuted), c.lastMsg);

        cy += iH;
    }

    // Кнопка добавить контакт
    {
        const float btnH2 = hdrH * 0.72f;
        const float btnY  = sy + H - btnH2 - 8.f;
        dl->AddRectFilled({sx + 8.f, btnY}, {sx + sideW - 8.f, btnY + btnH2},
                          hu(p.accent), 12.f);
        const char* btnTxt = "+ Контакт";
        const ImVec2 ts = ImGui::CalcTextSize(btnTxt);
        dl->AddText({sx + (sideW - ts.x) * 0.5f, btnY + (btnH2 - ts.y) * 0.5f},
                    hu(p.textOnAccent), btnTxt);
    }

    // ══ ПРАВАЯ ПАНЕЛЬ — ЧАТ ══════════════════════════════════════════════

    const float cx = origin.x + sideW + 1.f;

    // Хедер чата
    dl->AddRectFilled({cx, sy}, {cx + chatW, sy + hdrH}, hu(p.bgElevated));
    dl->AddLine({cx, sy + hdrH}, {cx + chatW, sy + hdrH}, hu(p.border), 1.f);

    // Аватар собеседника
    const float pR = hdrH * 0.37f;
    const ImVec2 pC = {cx + pR + 8.f, sy + hdrH * 0.5f};
    dl->AddCircleFilled(pC, pR, hu(p.bgInput));
    dl->AddCircleFilled({pC.x + pR * 0.65f, pC.y + pR * 0.65f}, pR * 0.35f, hu(p.online));
    dl->AddText({pC.x + pR + 6.f, sy + hdrH * 0.18f}, hu(p.textPrimary), "Алекс");
    dl->AddText({pC.x + pR + 6.f, sy + hdrH * 0.56f}, hu(p.online), "в сети");

    // Область сообщений
    dl->AddRectFilled({cx, sy + hdrH}, {cx + chatW, sy + hdrH + msgH}, hu(p.bg));

    // Пузыри
    struct Msg { const char* text; bool out; };
    constexpr Msg kMsgs[] = {
        {"Привет! Как дела?",            false},
        {"Отлично, спасибо!",            true },
        {"Чем занимаешься?",            false},
        {"Пишу тему для Naleystogramm", true },
    };

    float my = sy + hdrH + 8.f;
    const float bPad = 9.f, bH = 28.f, bGap = 5.f;

    for (const auto& m : kMsgs) {
        const ImVec2 ts = ImGui::CalcTextSize(m.text);
        float bw = ts.x + bPad * 2.f;
        bw = std::min(bw, chatW * 0.72f);
        const float bx = m.out ? (cx + chatW - bw - 8.f) : (cx + 8.f);

        dl->AddRectFilled({bx, my}, {bx + bw, my + bH},
                          m.out ? hu(p.bgBubbleOut) : hu(p.bgBubbleIn), 10.f);
        dl->AddText({bx + bPad, my + (bH - 13.f) * 0.5f},
                    m.out ? hu(p.textOnAccent) : hu(p.textPrimary), m.text);
        my += bH + bGap;
    }

    // ── Строка ввода ──────────────────────────────────────────────────────

    const float iy = sy + H - inpH;
    dl->AddRectFilled({cx, iy}, {cx + chatW, iy + inpH}, hu(p.bgElevated));
    dl->AddLine({cx, iy}, {cx + chatW, iy}, hu(p.border), 1.f);

    // Поле ввода
    const float ifPad  = 8.f;
    const float sbSize = inpH - 14.f;
    const float ifW    = chatW - sbSize - ifPad * 3.f;
    const float ifY1   = iy + 7.f, ifY2 = iy + inpH - 7.f;
    dl->AddRectFilled({cx + ifPad, ifY1}, {cx + ifPad + ifW, ifY2}, hu(p.bgInput), 15.f);
    dl->AddRect      ({cx + ifPad, ifY1}, {cx + ifPad + ifW, ifY2}, hu(p.border),  15.f, 0, 1.f);

    const char* placeholder = "Сообщение...";
    dl->AddText({cx + ifPad + 13.f, iy + inpH * 0.5f - 6.f}, hu(p.textMuted), placeholder);

    // Кнопка отправки
    const float sbx = cx + chatW - sbSize - ifPad;
    const float sby = iy + 7.f;
    const ImVec2 sbCenter = {sbx + sbSize * 0.5f, sby + sbSize * 0.5f};
    dl->AddCircleFilled(sbCenter, sbSize * 0.5f, hu(p.accent));
    dl->AddText({sbCenter.x - 5.f, sbCenter.y - 7.f}, hu(p.textOnAccent), "\xe2\x86\x92");

    // Заполняем content region чтобы ImGui не делал скролл
    ImGui::Dummy(sz);

    ImGui::End();
}

// ─────────────────────────────────────────────────────────────────────────────
//  Actions
// ─────────────────────────────────────────────────────────────────────────────

void App::syncBuffersFromTheme()
{
    if (!m_theme) return;
    std::strncpy(m_nameBuf,   m_theme->name.c_str(),      sizeof(m_nameBuf)  - 1);
    std::strncpy(m_authorBuf, m_theme->author.c_str(),    sizeof(m_authorBuf)- 1);
    std::strncpy(m_cssBuf,    m_theme->customCss.c_str(), kCssBufSize        - 1);
    m_nameBuf  [sizeof(m_nameBuf)  - 1] = '\0';
    m_authorBuf[sizeof(m_authorBuf)- 1] = '\0';
    m_cssBuf   [kCssBufSize        - 1] = '\0';
}

void App::actionNew()
{
    m_theme = Theme::createDefault();
    m_dirty = true;
    syncBuffersFromTheme();
    m_statusMsg = "Новая тема создана";
}

void App::actionOpen()
{
    nfdchar_t* outPath = nullptr;
    nfdfilteritem_t filters[] = {{"Theme JSON", "json"}};

    if (NFD_OpenDialog(&outPath, filters, 1, nullptr) == NFD_OKAY) {
        auto result = Theme::open(outPath);
        NFD_FreePath(outPath);

        if (result) {
            m_theme = std::move(*result);
            m_dirty = false;
            syncBuffersFromTheme();
            m_statusMsg = "Открыто: " + m_theme->jsonPath.filename().string();
        } else {
            m_statusMsg = "Ошибка: " + result.error();
        }
    }
}

void App::actionSave()
{
    if (!m_theme) return;
    if (m_theme->jsonPath.empty()) { actionSaveAs(); return; }

    // Синхронизируем данные из буферов в тему
    m_theme->name      = m_nameBuf;
    m_theme->author    = m_authorBuf;
    m_theme->customCss = m_cssBuf;

    auto result = m_theme->save();
    if (result) {
        m_dirty = false;
        m_statusMsg = "Сохранено: " + m_theme->jsonPath.filename().string();
    } else {
        m_statusMsg = "Ошибка: " + result.error();
    }
}

void App::actionSaveAs()
{
    if (!m_theme) return;

    nfdchar_t* savePath = nullptr;
    nfdfilteritem_t filters[] = {{"Theme JSON", "json"}};

    if (NFD_SaveDialog(&savePath, filters, 1, nullptr, "theme.json") == NFD_OKAY) {
        m_theme->jsonPath = savePath;
        m_theme->rootDir  = m_theme->jsonPath.parent_path();
        NFD_FreePath(savePath);
        actionSave();
    }
}

void App::actionExport()
{
    if (!m_theme) return;

    // Нужно знать rootDir — сначала сохраняем если нет
    if (m_theme->rootDir.empty()) {
        actionSaveAs();
        if (m_theme->rootDir.empty()) return;
    }
    // Актуальные данные на диске
    actionSave();

    nfdchar_t* savePath = nullptr;
    nfdfilteritem_t filters[] = {{"ZIP archive", "zip"}};
    const std::string defName = m_theme->name + ".zip";

    if (NFD_SaveDialog(&savePath, filters, 1, nullptr, defName.c_str()) == NFD_OKAY) {
        const auto& dir = m_theme->rootDir;
        // zip -r /path/to/out.zip folderName  (от родительской папки)
        const std::string cmd =
            "cd \"" + dir.parent_path().string() + "\" && "
            "zip -r \"" + std::string(savePath) + "\" \""
            + dir.filename().string() + "\"";

        const int ret = std::system(cmd.c_str());  // NOLINT
        if (ret == 0)
            m_statusMsg = "Экспортировано: "
                + std::filesystem::path(savePath).filename().string();
        else
            m_statusMsg = "Ошибка экспорта (установлен ли zip?)";

        NFD_FreePath(savePath);
    }
}
