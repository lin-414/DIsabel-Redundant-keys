// Offline unit tests for the config layer. These exercise the INI parsing
// and suppression matching without loading the game runtime; the engine-side
// integration (input sinks, BSFixedString) still needs an in-game check.

#include "Config.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace
{
    int failures = 0;

    void Check(bool a_ok, const std::string& a_what)
    {
        std::cout << (a_ok ? "[PASS] " : "[FAIL] ") << a_what << "\n";
        if (!a_ok) {
            ++failures;
        }
    }

    bool Contains(const std::vector<std::string>& a_list, const std::string& a_name)
    {
        return std::find(a_list.begin(), a_list.end(), a_name) != a_list.end();
    }

    // Writes the given contents to the location LoadConfig() looks at, so the
    // tests exercise the exact file handling the plugin performs in game.
    void WriteConfig(const std::filesystem::path& a_path, const std::string& a_contents)
    {
        std::ofstream file(a_path, std::ios::binary);
        file << a_contents;
    }
}

int main()
{
    const auto configPath = DisableRedundantKeys::GetConfigPath();
    if (!configPath) {
        std::cout << "[FAIL] could not determine the config file path\n";
        return 1;
    }
    auto& config = *DisableRedundantKeys::Config::GetSingleton();

    // 1. Missing config file -> the three defaults.
    std::filesystem::remove(*configPath);
    config.Reset();
    DisableRedundantKeys::LoadConfig();
    Check(
        config.Suppressed().size() == 3 && Contains(config.Suppressed(), "quicksave") &&
            Contains(config.Suppressed(), "quickload") && Contains(config.Suppressed(), "auto-move"),
        "missing config file falls back to the three defaults");

    // 2. Suppression switches.
    config.Reset();
    WriteConfig(*configPath, "[Suppressions]\nQuicksave=0\nQuickload=0\nAutoMove=1\n");
    Check(DisableRedundantKeys::ApplyConfigFile(*configPath), "well-formed config parses");
    Check(config.Suppressed().size() == 1 && Contains(config.Suppressed(), "auto-move"),
          "Quicksave=0 and Quickload=0 leave only auto-move");
    Check(config.IsSuppressed("Auto-Move"), "auto-move is suppressed");
    Check(!config.IsSuppressed("Quicksave"), "quicksave is not suppressed");

    // 3. Custom events: case-insensitive matching, whitespace trimming.
    config.Reset();
    WriteConfig(*configPath, "[Suppressions]\nAutoMove=0\n\n[Custom]\nCustomEvents= toggle always run ,Quick Map\n");
    Check(DisableRedundantKeys::ApplyConfigFile(*configPath), "custom-event config parses");
    Check(config.IsSuppressed("Toggle Always Run"), "custom event matches mixed casing");
    Check(config.IsSuppressed("TOGGLE ALWAYS RUN"), "custom event matches upper casing");
    Check(config.IsSuppressed("toggle always run"), "custom event matches lower casing");
    Check(config.IsSuppressed("Quick Map"), "second custom event matches");
    Check(!config.IsSuppressed("Toggle  Always Run"), "double internal spaces do not match");
    Check(!config.IsSuppressed("Auto-Move"), "AutoMove=0 disables the default entry");
    Check(!config.IsSuppressed("Jump"), "unrelated events are unaffected");

    // 4. Matching boundaries.
    config.Reset();
    WriteConfig(*configPath, "[Suppressions]\nQuicksave=1\n");
    Check(DisableRedundantKeys::ApplyConfigFile(*configPath), "minimal config parses");
    Check(config.IsSuppressed("Quicksave"), "exact name matches");
    Check(config.IsSuppressed("quicksave"), "lowercased name matches");
    Check(!config.IsSuppressed("Quicksav"), "prefix does not match");
    Check(!config.IsSuppressed("Quicksave "), "trailing space does not match");
    Check(config.IsSuppressed("Quickload"), "omitted key defaults to enabled");
    Check(config.IsSuppressed("Auto-Move"), "second omitted key defaults to enabled");

    // 5. Unknown keys are tolerated.
    config.Reset();
    WriteConfig(*configPath, "[Suppressions]\nQuicksave=1\nQuickSave=1\nTypo=0\n");
    Check(DisableRedundantKeys::ApplyConfigFile(*configPath), "unknown key does not break parsing");
    Check(config.IsSuppressed("Quicksave"), "explicit key still applies alongside unknown keys");

    // 6. Empty file -> defaults via GetBoolValue defaults.
    config.Reset();
    WriteConfig(*configPath, "");
    Check(DisableRedundantKeys::ApplyConfigFile(*configPath), "empty file parses");
    Check(config.Suppressed().size() == 3, "empty config file yields the three defaults");

    // 7. Garbage content: SimpleIni detects the UTF-16 BOM and parses the
    //    remainder as an ini without sections, so every suppression key
    //    falls back to enabled. Either way the defaults must survive.
    config.Reset();
    WriteConfig(*configPath, "\xFF\xFEQ\x00u\x00i\x00c\x00k\x00");
    Check(DisableRedundantKeys::ApplyConfigFile(*configPath), "UTF-16 content is BOM-detected and parses");
    Check(config.Suppressed().size() == 3, "garbage content still yields the three defaults");

    // 8. A missing file is reported as unloadable and adds nothing.
    config.Reset();
    Check(!DisableRedundantKeys::ApplyConfigFile(configPath->parent_path() / L"does-not-exist.ini"),
          "missing file is reported as unloadable");
    Check(config.Suppressed().empty(), "a failed parse adds nothing on its own");

    // 9. LoadConfig regression: an unreadable/unparseable config file must
    //    end with the defaults (this used to disable all suppression
    //    silently).
    config.Reset();
    DisableRedundantKeys::LoadConfig();
    Check(config.Suppressed().size() == 3, "unparseable config file falls back to defaults in LoadConfig");

    // 10. Multiple comma-separated custom events.
    config.Reset();
    WriteConfig(*configPath, "[Custom]\nCustomEvents=Wait,Journal,Sneak\n");
    Check(DisableRedundantKeys::ApplyConfigFile(*configPath), "multi custom event config parses");
    Check(config.IsSuppressed("WAIT") && config.IsSuppressed("journal") && config.IsSuppressed("Sneak"),
          "comma-separated custom events all apply");

    std::filesystem::remove(*configPath);

    if (failures == 0) {
        std::cout << "\nAll tests passed.\n";
        return 0;
    }
    std::cout << "\n" << failures << " test(s) failed.\n";
    return 1;
}
