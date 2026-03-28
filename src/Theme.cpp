#include "Theme.h"
#include <nlohmann/json.hpp>
#include <fstream>

using json = nlohmann::json;

// ─────────────────────────────────────────────────────────────────────────────
//  Helpers
// ─────────────────────────────────────────────────────────────────────────────

static json themeToJson(const Theme& t)
{
    json j;
    j["name"]        = t.name;
    if (!t.author.empty())      j["author"]      = t.author;
    if (!t.description.empty()) j["description"] = t.description;
    j["version"] = t.version;

    j["main"]["buttons"]     = t.main.buttons;
    if (!t.main.layout.sidebar_position.empty() || !t.main.layout.play_button_align.empty())
    {
        if (!t.main.layout.sidebar_position.empty())
            j["main"]["layout"]["sidebar_position"]  = t.main.layout.sidebar_position;
        if (!t.main.layout.play_button_align.empty())
            j["main"]["layout"]["play_button_align"] = t.main.layout.play_button_align;
    }
    j["main"]["backgrounds"] = t.main.backgrounds;
    j["main"]["css"]         = t.main.css;

    j["splash"]["texts"]       = t.splash.texts;
    j["splash"]["backgrounds"] = t.splash.backgrounds;
    j["splash"]["css"]         = t.splash.css;

    return j;
}

static void jsonToTheme(const json& j, Theme& t)
{
    t.name        = j.value("name",        "");
    t.author      = j.value("author",      "");
    t.description = j.value("description", "");
    t.version     = j.value("version",     "");

    if (j.contains("main"))
    {
        auto& m = j.at("main");
        if (m.contains("buttons"))     t.main.buttons     = m["buttons"]    .get<decltype(t.main.buttons)>();
        if (m.contains("backgrounds")) t.main.backgrounds = m["backgrounds"].get<decltype(t.main.backgrounds)>();
        if (m.contains("css"))         t.main.css         = m["css"]        .get<decltype(t.main.css)>();
        if (m.contains("layout"))
        {
            auto& l = m["layout"];
            t.main.layout.sidebar_position  = l.value("sidebar_position",  "");
            t.main.layout.play_button_align = l.value("play_button_align", "");
        }
    }

    if (j.contains("splash"))
    {
        auto& s = j.at("splash");
        if (s.contains("texts"))       t.splash.texts       = s["texts"]      .get<decltype(t.splash.texts)>();
        if (s.contains("backgrounds")) t.splash.backgrounds = s["backgrounds"].get<decltype(t.splash.backgrounds)>();
        if (s.contains("css"))         t.splash.css         = s["css"]        .get<decltype(t.splash.css)>();
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Public API
// ─────────────────────────────────────────────────────────────────────────────

Theme Theme::createDefault()
{
    Theme t;
    t.name        = "NewTheme";
    t.author      = "";
    t.description = "";
    t.version     = "1.0.0";

    t.main.buttons = {
        {"play",         "Play"},
        {"settings",     "Settings"},
        {"login",        "Login"},
        {"successfully", "Success"},
        {"logs",         "Logs"},
        {"themes",       "Themes"},
    };
    t.main.layout.sidebar_position  = "left";
    t.main.layout.play_button_align = "stretch";
    t.main.backgrounds = {{"main",   "./backgrounds/bg_main.png"}};
    t.main.css         = {{"main",   "./css/main.css"}};

    t.splash.texts       = {"Welcome!", "Loading..."};
    t.splash.backgrounds = {{"splash", "./backgrounds/bg_splash.png"}};
    t.splash.css         = {{"splash", "./css/splash.css"}};

    return t;
}

bool Theme::load(const std::filesystem::path& path)
{
    std::ifstream f(path);
    if (!f.is_open()) return false;
    try {
        auto j = json::parse(f);
        jsonToTheme(j, *this);
        jsonPath = path;
        rootDir  = path.parent_path();
        return true;
    } catch (...) {
        return false;
    }
}

bool Theme::save() const
{
    return const_cast<Theme*>(this)->saveAs(jsonPath);
}

bool Theme::saveAs(const std::filesystem::path& path)
{
    std::ofstream f(path);
    if (!f.is_open()) return false;
    f << themeToJson(*this).dump(4);
    jsonPath = path;
    rootDir  = path.parent_path();
    return true;
}

std::string Theme::toJsonString() const
{
    return themeToJson(*this).dump(4);
}

bool Theme::fromJsonString(const std::string& s)
{
    try {
        auto j = json::parse(s);
        jsonToTheme(j, *this);
        return true;
    } catch (...) {
        return false;
    }
}
