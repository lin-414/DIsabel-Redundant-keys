#include "Config.h"

#include <SimpleIni.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <windows.h>

namespace DisableRedundantKeys
{
    void Config::AddSuppressedEvent(std::string_view a_name)
    {
        std::string lower;
        lower.reserve(a_name.size());
        for (const char c : a_name) {
            lower.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
        }
        suppressed.push_back(std::move(lower));
    }

    void Config::Reset()
    {
        suppressed.clear();
    }

    bool Config::IsSuppressed(std::string_view a_userEvent) const
    {
        return std::any_of(
            suppressed.begin(),
            suppressed.end(),
            [&](const std::string& a_name) {
                if (a_name.size() != a_userEvent.size()) {
                    return false;
                }
                for (std::size_t i = 0; i < a_userEvent.size(); ++i) {
                    if (std::tolower(static_cast<unsigned char>(a_userEvent[i])) !=
                        static_cast<unsigned char>(a_name[i])) {
                        return false;
                    }
                }
                return true;
            });
    }

    std::optional<std::filesystem::path> GetConfigPath()
    {
        HMODULE module = nullptr;
        if (!GetModuleHandleExW(
                GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                reinterpret_cast<LPCWSTR>(&GetConfigPath),
                &module)) {
            return std::nullopt;
        }

        std::array<wchar_t, 4096> buffer{};
        const auto length = GetModuleFileNameW(module, buffer.data(), static_cast<DWORD>(buffer.size()));
        if (length == 0 || length >= buffer.size()) {
            return std::nullopt;
        }

        auto path = std::filesystem::path(std::wstring_view(buffer.data(), length));
        path.replace_extension(L".ini");
        return path;
    }

    namespace
    {
        void AddDefaultSuppressedEvents()
        {
            auto* config = Config::GetSingleton();
            for (const auto& [key, userEvent] : kConfigurableActions) {
                config->AddSuppressedEvent(userEvent);
            }
        }
    }

    bool ApplyConfigFile(const std::filesystem::path& a_path)
    {
        CSimpleIniA ini(/* a_isUtf8 = */ true);
        if (ini.LoadFile(a_path.c_str()) < 0) {
            return false;
        }

        auto* config = Config::GetSingleton();

        for (const auto& [key, userEvent] : kConfigurableActions) {
            if (ini.GetBoolValue("Suppressions", key.data(), /* a_pDefault = */ true)) {
                config->AddSuppressedEvent(userEvent);
            }
        }

        // Flag unknown keys in [Suppressions]; a typo would otherwise silently
        // leave the matching action enabled.
        if (const CSimpleIniA::TKeyVal* entries = ini.GetSection("Suppressions")) {
            for (const auto& entry : *entries) {
                std::string key(entry.first.pItem);
                std::ranges::transform(key, key.begin(), [](unsigned char c) {
                    return static_cast<char>(std::tolower(c));
                });
                const bool known = std::any_of(
                    kConfigurableActions.begin(),
                    kConfigurableActions.end(),
                    [&](const auto& a_action) {
                        const std::string_view known = a_action.first;
                        return key.size() == known.size() &&
                               std::equal(
                                   known.begin(),
                                   known.end(),
                                   key.begin(),
                                   [](char a, char b) {
                                       return std::tolower(static_cast<unsigned char>(a)) ==
                                              std::tolower(static_cast<unsigned char>(b));
                                   });
                    });
                if (!known) {
                    spdlog::warn(
                        "Unknown key '{}' in [Suppressions]; expected Quicksave, Quickload, or AutoMove",
                        entry.first.pItem);
                }
            }
        }

        // Advanced: suppress any additional controlmap user event by name.
        const std::string_view remaining = ini.GetValue("Custom", "CustomEvents", "");
        for (auto token = remaining; !token.empty();) {
            const auto comma = token.find(',');
            auto item = token.substr(0, comma);
            token.remove_prefix((std::min)(item.size() + 1, token.size()));
            while (!item.empty() && (item.front() == ' ' || item.front() == '\t')) {
                item.remove_prefix(1);
            }
            while (!item.empty() && (item.back() == ' ' || item.back() == '\t')) {
                item.remove_suffix(1);
            }
            if (!item.empty()) {
                config->AddSuppressedEvent(item);
                spdlog::info("Suppressing custom user event '{}'", item);
            }
        }

        return true;
    }

    void LoadConfig()
    {
        auto* config = Config::GetSingleton();

        const auto configPath = GetConfigPath();
        std::error_code ec;
        bool loaded = false;
        if (configPath && std::filesystem::exists(*configPath, ec)) {
            spdlog::info("Loading config file '{}'", configPath->string());
            if (ApplyConfigFile(*configPath)) {
                loaded = true;
            } else {
                spdlog::warn(
                    "Failed to load config file '{}'; suppressing all default actions", configPath->string());
            }
        } else if (configPath && !ec) {
            spdlog::info(
                "No config file found at '{}'; suppressing all default actions", configPath->string());
        } else {
            spdlog::warn("Unable to determine the config file location; suppressing all default actions");
        }

        if (!loaded) {
            AddDefaultSuppressedEvents();
        }

        const auto& suppressed = config->Suppressed();
        if (suppressed.empty()) {
            spdlog::info("Suppression is disabled; all actions behave like vanilla");
        } else {
            for (const auto& name : suppressed) {
                spdlog::info("Suppressed user event: '{}'", name);
            }
        }
    }
}
