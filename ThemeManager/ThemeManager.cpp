// ThemeManager.cpp
#include "ThemeManager.h"
#include <imgui.h>

#ifdef _WIN32
#include <windows.h>
#include <winreg.h>
#elif __APPLE__
#include <CoreFoundation/CoreFoundation.h>
#elif __linux__
#include <cstdlib>
#include <fstream>
#endif

namespace ThemeManager {

    Theme& Theme::getInstance() {
        static Theme instance;
        return instance;
    }

    Theme::Theme()
        : m_currentTheme(ThemeType::Classic) // Default to Classic to match your current setup
        , m_lastSystemDarkMode(true) {
    }

    void Theme::applyTheme(ThemeType theme) {
        m_currentTheme = theme;

        switch (theme) {
        case ThemeType::Dark:
            applyDarkTheme();
            break;
        case ThemeType::Light:
            applyLightTheme();
            break;
        case ThemeType::Classic:
            applyClassicTheme();
            break;
        case ThemeType::System:
            applySystemTheme();
            break;
        }
    }

    void Theme::applyDarkTheme() {
        ImGui::StyleColorsDark();

        ImGuiStyle& style = ImGui::GetStyle();

        // Customize for economic simulator
        style.WindowRounding = 6.0f;
        style.FrameRounding = 4.0f;
        style.GrabRounding = 3.0f;
        style.ScrollbarRounding = 9.0f;
        style.TabRounding = 4.0f;

        ImVec4* colors = style.Colors;

        // Custom dark theme colors optimized for data visualization
        colors[ImGuiCol_WindowBg] = ImVec4(0.06f, 0.06f, 0.06f, 0.94f);
        colors[ImGuiCol_MenuBarBg] = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
        colors[ImGuiCol_Header] = ImVec4(0.26f, 0.59f, 0.98f, 0.31f);
        colors[ImGuiCol_HeaderHovered] = ImVec4(0.26f, 0.59f, 0.98f, 0.80f);
        colors[ImGuiCol_HeaderActive] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
        colors[ImGuiCol_Button] = ImVec4(0.26f, 0.59f, 0.98f, 0.40f);
        colors[ImGuiCol_ButtonHovered] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
        colors[ImGuiCol_ButtonActive] = ImVec4(0.06f, 0.53f, 0.98f, 1.00f);
        colors[ImGuiCol_PlotLines] = ImVec4(0.61f, 0.61f, 0.61f, 1.00f);
        colors[ImGuiCol_PlotLinesHovered] = ImVec4(1.00f, 0.43f, 0.35f, 1.00f);
        colors[ImGuiCol_PlotHistogram] = ImVec4(0.90f, 0.70f, 0.00f, 1.00f);
        colors[ImGuiCol_PlotHistogramHovered] = ImVec4(1.00f, 0.60f, 0.00f, 1.00f);
    }

    void Theme::applyLightTheme() {
        ImGui::StyleColorsLight();

        ImGuiStyle& style = ImGui::GetStyle();

        style.WindowRounding = 6.0f;
        style.FrameRounding = 4.0f;
        style.GrabRounding = 3.0f;
        style.ScrollbarRounding = 9.0f;
        style.TabRounding = 4.0f;

        ImVec4* colors = style.Colors;

        // Custom light theme
        colors[ImGuiCol_WindowBg] = ImVec4(0.94f, 0.94f, 0.94f, 1.00f);
        colors[ImGuiCol_MenuBarBg] = ImVec4(0.86f, 0.86f, 0.86f, 1.00f);
        colors[ImGuiCol_Header] = ImVec4(0.26f, 0.59f, 0.98f, 0.31f);
        colors[ImGuiCol_HeaderHovered] = ImVec4(0.26f, 0.59f, 0.98f, 0.80f);
        colors[ImGuiCol_HeaderActive] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
        colors[ImGuiCol_Button] = ImVec4(0.26f, 0.59f, 0.98f, 0.40f);
        colors[ImGuiCol_ButtonHovered] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
        colors[ImGuiCol_ButtonActive] = ImVec4(0.06f, 0.53f, 0.98f, 1.00f);
    }

    void Theme::applyClassicTheme() {
        ImGui::StyleColorsClassic();

        // Minor adjustments for better readability
        ImGuiStyle& style = ImGui::GetStyle();
        style.WindowRounding = 0.0f;
        style.FrameRounding = 0.0f;
        style.GrabRounding = 0.0f;
    }

    void Theme::applySystemTheme() {
        m_lastSystemDarkMode = isSystemDarkMode();

        if (m_lastSystemDarkMode) {
            applyDarkTheme();
        }
        else {
            applyLightTheme();
        }
    }

    void Theme::refreshSystemTheme() {
        if (m_currentTheme == ThemeType::System) {
            bool currentSystemDarkMode = isSystemDarkMode();
            if (currentSystemDarkMode != m_lastSystemDarkMode) {
                applySystemTheme();
            }
        }
    }

    bool Theme::isDarkMode() const {
        switch (m_currentTheme) {
        case ThemeType::Dark:
            return true;
        case ThemeType::Light:
            return false;
        case ThemeType::Classic:
            return false; // Classic is considered light
        case ThemeType::System:
            return m_lastSystemDarkMode;
        default:
            return false;
        }
    }

    bool Theme::isSystemDarkMode() const {
#ifdef _WIN32
        HKEY hKey;
        DWORD value = 1; // Default to light
        DWORD size = sizeof(DWORD);

        if (RegOpenKeyExW(HKEY_CURRENT_USER,
            L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
            0, KEY_READ, &hKey) == ERROR_SUCCESS) {

            RegQueryValueExW(hKey, L"AppsUseLightTheme", nullptr, nullptr,
                (LPBYTE)&value, &size);
            RegCloseKey(hKey);
        }

        return value == 0; // 0 means dark mode
#else
        // Default to dark mode on non-Windows platforms
        return true;
#endif
    }

    const char* Theme::getThemeName(ThemeType theme) const {
        switch (theme) {
        case ThemeType::Dark:    return "Dark";
        case ThemeType::Light:   return "Light";
        case ThemeType::Classic: return "Classic";
        case ThemeType::System:  return "System";
        default: return "Unknown";
        }
    }

    std::string Theme::themeToString(ThemeType theme) const {
        switch (theme) {
        case ThemeType::Dark:    return "dark";
        case ThemeType::Light:   return "light";
        case ThemeType::Classic: return "classic";
        case ThemeType::System:  return "system";
        default: return "classic";
        }
    }

    ThemeType Theme::stringToTheme(const std::string& themeStr) const {
        if (themeStr == "dark") return ThemeType::Dark;
        if (themeStr == "light") return ThemeType::Light;
        if (themeStr == "system") return ThemeType::System;
        return ThemeType::Classic; // Default
    }

} // namespace ThemeManager