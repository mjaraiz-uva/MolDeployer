// ThemeManager.h
#pragma once

#include <string>

namespace ThemeManager {

    enum class ThemeType {
        Dark = 0,
        Light = 1,
        Classic = 2,
        System = 3
    };

    class Theme {
    public:
        static Theme& getInstance();

        // Delete copy/move constructors
        Theme(const Theme&) = delete;
        Theme& operator=(const Theme&) = delete;
        Theme(Theme&&) = delete;
        Theme& operator=(Theme&&) = delete;

        // Theme operations
        void applyTheme(ThemeType theme);
        void applySystemTheme();
        void refreshSystemTheme();

        // Getters
        ThemeType getCurrentTheme() const { return m_currentTheme; }
        const char* getThemeName(ThemeType theme) const;
        const char* getCurrentThemeName() const { return getThemeName(m_currentTheme); }
        bool isDarkMode() const;

        // For configuration
        std::string themeToString(ThemeType theme) const;
        ThemeType stringToTheme(const std::string& themeStr) const;

    private:
        Theme();
        ~Theme() = default;

        void applyDarkTheme();
        void applyLightTheme();
        void applyClassicTheme();
        bool isSystemDarkMode() const;

        ThemeType m_currentTheme;
        bool m_lastSystemDarkMode;
    };

} // namespace ThemeManager