#include "Theme.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <iterator>

using json = nlohmann::json;

// ─────────────────────────────────────────────────────────────────────────────

static json paletteToJson(const ThemePalette& p)
{
    json j;
    j["bg"]           = p.bg;
    j["bgSurface"]    = p.bgSurface;
    j["bgElevated"]   = p.bgElevated;
    j["bgInput"]      = p.bgInput;
    j["bgBubbleOut"]  = p.bgBubbleOut;
    j["bgBubbleIn"]   = p.bgBubbleIn;
    j["border"]       = p.border;
    j["borderFocus"]  = p.borderFocus;
    j["textPrimary"]  = p.textPrimary;
    j["textSecondary"]= p.textSecondary;
    j["textMuted"]    = p.textMuted;
    j["textOnAccent"] = p.textOnAccent;
    j["accent"]       = p.accent;
    j["accentHover"]  = p.accentHover;
    j["accentPressed"]= p.accentPressed;
    j["online"]       = p.online;
    j["offline"]      = p.offline;
    j["danger"]       = p.danger;
    j["success"]      = p.success;
    j["bannerBg"]       = p.bannerBg;
    j["bannerBorder"]   = p.bannerBorder;
    j["bannerText"]     = p.bannerText;
    j["bannerBtnHover"] = p.bannerBtnHover;
    return j;
}

static void jsonToPalette(const json& j, ThemePalette& p)
{
    auto get = [&](const char* key, std::string& out) {
        if (j.contains(key)) out = j[key].get<std::string>();
    };
    get("bg",          p.bg);
    get("bgSurface",   p.bgSurface);
    get("bgElevated",  p.bgElevated);
    get("bgInput",     p.bgInput);
    get("bgBubbleOut", p.bgBubbleOut);
    get("bgBubbleIn",  p.bgBubbleIn);
    get("border",      p.border);
    get("borderFocus", p.borderFocus);
    get("textPrimary",   p.textPrimary);
    get("textSecondary", p.textSecondary);
    get("textMuted",     p.textMuted);
    get("textOnAccent",  p.textOnAccent);
    get("accent",        p.accent);
    get("accentHover",   p.accentHover);
    get("accentPressed", p.accentPressed);
    get("online",  p.online);
    get("offline", p.offline);
    get("danger",  p.danger);
    get("success", p.success);
    get("bannerBg",       p.bannerBg);
    get("bannerBorder",   p.bannerBorder);
    get("bannerText",     p.bannerText);
    get("bannerBtnHover", p.bannerBtnHover);
}

// ─────────────────────────────────────────────────────────────────────────────

std::expected<Theme, std::string>
Theme::open(const std::filesystem::path& path)
{
    std::ifstream f(path);
    if (!f.is_open())
        return std::unexpected("Файл не найден: " + path.string());

    try {
        const auto j = json::parse(f);
        Theme t;
        t.name   = j.value("name",   "");
        t.author = j.value("author", "");
        if (j.contains("palette"))
            jsonToPalette(j["palette"], t.palette);

        t.jsonPath = path;
        t.rootDir  = path.parent_path();

        std::ifstream css(t.rootDir / "css" / "main.css");
        if (css.is_open())
            t.customCss = {std::istreambuf_iterator<char>(css), {}};

        return t;
    } catch (const json::exception& e) {
        return std::unexpected(std::string("JSON ошибка: ") + e.what());
    }
}

std::expected<void, std::string> Theme::save() const
{
    if (jsonPath.empty())
        return std::unexpected("Путь не задан");

    std::ofstream f(jsonPath);
    if (!f.is_open())
        return std::unexpected("Не удалось открыть файл на запись");

    f << toJsonString();

    if (!customCss.empty()) {
        const auto cssDir = rootDir / "css";
        std::filesystem::create_directories(cssDir);
        std::ofstream cf(cssDir / "main.css");
        if (!cf.is_open())
            return std::unexpected("Не удалось сохранить css/main.css");
        cf << customCss;
    }
    return {};
}

std::string Theme::toJsonString() const
{
    json j;
    j["name"] = name;
    if (!author.empty()) j["author"] = author;
    j["palette"] = paletteToJson(palette);
    return j.dump(4);
}

bool Theme::fromJsonString(const std::string& s)
{
    try {
        const auto j = json::parse(s);
        name   = j.value("name",   "");
        author = j.value("author", "");
        if (j.contains("palette"))
            jsonToPalette(j["palette"], palette);
        return true;
    } catch (...) { return false; }
}
