#include "App.h"

#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include <GLFW/glfw3.h>
#include <nfd.h>
#include <stb_image.h>

#include <algorithm>
#include <cstring>
#include <fstream>
#include <iterator>

// ─────────────────────────────────────────────────────────────────────────────
//  EditorTab
// ─────────────────────────────────────────────────────────────────────────────

void EditorTab::setContent(const std::string& s)
{
    size_t len = std::min(s.size(), kBufSize - 1);
    std::memcpy(buf, s.data(), len);
    std::memset(buf + len, 0, kBufSize - len);
}

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

    m_window = glfwCreateWindow(1280, 720, "MyGUI — Theme Editor", nullptr, nullptr);
    if (!m_window) { glfwTerminate(); return false; }

    glfwMakeContextCurrent(m_window);
    glfwSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForOpenGL(m_window, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    NFD_Init();

    return true;
}

void App::shutdown()
{
    clearTextures();
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
        glClearColor(0.10f, 0.10f, 0.10f, 1.00f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(m_window);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Rendering — layout
// ─────────────────────────────────────────────────────────────────────────────

void App::render()
{
    // Global keyboard shortcuts
    ImGuiIO& io = ImGui::GetIO();
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_N, false)) actionNew();
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_O, false)) actionOpen();
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S, false)) actionSave();

    ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->WorkPos);
    ImGui::SetNextWindowSize(vp->WorkSize);
    ImGui::SetNextWindowViewport(vp->ID);

    constexpr ImGuiWindowFlags kHostFlags =
        ImGuiWindowFlags_NoTitleBar          | ImGuiWindowFlags_NoCollapse          |
        ImGuiWindowFlags_NoResize            | ImGuiWindowFlags_NoMove              |
        ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus        |
        ImGuiWindowFlags_MenuBar             | ImGuiWindowFlags_NoDocking;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding,   0.f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,    ImVec2(0, 0));
    ImGui::Begin("##Host", nullptr, kHostFlags);
    ImGui::PopStyleVar(3);

    renderMenuBar();

    ImGuiID dsid = ImGui::GetID("##MainDS");
    ImGui::DockSpace(dsid, ImVec2(0, 0));

    if (!m_dockLayoutReady)
    {
        setupDockLayout(dsid);
        m_dockLayoutReady = true;
    }

    ImGui::End();

    renderProjectsPanel();
    renderEditorPanel();
    renderPreviewPanel();
}

void App::setupDockLayout(unsigned int dsid)
{
    ImGui::DockBuilderRemoveNode(dsid);
    ImGui::DockBuilderAddNode(dsid, ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(dsid, ImGui::GetMainViewport()->WorkSize);

    ImGuiID left, center, right;
    ImGui::DockBuilderSplitNode(dsid,   ImGuiDir_Left,  0.20f, &left,   &center);
    ImGui::DockBuilderSplitNode(center, ImGuiDir_Right, 0.35f, &right,  &center);

    ImGui::DockBuilderDockWindow("Projects", left);
    ImGui::DockBuilderDockWindow("Editor",   center);
    ImGui::DockBuilderDockWindow("Preview",  right);
    ImGui::DockBuilderFinish(dsid);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Menu bar
// ─────────────────────────────────────────────────────────────────────────────

void App::renderMenuBar()
{
    if (!ImGui::BeginMenuBar()) return;

    if (ImGui::BeginMenu("File"))
    {
        if (ImGui::MenuItem("New Theme",  "Ctrl+N")) actionNew();
        if (ImGui::MenuItem("Open...",    "Ctrl+O")) actionOpen();
        ImGui::Separator();
        if (ImGui::MenuItem("Save",       "Ctrl+S", false, m_theme.has_value())) actionSave();
        if (ImGui::MenuItem("Save As...",  nullptr,  false, m_theme.has_value())) actionSaveAs();
        ImGui::Separator();
        if (ImGui::MenuItem("Exit"))
            glfwSetWindowShouldClose(m_window, true);
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Plugins"))
    {
        if (ImGui::MenuItem("Manage Plugins...")) {}
        ImGui::EndMenu();
    }

    // Right-aligned status message
    if (!m_statusMsg.empty())
    {
        float msgW = ImGui::CalcTextSize(m_statusMsg.c_str()).x + 8.f;
        ImGui::SetCursorPosX(ImGui::GetContentRegionMax().x - msgW);
        ImGui::TextDisabled("%s", m_statusMsg.c_str());
    }

    ImGui::EndMenuBar();
}

// ─────────────────────────────────────────────────────────────────────────────
//  Projects panel
// ─────────────────────────────────────────────────────────────────────────────

void App::renderProjectsPanel()
{
    ImGui::Begin("Projects");

    if (!m_theme)
    {
        ImGui::TextDisabled("(no theme loaded)");
        ImGui::Spacing();
        if (ImGui::Button("New Theme"))  actionNew();
        ImGui::SameLine();
        if (ImGui::Button("Open..."))    actionOpen();
        ImGui::End();
        return;
    }

    auto& t = *m_theme;

    std::string title = t.name.empty() ? "(unnamed)" : t.name;
    title += "  v" + t.version;

    if (ImGui::TreeNodeEx(title.c_str(), ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanFullWidth))
    {
        if (!t.description.empty())
            ImGui::TextDisabled("%s", t.description.c_str());
        ImGui::TextDisabled("Language: %s", t.language.c_str());
        ImGui::Spacing();

        // ── Main screen ──────────────────────────────────────────────────
        if (ImGui::TreeNodeEx("Main Screen", ImGuiTreeNodeFlags_DefaultOpen))
        {
            // Buttons
            std::string btnLabel = "Buttons (" + std::to_string(t.main.buttons.size()) + ")";
            if (ImGui::TreeNode(btnLabel.c_str()))
            {
                for (auto& [k, v] : t.main.buttons)
                    ImGui::Text("%-16s  \"%s\"", k.c_str(), v.c_str());
                ImGui::TreePop();
            }
            // Backgrounds
            if (ImGui::TreeNode("Backgrounds##main"))
            {
                for (auto& [k, v] : t.main.backgrounds)
                    ImGui::Text("%-16s  %s", k.c_str(), v.c_str());
                ImGui::TreePop();
            }
            // CSS files — clickable to open in editor
            if (ImGui::TreeNode("CSS##main"))
            {
                for (auto& [k, v] : t.main.css)
                {
                    if (ImGui::Selectable(v.c_str()))
                    {
                        auto absPath  = t.rootDir / v;
                        std::string lbl = std::filesystem::path(v).filename().string();
                        m_pendingFocus = findOrOpenTab(absPath, lbl);
                    }
                }
                ImGui::TreePop();
            }
            ImGui::TreePop();
        }

        // ── Splash screen ─────────────────────────────────────────────────
        if (ImGui::TreeNodeEx("Splash Screen", ImGuiTreeNodeFlags_DefaultOpen))
        {
            std::string txtLabel = "Texts (" + std::to_string(t.splash.texts.size()) + ")";
            if (ImGui::TreeNode(txtLabel.c_str()))
            {
                for (auto& s : t.splash.texts)
                    ImGui::Text("\"%s\"", s.c_str());
                ImGui::TreePop();
            }
            if (ImGui::TreeNode("Backgrounds##splash"))
            {
                for (auto& [k, v] : t.splash.backgrounds)
                    ImGui::Text("%-16s  %s", k.c_str(), v.c_str());
                ImGui::TreePop();
            }
            if (ImGui::TreeNode("CSS##splash"))
            {
                for (auto& [k, v] : t.splash.css)
                {
                    if (ImGui::Selectable(v.c_str()))
                    {
                        auto absPath  = t.rootDir / v;
                        std::string lbl = std::filesystem::path(v).filename().string();
                        m_pendingFocus = findOrOpenTab(absPath, lbl);
                    }
                }
                ImGui::TreePop();
            }
            ImGui::TreePop();
        }

        ImGui::TreePop();
    }

    ImGui::End();
}

// ─────────────────────────────────────────────────────────────────────────────
//  Editor panel
// ─────────────────────────────────────────────────────────────────────────────

void App::renderEditorPanel()
{
    ImGui::Begin("Editor");

    if (m_tabs.empty())
    {
        ImGui::TextDisabled("(no theme loaded — File > New or Open)");
        ImGui::End();
        return;
    }

    if (ImGui::BeginTabBar("##Tabs", ImGuiTabBarFlags_TabListPopupButton | ImGuiTabBarFlags_FittingPolicyScroll))
    {
        for (int i = 0; i < (int)m_tabs.size(); i++)
        {
            auto& tab = m_tabs[i];

            ImGuiTabItemFlags flags = tab.dirty ? ImGuiTabItemFlags_UnsavedDocument : 0;
            if (m_pendingFocus == i)
            {
                flags |= ImGuiTabItemFlags_SetSelected;
                m_pendingFocus = -1;
            }

            bool open = true;
            // theme.json (tab 0) cannot be closed
            bool* pOpen = (i == 0) ? nullptr : &open;

            if (ImGui::BeginTabItem(tab.label.c_str(), pOpen, flags))
            {
                m_activeTab = i;
                if (ImGui::InputTextMultiline("##content", tab.buf, EditorTab::kBufSize,
                    ImVec2(-1, -1), ImGuiInputTextFlags_AllowTabInput))
                {
                    tab.dirty = true;
                }
                ImGui::EndTabItem();
            }

            if (!open)
            {
                m_tabs.erase(m_tabs.begin() + i);
                if (m_activeTab >= (int)m_tabs.size())
                    m_activeTab = (int)m_tabs.size() - 1;
                break;
            }
        }

        ImGui::EndTabBar();
    }

    ImGui::End();
}

// ─────────────────────────────────────────────────────────────────────────────
//  Preview panel
// ─────────────────────────────────────────────────────────────────────────────

void App::renderPreviewPanel()
{
    ImGui::Begin("Preview");

    if (!m_theme)
    {
        ImGui::TextDisabled("(open a theme to preview)");
        ImGui::End();
        return;
    }

    if (ImGui::BeginTabBar("##PreviewTabs"))
    {
        if (ImGui::BeginTabItem("Main Screen"))
        {
            renderPreviewMain();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Splash Screen"))
        {
            renderPreviewSplash();
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }

    ImGui::End();
}

// Draw background image (or a solid placeholder) + transparent overlay child window.
// Returns the overlay child window size so callers can place widgets inside.
static ImVec2 beginPreviewOverlay(unsigned texId, const char* childId)
{
    ImVec2 avail = ImGui::GetContentRegionAvail();
    if (avail.x < 1) avail.x = 1;
    if (avail.y < 1) avail.y = 1;

    ImVec2 p0 = ImGui::GetCursorScreenPos();
    ImVec2 p1 = ImVec2(p0.x + avail.x, p0.y + avail.y);

    ImDrawList* dl = ImGui::GetWindowDrawList();

    if (texId)
        dl->AddImage((ImTextureID)(uintptr_t)texId, p0, p1);
    else
        dl->AddRectFilled(p0, p1, IM_COL32(35, 35, 55, 255));

    // Transparent child window drawn on top of the image
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0, 0, 0, 0));
    ImGui::BeginChild(childId, avail, false,
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    ImGui::PopStyleColor();

    return avail;
}

void App::renderPreviewMain()
{
    auto& t = *m_theme;

    // Resolve background texture
    unsigned texId = 0;
    if (!t.rootDir.empty() && !t.main.backgrounds.empty())
    {
        auto it = t.main.backgrounds.count("main")
                ? t.main.backgrounds.find("main")
                : t.main.backgrounds.begin();
        texId = getTexture(t.rootDir / it->second);
    }

    ImVec2 size = beginPreviewOverlay(texId, "##mainOverlay");

    // Semi-transparent button style
    ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.10f, 0.10f, 0.10f, 0.75f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.25f, 0.25f, 0.25f, 0.85f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.40f, 0.40f, 0.40f, 0.90f));

    const float btnW   = size.x * 0.45f;
    const float btnH   = 26.f;
    const float gap    = 5.f;
    const int   count  = (int)t.main.buttons.size();
    const float totalH = count * (btnH + gap) - gap;
    float       y      = (size.y - totalH) * 0.38f;  // slightly above center
    const float x      = (size.x - btnW) * 0.5f;

    for (auto& [key, label] : t.main.buttons)
    {
        ImGui::SetCursorPos(ImVec2(x, y));
        ImGui::Button(label.c_str(), ImVec2(btnW, btnH));
        y += btnH + gap;
    }

    ImGui::PopStyleColor(3);

    // Theme name watermark (bottom-left)
    ImGui::SetCursorPos(ImVec2(6.f, size.y - ImGui::GetTextLineHeight() - 4.f));
    ImGui::TextDisabled("%s  v%s", t.name.c_str(), t.version.c_str());

    ImGui::EndChild();
}

void App::renderPreviewSplash()
{
    auto& t = *m_theme;

    unsigned texId = 0;
    if (!t.rootDir.empty() && !t.splash.backgrounds.empty())
    {
        auto it = t.splash.backgrounds.count("splash")
                ? t.splash.backgrounds.find("splash")
                : t.splash.backgrounds.begin();
        texId = getTexture(t.rootDir / it->second);
    }

    ImVec2 size = beginPreviewOverlay(texId, "##splashOverlay");

    if (!t.splash.texts.empty())
    {
        // Cycle through texts every 2 seconds
        int idx = static_cast<int>(ImGui::GetTime() * 0.5) % (int)t.splash.texts.size();
        const auto& txt = t.splash.texts[idx];

        ImVec2 ts = ImGui::CalcTextSize(txt.c_str());
        ImGui::SetCursorPos(ImVec2((size.x - ts.x) * 0.5f, (size.y - ts.y) * 0.5f));
        ImGui::TextUnformatted(txt.c_str());
    }

    ImGui::EndChild();
}

// ─────────────────────────────────────────────────────────────────────────────
//  File operations
// ─────────────────────────────────────────────────────────────────────────────

void App::actionNew()
{
    clearTextures();
    m_theme = Theme::createDefault();
    applyThemeToEditor();
    m_statusMsg = "New theme created";
}

void App::actionOpen()
{
    nfdchar_t* outPath = nullptr;
    nfdfilteritem_t filters[] = {{"Theme JSON", "json"}};

    if (NFD_OpenDialog(&outPath, filters, 1, nullptr) == NFD_OKAY)
    {
        loadTheme(outPath);
        NFD_FreePath(outPath);
    }
}

void App::actionSave()
{
    if (!m_theme) return;

    if (m_theme->jsonPath.empty())
    {
        actionSaveAs();
        return;
    }

    // Sync theme struct from JSON tab, then save the raw text
    if (!m_tabs.empty())
    {
        m_theme->fromJsonString(m_tabs[0].getContent());

        std::ofstream f(m_theme->jsonPath);
        if (f.is_open())
        {
            f << m_tabs[0].getContent();
            m_tabs[0].dirty = false;
        }
    }

    // Save open CSS tabs
    for (int i = 1; i < (int)m_tabs.size(); i++)
    {
        auto& tab = m_tabs[i];
        if (!tab.dirty || tab.filePath.empty()) continue;

        std::filesystem::create_directories(tab.filePath.parent_path());
        std::ofstream f(tab.filePath);
        if (f.is_open())
        {
            f << tab.getContent();
            tab.dirty = false;
        }
    }

    m_statusMsg = "Saved: " + m_theme->jsonPath.filename().string();
}

void App::actionSaveAs()
{
    if (!m_theme) return;

    nfdchar_t* savePath = nullptr;
    nfdfilteritem_t filters[] = {{"Theme JSON", "json"}};

    if (NFD_SaveDialog(&savePath, filters, 1, nullptr, "theme.json") == NFD_OKAY)
    {
        m_theme->jsonPath = savePath;
        m_theme->rootDir  = m_theme->jsonPath.parent_path();
        NFD_FreePath(savePath);

        // Update JSON tab file path
        if (!m_tabs.empty())
            m_tabs[0].filePath = m_theme->jsonPath;

        actionSave();
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Theme / tab management
// ─────────────────────────────────────────────────────────────────────────────

void App::loadTheme(const std::filesystem::path& path)
{
    Theme t;
    if (!t.load(path))
    {
        m_statusMsg = "Error: could not load " + path.filename().string();
        return;
    }
    clearTextures();
    m_theme     = std::move(t);
    m_statusMsg = "Opened: " + m_theme->jsonPath.filename().string();
    applyThemeToEditor();
}

void App::applyThemeToEditor()
{
    m_tabs.clear();
    m_activeTab    = 0;
    m_pendingFocus = -1;

    if (!m_theme) return;

    // Tab 0 — theme.json
    EditorTab jsonTab;
    jsonTab.label    = "theme.json";
    jsonTab.filePath = m_theme->jsonPath;
    jsonTab.setContent(m_theme->toJsonString());
    m_tabs.push_back(std::move(jsonTab));

    // CSS tabs (load from disk if possible)
    auto addCssTabs = [&](const std::map<std::string, std::string>& cssMap)
    {
        for (auto& [id, relPath] : cssMap)
        {
            if (m_theme->rootDir.empty()) continue;
            auto absPath = m_theme->rootDir / relPath;

            // Skip duplicates
            bool dup = false;
            for (auto& tab : m_tabs)
                if (tab.filePath == absPath) { dup = true; break; }
            if (dup) continue;

            EditorTab cssTab;
            cssTab.label    = absPath.filename().string();
            cssTab.filePath = absPath;

            std::ifstream f(absPath);
            if (f.is_open())
            {
                std::string content(std::istreambuf_iterator<char>(f), {});
                cssTab.setContent(content);
            }
            m_tabs.push_back(std::move(cssTab));
        }
    };

    addCssTabs(m_theme->main.css);
    addCssTabs(m_theme->splash.css);
}

int App::findOrOpenTab(const std::filesystem::path& path, const std::string& label)
{
    for (int i = 0; i < (int)m_tabs.size(); i++)
        if (m_tabs[i].filePath == path) return i;

    EditorTab tab;
    tab.label    = label;
    tab.filePath = path;

    std::ifstream f(path);
    if (f.is_open())
    {
        std::string content(std::istreambuf_iterator<char>(f), {});
        tab.setContent(content);
    }

    m_tabs.push_back(std::move(tab));
    return (int)m_tabs.size() - 1;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Texture management
// ─────────────────────────────────────────────────────────────────────────────

unsigned App::getTexture(const std::filesystem::path& path)
{
    auto it = m_textures.find(path);
    if (it != m_textures.end()) return it->second;

    int w = 0, h = 0, n = 0;
    unsigned char* data = stbi_load(path.string().c_str(), &w, &h, &n, 4);
    if (!data)
    {
        m_textures[path] = 0;
        return 0;
    }

    unsigned tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, data);
    stbi_image_free(data);

    m_textures[path] = tex;
    return tex;
}

void App::clearTextures()
{
    for (auto& [path, tex] : m_textures)
        if (tex) glDeleteTextures(1, &tex);
    m_textures.clear();
}
