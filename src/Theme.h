#pragma once
#include <filesystem>
#include <map>
#include <string>
#include <vector>

struct ThemeLayout {
    std::string sidebar_position;   // "left" | "right"
    std::string play_button_align;  // "stretch" | "center" | …
};

struct ThemeMain {
    std::map<std::string, std::string> buttons;      // id -> label text
    ThemeLayout                        layout;
    std::map<std::string, std::string> backgrounds;  // id -> relative path
    std::map<std::string, std::string> css;          // id -> relative path
};

struct ThemeSplash {
    std::vector<std::string>           texts;
    std::map<std::string, std::string> backgrounds;
    std::map<std::string, std::string> css;
};

struct Theme {
    std::string  name;
    std::string  author;
    std::string  description;
    std::string  version;
    ThemeMain    main;
    ThemeSplash  splash;

    std::filesystem::path rootDir;   // folder containing theme.json
    std::filesystem::path jsonPath;

    static Theme createDefault();

    bool load(const std::filesystem::path& path);
    bool save() const;
    bool saveAs(const std::filesystem::path& path);

    std::string toJsonString() const;
    bool        fromJsonString(const std::string& s);
};
