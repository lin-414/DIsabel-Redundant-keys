## 目标

回应玩家反馈：为插件增加 INI 配置文件支持，让玩家自行决定禁用哪些项（F5/F9/C 任一开关），并能进阶自定义任意用户事件名。版本 0.5.0。

## 实现（全部改动在 [plugin.cpp](D:\Documents\Zcode\DIsabel Redundant keys\src\plugin.cpp) + CMakeLists）

1. **配置文件定位**：通过 `GetModuleHandleExW(FROM_ADDRESS)` + `GetModuleFileNameW` 取插件 DLL 自身路径并替换扩展名——即 `Data/SKSE/Plugins/Disable Redundant keys.ini`。跟随 DLL 名，改名也不失效。在 `SKSEPluginLoad` 里、注册消息监听前调用 `LoadConfig()`。

2. **INI 格式**（SimpleIni 解析，header-only，已在 vcpkg 依赖树中，零新增依赖）：
   ```ini
   [Suppressions]
   ; 1 = 游戏中禁用该操作，0 = 保持原版行为
   Quicksave=1
   Quickload=1
   AutoMove=1

   [Custom]
   ; 进阶：追加任意 controlmap 用户事件名，逗号分隔（如 Wait,Quick Map）
   CustomEvents=
   ```

3. **核心改动**：现有 `kSuppressedEvents` constexpr 数组改为 Config 单例持有的运行时 `std::vector<std::string>`（默认值 = 三项全禁，与 0.4.1 行为一致）；`IsSuppressed()` 改查该 vector；`[Suppressions]` 中未知键名记 warning 日志（防拼写错误静默失效）。

4. **容错与日志**：文件不存在/解析失败 → 用默认值并记日志；启动时逐条输出实际生效的抑制项，全关时明确提示 "suppression disabled"。

5. **CMakeLists**：加 `find_package(simpleini CONFIG REQUIRED)` + 链接 `simpleini::simpleini`。

## 文档与发布

- README：新增 "Configuration" 章节（路径、模板、各项含义、Custom 进阶用法、改动后需重启游戏）；Features 提及可配置；Changelog 加 0.5.0；版本引用同步 v0.5.0
- `VERSION`/`vcpkg.json` → 0.5.0
- 发布包仍只含 DLL（不预置 ini——缺文件即默认行为，README 给模板）
- **正常提交**（不 amend 不强推）、普通 push、打 v0.5.0 tag、发 v0.5.0 release（保留 v0.4.x）
- 构建验证：本机编译通过 + DLL 字符串检查（无法游戏内实测，README 测试章节更新：改 ini 后重启游戏验证对应键恢复原版行为）

## 明确不做

- MCM 菜单集成（需 Papyrus + SkyUI 依赖，后续可加）
- 配置热重载（重启游戏生效，符合 SKSE 生态惯例）