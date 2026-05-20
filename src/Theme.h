#pragma once
#include <expected>
#include <filesystem>
#include <string>

// Палитра — 23 поля, точно соответствует формату Naleystogramm
struct ThemePalette {
    // Фоны
    std::string bg           = "#080810";
    std::string bgSurface    = "#0e0e1c";
    std::string bgElevated   = "#13132a";
    std::string bgInput      = "#18182e";
    std::string bgBubbleOut  = "#2a1f5e";
    std::string bgBubbleIn   = "#161628";
    // Границы
    std::string border       = "#1e1e3a";
    std::string borderFocus  = "#6c5ce7";
    // Текст
    std::string textPrimary   = "#ece9ff";
    std::string textSecondary = "#b8b4d8";
    std::string textMuted     = "#5a5880";
    std::string textOnAccent  = "#ffffff";
    // Акцент
    std::string accent        = "#6c5ce7";
    std::string accentHover   = "#8677ff";
    std::string accentPressed = "#5449c4";
    // Статус
    std::string online  = "#00cba9";
    std::string offline = "#3a3a5c";
    std::string danger  = "#ff4d6d";
    std::string success = "#00cba9";
    // Баннер
    std::string bannerBg       = "#1e1b3a";
    std::string bannerBorder   = "#6c5ce7";
    std::string bannerText     = "#e2e2f0";
    std::string bannerBtnHover = "#8075e5";
};

struct Theme {
    std::string  name    = "NewTheme";
    std::string  author;
    ThemePalette palette;
    std::string  customCss;   // содержимое css/main.css (пусто — нет переопределения)

    std::filesystem::path rootDir;
    std::filesystem::path jsonPath;

    static Theme createDefault() { return {}; }

    // C++23: возвращает тему или строку ошибки
    [[nodiscard]] static std::expected<Theme, std::string>
        open(const std::filesystem::path& path);

    [[nodiscard]] std::expected<void, std::string> save() const;

    [[nodiscard]] std::string toJsonString() const;
    bool fromJsonString(const std::string& s);
};
