#pragma once

#include <array>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace DisableRedundantKeys
{
    // The configurable actions and the controlmap user event each maps to.
    inline constexpr std::array<std::pair<std::string_view, std::string_view>, 3> kConfigurableActions = {
        { { "Quicksave", "Quicksave" },
          { "Quickload", "Quickload" },
          { "AutoMove", "Auto-Move" } }
    };

    // User events suppressed during unpaused gameplay. Names must match
    // controlmap.txt (matching is case-insensitive); the physical key codes
    // behind them are never touched. The list comes from the optional INI
    // file next to the DLL and defaults to all three supported actions.
    //
    // This layer is deliberately free of game-engine types so it can be
    // exercised by the offline unit tests.
    class Config final
    {
    public:
        static Config* GetSingleton()
        {
            static Config singleton;
            return std::addressof(singleton);
        }

        // Names are stored lowercased so the lookup below can match
        // case-insensitively; user event names are plain ASCII.
        void AddSuppressedEvent(std::string_view a_name);

        // Test hook: drop every suppression so a test starts from scratch.
        void Reset();

        [[nodiscard]] const std::vector<std::string>& Suppressed() const noexcept { return suppressed; }

        [[nodiscard]] bool IsSuppressed(std::string_view a_userEvent) const;

    private:
        std::vector<std::string> suppressed;
    };

    // Path of the INI file next to the plugin DLL, if it can be determined.
    [[nodiscard]] std::optional<std::filesystem::path> GetConfigPath();

    // Parses the given INI file and adds its suppressions on top of the
    // current state. Returns false when the file exists but cannot be parsed;
    // the caller decides on a fallback in that case.
    bool ApplyConfigFile(const std::filesystem::path& a_path);

    // Reads the config (or the defaults when it is missing or unreadable) and
    // logs the effective suppression list. Safe to call before the game
    // runtime loads.
    void LoadConfig();
}
