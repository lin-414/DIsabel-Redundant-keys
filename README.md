# Disable Redundant keys

A lightweight SKSE plugin for **Skyrim Special Edition / Anniversary Edition** that disables **Quick Save**, **Quick Load**, and the **Auto-Move** toggle during normal gameplay, while preserving all vanilla key mappings.

The plugin does **not** replace `controlmap.txt`, does not require an ESP, and does not store persistent data in your save.

## Features

- Prevents the native **Quicksave**, **Quickload**, and **Auto-Move** actions from reaching Skyrim's gameplay handlers.
- Preserves all of Skyrim's control mappings instead of unmapping them.
- Keeps the default key bindings available to UI mods such as RaceMenu that query the vanilla bindings.
- Leaves the physical key codes intact for other SKSE hotkey mods.
- Uses CommonLib/SKSE event APIs only; no executable code hook or trampoline is installed.
- Does not replace or patch `controlmap.txt`.
- Does not affect sleeping in beds.
- No ESP or ESL.
- No Papyrus scripts.
- No persistent save-game data.
- Safe to uninstall.

## How it works

The plugin never removes a mapping from `RE::ControlMap`, and it does not wait for a menu to open and then close it. It filters the transient input event itself:

1. The plugin registers an input sink when SKSE reports `kInputLoaded`.
2. It moves that sink to the front of `BSInputDeviceManager`'s sink list while preserving the relative order of all existing sinks.
3. During normal unpaused gameplay, if an input event carries one of the suppressed vanilla user events listed below, only that transient event is renamed before Skyrim's native handlers receive it.
4. The physical key codes and the `ControlMap` bindings are not modified.
5. While a pausing UI menu is active, the plugin leaves all user events untouched so UI mods such as RaceMenu can continue to use them.

The suppressed user events (names match `controlmap.txt` exactly):

| Key | User event | Vanilla action |
| --- | --- | --- |
| `F5` | `Quicksave` | Quick save |
| `F9` | `Quickload` | Quick load |
| `C` | `Auto-Move` | Auto-move toggle |

Because suppression is keyed on the user event name rather than the key code, the blocking follows the game's controls even if the player remaps these actions to other keys. Every other vanilla action — Wait, Map, Magic, Items, Skill Tree, Journal, and the rest — passes through untouched.

## Requirements

- Skyrim Special Edition / Anniversary Edition (a VR runtime layout is also compiled in, but untested)
- SKSE64 matching the game version (for Skyrim AE 1.7.99, SKSE 2.3.0 or newer)
- Address Library for SKSE Plugins

The project is built against the maintained **[CommonLibSSE-NG](https://github.com/alandtse/CommonLibSSE-NG)** fork (v7.5.4). A single DLL targets SE, AE, and VR runtimes, including AE before and after 1.6.629 and the 1.7.99 update. CMake fetches it from source at configure time, so building requires network access.

## Installation

Install the compiled release archive with Vortex or another mod manager.

The release package contains:

```text
SKSE/
└── Plugins/
    └── Disable Redundant keys.dll
```

Enable the mod and launch Skyrim through SKSE.

## Testing

After loading a save:

1. Press `F5` and `F9` during normal gameplay and confirm that nothing is saved or loaded.
2. Press `C` during normal gameplay and confirm that auto-move does not toggle.
3. Confirm that another mod using the physical keys can still react to them.
4. Confirm that other vanilla keys such as `T`, `M`, `P`, `I`, and `J` still work normally.
5. Open RaceMenu and verify that **Choose Texture** still uses the vanilla Wait-bound key.
6. Activate a bed and confirm that sleeping still works normally.

The plugin log is written to:

```text
Documents/My Games/Skyrim Special Edition/SKSE/Disable Redundant keys.log
```

## Building from source

### Requirements

- Visual Studio with the C++ desktop workload
- CMake
- vcpkg
- CommonLibSSE-NG

Set the `VCPKG_ROOT` environment variable to your vcpkg installation, then run:

```bat
build_release.bat
```

The build script reads the project version from `VERSION`, configures CMake, compiles the plugin, stages the Vortex package under `package/`, and creates the versioned release archive under `dist/`.

For the current version, the output is:

```text
package/
└── SKSE/
    └── Plugins/
        └── Disable Redundant keys.dll

dist/
└── Disable-Redundant-keys-v0.4.1.zip
```

The ZIP itself contains only the deployable `SKSE/` tree.

## Versioning

`VERSION` is the source of truth for the project version.

Current source version: **v0.4.1**

Versioning policy:

- Small fixes and minor maintenance changes increment the third number: `0.3.2` → `0.3.3`.
- Larger feature or behavior changes increment the second number and reset the third to zero: `0.3.3` → `0.4.0`.
- `CMakeLists.txt`, the plugin log version, and the deployment ZIP name derive their version from `VERSION`.
- `README.md` and package metadata must be updated whenever `VERSION` changes.

## Changelog

### 0.4.1

- Migrated to the maintained **[CommonLibSSE-NG](https://github.com/alandtse/CommonLibSSE-NG)** fork (v7.5.4), replacing the unmaintained colorglass vcpkg port. The single plugin DLL now also supports Skyrim AE **1.7.99** and the VR runtime layout, in addition to SE 1.5.97 and AE 1.6.629+.
- Dependencies (spdlog, fmt, DirectXTK, ...) now come from the standard vcpkg registry and are linked statically (`x64-windows-static-md`), so the release archive still ships a single plugin DLL.
- No change to the suppression behavior itself.

### 0.4.0

- Renamed the plugin to **Disable Redundant keys** and moved to a standalone repository.
- Narrowed suppression to three actions: **Quicksave** (F5), **Quickload** (F9), and **Auto-Move** (C). Wait, Map, Magic, Items, Skill Tree, and Journal behave exactly like vanilla again.
- Suppression follows exact `controlmap.txt` user event names, so it tracks remapped controls; physical keys and `ControlMap` stay untouched, and pausing menus receive every user event.

### 0.3.3

- Replaced the delayed Sleep/Wait menu-closing approach with input-event filtering.
- Moves the input sink ahead of Skyrim's native input handlers.
- Neutralizes only the transient gameplay `Wait` user event.
- Preserves the physical key code and the `ControlMap` mapping for RaceMenu and other hotkey mods.

### 0.3.2

- Removed the low-level `UIMessageQueue::AddMessage` branch hook that could crash during SKSE plugin startup.
- Replaced the hook with CommonLib/SKSE input and menu event sinks.
- Preserved the vanilla `Wait` mapping for RaceMenu compatibility.

### 0.3.1

- Fixed the CommonLibSSE-NG 3.5.3 hook build by using the SKSE trampoline API for the branch hook.

### 0.3.0

- Reworked Wait suppression so the vanilla `Wait` mapping is preserved.
- Added RaceMenu compatibility for actions that query Skyrim's Wait binding, including **Choose Texture**.

## Project structure

```text
Disable Redundant keys/
├── .gitignore
├── LICENSE
├── README.md
├── VERSION
├── CMakeLists.txt
├── vcpkg.json
├── vcpkg-configuration.json
├── build_release.bat
└── src/
    ├── PCH.h
    └── plugin.cpp
```

Generated build artifacts are kept out of source control:

```text
build/
package/
dist/
```

## Compatibility

The plugin is intended for Skyrim SE/AE runtimes supported by CommonLibSSE-NG.

Because it preserves Skyrim's control mapping and filters only the transient gameplay input events, it is less invasive than replacing `controlmap.txt`, removing key bindings entirely, or patching executable code.

Note for other SKSE hotkey mods: while no pausing menu is open, the suppressed user events arrive renamed to `DisableRedundantKeys_Consumed`. Mods that read the physical key codes directly are unaffected.

### RaceMenu

Wait is not filtered, so RaceMenu's **Choose Texture** shortcut works exactly like vanilla.

## Uninstallation

Disable or remove the mod and restart Skyrim.

The plugin does not persistently modify Skyrim's controls or save data. No save cleaning is required.

## Credits

This project is a continuation of **[Disable Wait Runtime](https://github.com/Caelvanost/DisableWaitRuntime)** by **Caelvanost** (Angélique Gaillard), who introduced the input-event filtering approach used here and maintained versions 0.2.0 through 0.3.3. All credit for the core idea and implementation belongs to the original author; this repository narrows its scope and carries it forward under the same MIT License.

## License

This project is licensed under the **MIT License**. See [LICENSE](LICENSE).
