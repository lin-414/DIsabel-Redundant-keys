#include "RE/Skyrim.h"
#include "SKSE/SKSE.h"

#include <algorithm>
#include <array>
#include <iterator>
#include <spdlog/sinks/basic_file_sink.h>

namespace logger = SKSE::log;

namespace DisableRedundantKeys
{
    constexpr std::string_view kConsumedEvent = "DisableRedundantKeys_Consumed";

    // Vanilla ControlMap user events suppressed during unpaused gameplay. The
    // names must match controlmap.txt exactly; the physical key codes behind
    // them (F5, F9, C) are never touched.
    constexpr std::array<std::string_view, 3> kSuppressedEvents = {
        "Quicksave",        // Quick save (F5)
        "Quickload",        // Quick load (F9)
        "Auto-Move",        // Auto-move toggle (C)
    };

    bool IsSuppressed(const RE::BSFixedString& a_userEvent)
    {
        return std::any_of(
            kSuppressedEvents.begin(),
            kSuppressedEvents.end(),
            [&](std::string_view a_name) { return a_userEvent == a_name; });
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

                if (IsSuppressed(button->QUserEvent())) {
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
        for (const auto& suppressedEvent : kSuppressedEvents) {
            logger::info("Suppressed user event: '{}'", suppressedEvent);
        }
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

    auto* messaging = SKSE::GetMessagingInterface();
    if (!messaging || !messaging->RegisterListener(DisableRedundantKeys::MessageHandler)) {
        logger::critical("Failed to register SKSE messaging listener");
        return false;
    }

    return true;
}
