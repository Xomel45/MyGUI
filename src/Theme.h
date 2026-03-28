#pragma once
#include <filesystem>
#include <map>
#include <string>
#include <vector>

struct ThemeMain {
    std::map<std::string, std::string> buttons;      // id -> label text
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
    std::string  description;
    std::string  version;
    std::string  language;
    ThemeMain    main;
    ThemeSplash  splash;

    std::filesystem::path rootDir;   // folder containing theme.json
    std::filesystem::path jsonPath;

    // Returns a new theme with sensible defaults
    static Theme createDefault();

    // Load from a .json file on disk; returns false on error
    bool load(const std::filesystem::path& path);

    // Save JSON to jsonPath (or to a new path via saveAs)
    bool save() const;
    bool saveAs(const std::filesystem::path& path);

    // Serialize / deserialize to/from a JSON string
    std::string toJsonString() const;
    bool        fromJsonString(const std::string& s);
};
