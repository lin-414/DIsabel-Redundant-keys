#include "RE/Skyrim.h"
#include "SKSE/SKSE.h"

#include <SimpleIni.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <filesystem>
#include <iterator>
#include <optional>
#include <spdlog/sinks/basic_file_sink.h>
#include <string>
#include <utility>
#include <vector>

namespace logger = SKSE::log;

namespace DisableRedundantKeys
{
    constexpr std::string_view kConsumedEvent = "DisableRedundantKeys_Consumed";

    // User events suppressed during unpaused gameplay. Names must match
    // controlmap.txt exactly; the physical key codes behind them are never
    // touched. The list comes from the optional INI file next to the DLL and
    // defaults to all three supported actions.
    class Config final
    {
    public:
        static Config* GetSingleton()
        {
            static Config singleton;
            return std::addressof(singleton);
        }

        void AddSuppressedEvent(std::string a_name) { suppressed.push_back(std::move(a_name)); }

        [[nodiscard]] const std::vector<std::string>& Suppressed() const noexcept { return suppressed; }

        [[nodiscard]] bool IsSuppressed(const RE::BSFixedString& a_userEvent) const
        {
            return std::any_of(
                suppressed.begin(),
                suppressed.end(),
                [&](const std::string& a_name) { return static_cast<std::string_view>(a_userEvent) == a_name; });
        }

    private:
        std::vector<std::string> suppressed;
    };

    // The configurable actions and the controlmap user event each maps to.
    constexpr std::array<std::pair<std::string_view, std::string_view>, 3> kConfigurableActions = {
        { { "Quicksave", "Quicksave" },
          { "Quickload", "Quickload" },
          { "AutoMove", "Auto-Move" } }
    };

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

    void ApplyConfigFile(const std::filesystem::path& a_path)
    {
        CSimpleIniA ini(/* a_isUtf8 = */ true);
        if (ini.LoadFile(a_path.c_str()) < 0) {
            logger::warn("Failed to load config file '{}'; keeping default suppressions", a_path.string());
            return;
        }

        auto* config = Config::GetSingleton();

        for (const auto& [key, userEvent] : kConfigurableActions) {
            if (ini.GetBoolValue("Suppressions", key.data(), /* a_pDefault = */ true)) {
                config->AddSuppressedEvent(std::string(userEvent));
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
                    [&](const auto& a_action) { return key == a_action.first; });
                if (!known) {
                    logger::warn(
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
                config->AddSuppressedEvent(std::string(item));
                logger::info("Suppressing custom user event '{}'", item);
            }
        }
    }

    void LoadConfig()
    {
        auto* config = Config::GetSingleton();

        const auto configPath = GetConfigPath();
        if (configPath && std::filesystem::exists(*configPath)) {
            logger::info("Loading config file '{}'", configPath->string());
            ApplyConfigFile(*configPath);
        } else {
            logger::info("No config file found; suppressing all default actions");
            for (const auto& [key, userEvent] : kConfigurableActions) {
                config->AddSuppressedEvent(std::string(userEvent));
            }
        }

        const auto& suppressed = config->Suppressed();
        if (suppressed.empty()) {
            logger::info("Suppression is disabled; all actions behave like vanilla");
        } else {
            for (const auto& name : suppressed) {
                logger::info("Suppressed user event: '{}'", name);
            }
        }
    }

    class InputFilter final : public RE::BSTEventSink<RE::InputEvent*>
    {
    public:
        static InputFilter* GetSingleton()
        {
            static InputFilter singleton;
            return std::addressof(singleton);
        }

        RE::BSEventNotifyControl ProcessEvent(
            RE::InputEvent* const* eventPtr,
            RE::BSTEventSource<RE::InputEvent*>*) override
        {
            if (!eventPtr) {
                return RE::BSEventNotifyControl::kContinue;
            }

            // Do not alter menu input. Pausing UI menus and the mods behind
            // them must keep receiving the vanilla user events, including the
            // suppressed ones, so in-menu controls keep working.
            if (auto* ui = RE::UI::GetSingleton(); ui && ui->GameIsPaused()) {
                return RE::BSEventNotifyControl::kContinue;
            }

            for (auto* event = *eventPtr; event; event = event->next) {
                if (event->eventType != RE::INPUT_EVENT_TYPE::kButton) {
                    continue;
                }

                auto* button = event->AsButtonEvent();
                if (!button) {
                    continue;
                }

                if (Config::GetSingleton()->IsSuppressed(button->QUserEvent())) {
                    // Neutralize only this transient gameplay input event. The
                    // ControlMap entries themselves are left untouched, so UI
                    // mods can still query these bindings and the physical keys
                    // remain available to hotkey mods that inspect the key code
                    // directly.
                    button->SetUserEvent(kConsumedEvent.data());
                }
            }

            return RE::BSEventNotifyControl::kContinue;
        }
    };

    void RegisterInputFilter()
    {
        auto* input = RE::BSInputDeviceManager::GetSingleton();
        if (!input) {
            logger::error("BSInputDeviceManager singleton is unavailable");
            return;
        }

        auto* filter = InputFilter::GetSingleton();
        input->AddEventSink(filter);

        // AddEventSink appends sinks. Skyrim's native PlayerControls/MenuControls
        // are already registered by kInputLoaded, so move our filter to the front
        // while preserving the relative order of every other sink. This ensures
        // the suppressed user events are neutralized before native handlers see
        // them.
        auto* source = static_cast<RE::BSTEventSource<RE::InputEvent*>*>(input);
        {
            RE::BSSpinLockGuard lock(source->lock);
            auto it = std::find(source->sinks.begin(), source->sinks.end(), filter);
            if (it != source->sinks.end() && it != source->sinks.begin()) {
                std::rotate(source->sinks.begin(), it, std::next(it));
            }
        }

        logger::info("Input filter registered at highest input-sink priority");
    }

    void MessageHandler(SKSE::MessagingInterface::Message* message)
    {
        if (!message) {
            return;
        }

        if (message->type == SKSE::MessagingInterface::kInputLoaded) {
            RegisterInputFilter();
        }
    }
}

SKSEPluginLoad(const SKSE::LoadInterface* skse)
{
    SKSE::Init(skse);

    auto path = logger::log_directory();
    if (path) {
        *path /= "Disable Redundant keys.log";
        auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(path->string(), true);
        auto log = std::make_shared<spdlog::logger>(
            "global log",
            std::move(sink)
        );
        log->set_level(spdlog::level::info);
        log->flush_on(spdlog::level::info);
        spdlog::set_default_logger(std::move(log));
    }

    logger::info("Disable Redundant keys v{} loading", DISABLE_REDUNDANT_KEYS_VERSION);

    DisableRedundantKeys::LoadConfig();

    auto* messaging = SKSE::GetMessagingInterface();
    if (!messaging || !messaging->RegisterListener(DisableRedundantKeys::MessageHandler)) {
        logger::critical("Failed to register SKSE messaging listener");
        return false;
    }

    return true;
}
