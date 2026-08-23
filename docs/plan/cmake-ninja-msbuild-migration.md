# CMake/Ninja 构建与 MSBuild 去依赖计划

> 状态：执行中  
> 首次建立：2026-08-20  
> 最后更新：2026-08-23  
> 维护要求：每次构建系统变更都必须同步更新本文的状态、证据、阻塞项和变更记录。

> 最新检查点（CP-275）：跨架构本机构建继续验证了此前未覆盖的 CMake 合同。x86 `TerminalControl` 已从 MIDL/WinMD/XAML 到 `Microsoft.Terminal.Control.dll/.winmd` 实际完整链接成功；修复包括 MIDL 的 `x86 -> win32` `/env` 映射、x86 vcpkg 路径、x86 stdcall 的 `_DllMain@12` 链接符号，以及真实可达性告警。重新创建的普通 PowerShell Ninja Multi-Config 图还暴露 `vs_link_dll` 会从 PATH 查找 `rc.exe`；根 CMake 现于 `project()` 前固定 Windows SDK RC 编译器，`OpenConsoleProxy` 实际链接通过，随后完整 `Release full` 复跑成功（foundation 475s、settings adapter 160s、shims 20s、WorkspaceExtension 371s、repack 15s）。`WpfTerminalControlPack --config Release` 现正确解析 Release 路径并在缺少 `bin/arm64/Release/Microsoft.Terminal.Control` 时明确失败，不会伪造包。当前机器 VS 工具集缺少 `atlmfc/lib/arm64`，因此无法本机构建 ARM64；迁移后的 Azure 三架构矩阵仍是完整 WPF 包的最终验证环境。已移除受版本控制的 `build.before-relocation` 旧 Ninja/MSBuild 生成元数据；该目录其余未跟踪文件保留为本机可恢复缓存，不参与版本控制或门禁扫描。

> 最新检查点（CP-274）：在 CP-273 的 PGD 验证后，已恢复默认 None 图并完成最终 `cmake --build .\\build --config Release --target full -- -j 1` 回归。native-product-foundation、settings adapter、native shims、WorkspaceExtension 和 full-repack 全部 exit 0；只保留既有 MakePri PRI263 警告。WPF 的 CMake 双框架目标再次 no-op 成功，Fuzzing `-runs=1`、AuditMode `MidiAudio`、PGO Instrument `TerminalConnection` 和 PGO Optimize `TerminalConnection` 均已有真实证据；活跃入口静态扫描继续为 0。完整 WPF NuGet 包需要 Azure 下载的 x86/x64/arm64 原生产物，已由迁移后的 CI job 负责最终矩阵验证。

> 最新检查点（CP-273）：Optimize 已从“始终拒绝”迁为直接 NuGet/CMake 合同。产品 Azure job 认证后以 Git branch 的 merge-base 时间为上限，从受控 feed 选择同主/次版本、同 branch 的最新 PGODatabase prerelease，精确 `nuget install` 并传入 `PORTABLE_PGO_DATABASE_ROOT`；不会使用无约束 latest。CMake 对七个产品目标逐一验证 `tools/<arch>/<Target>.pgd` 并添加 `/USEPROFILE:PGD=...`、`/LTCG` 和 IPO；Instrument 仍独享 runtime/PGO sweep 部署。本机从该 feed 下载 `1.26.0-2608122315-main` 后，Optimize 图配置成功，`TerminalConnection` 实际重新链接成功；随后已恢复 None 图。产品 pipeline 的 CMake 工作目录已校正为 OpenConsole 子树的上级根，并从根 `bin/msix` 显式发现生成的 MSIX；WPF job 同样使用根 CMake build tree。

> 最新检查点（CP-272）：Fuzzing 的 CMake/Ninja 闭包已真实通过。ASan/coverage 只施加到 `OpenConsoleFuzzer` 及其静态库闭包，避免污染不链接 sanitizer runtime 的 DLL；链接显式加入 `libsancov`、`clang_rt.fuzzer_MT-*` 与 ASan runtime。为保持与未插桩静态 fmt 的 STL ABI 兼容，插桩闭包禁用 string/vector annotation marker，但保留普通 ASan 和 coverage 插桩。构建后直接复制 `clang_rt.asan_dynamic-*.dll`。`cmake --build .\\build --config Fuzzing --target HostFuzzWrapper -- -j 1` 实际完成并生成 `microsoft/bin/x64/Fuzzing/OpenConsoleFuzzer.exe`；`OpenConsoleFuzzer.exe -runs=1` 成功加载 libFuzzer、95016 个 coverage counter 并以 exit 0 完成。

> 最新检查点（CP-271）：根 CMake 现在显式接受 `PORTABLE_PGO_BUILD_MODE`。Instrument 模式只允许 x64/arm64，向原 PGO 七个产品目标加入 `/GENPROFILE`、`/LTCG` 和架构对应 `pgort.lib`，并在每个目标目录复制 `pgort140.dll`/`pgosweep.exe`，因此 PGC/PGD 产出和运行时不再依赖 targets 导入。首次链接揭示 `/GENPROFILE` 必须配合 `/GL`，现已对这七个 PGO 目标设置 CMake IPO。`cmake --build .\\build --config Release --target TerminalConnection -- -j 1` 在 Instrument 模式实际生成 `TerminalConnection.pgd`、DLL、`pgort140.dll` 与 `pgosweep.exe`，随后已恢复 None 图。产品 Azure job 已把参数透传给 CMake；Optimize 之前的始终拒绝已替换为 CP-273 的显式 PGD 包目录输入合同。

> 最新检查点（CP-270）：AuditMode 的首个原生库验证暴露 CppCoreCheck 会对 SDK/WIL 外部头报告 C6553；旧工程通过外部排除不应把该告警变成普通 `/WX` 失败。CMake 现仅在 AuditMode 对可编译目标追加 `/WX-`，保持 `/analyze` 实际运行并让报告完整产出；`cmake --build .\\build --config AuditMode --target MidiAudio -- -j 1` 已重新编译 PCH、实现和 archive 成功。此前 `Release full` 验证不受影响。

> 最新检查点（CP-269）：CP-268 后的根图已重新配置，并完成 `cmake --build .\\build --config Release --target full -- -j 1` 实际回归。Ninja 从 MIDL/WinMD、XAML/XBF、TerminalApp、TestHostApp、Settings Model 到 WorkspaceExtension 重新执行受影响节点，最终 `full-Release` 记录完成；没有 MSBuild/VSBuild/dotnet-build 子命令。WPF `Release` 和 `AuditMode` 双框架目标仍为 no-op 复验通过，活跃 CI/CMake/开发入口静态扫描的 `MSBuild@`、`VSBuild@`、`MSBuild.exe`、`dotnet build/publish/msbuild` 和 `.appxrecipe` 为 0，`git diff --check` 通过。完整 WPF NuGet 包仍需要三架构原生产物，Fuzzing 全图与 PGO Optimize/Instrument 的产品级链接合同继续执行，不能据此宣布 L2 完成。

> 最新检查点（CP-268）：WPF 控件的编译和 NuGet 打包已进入根 CMake/Ninja 图，不再调用 SDK project 或 `VSBuild`。CMake 直接以固定 SDK Roslyn 编译 `net472`/`net8.0-windows` 两套程序集；原本唯一的 XAML 结构以等价的显式 WPF 对象树生成，因而不需要 PresentationBuildTasks/MSBuild。`WpfTerminalControlPack` 用仓库固定 `dep/nuget/nuget.exe` 生成主包与 symbols 包，并显式收录三种架构的 `Microsoft.Terminal.Control` DLL/PDB。已在本机通过 `Release` 与 `AuditMode` 的双框架编译；因本机未准备 x86/ARM64 native artifacts，完整 NuGet 包仅由 Azure 下载三架构产物后的 job 验证。Azure WPF job 和产品 build job 都已替换为 CMake/Ninja；无调用方的 solution-restore 模板已删除。CMake 新增 `AuditMode`、`Fuzzing` 配置名，Audit 启用 `/analyze`，fuzzer 启用 ASan/coverage 参数并保留 `bin/<arch>/<mode>` 布局；Fuzzing 全量链接仍在本机进行中，PGO Instrument/Optimize 的 NuGet helper 合同仍待直接实现。

> 最新检查点（CP-267）：Azure PGO 后处理已去掉独立的 MSBuild/Developer Shell 入口。PGD 合并 job 直接发现每个架构的 MSVC `pgomgr.exe` 并保留原 artifact 目录与 `/merge` 语义；本机 x64 定位和 `pgomgr /?` 已通过。PGO database 发布 job 不再构建 `PGO.DB.proj`，改为从 `custom.props` 的既有主/次版本和合并 PGD 目录直接执行 `nuget pack Terminal.PGO.DB.nuspec`。这两项不替代产品的 PGO Instrument/Optimize 编译合同，后者仍待根 CMake 明确实现。

> 最新检查点（CP-266）：开发者和独立工具入口进一步收口到 CMake/Ninja。`OpenConsole.psm1` 导出的 `Set-NativeBuildEnvironment` 不再调用 VS Developer Shell 或查找 `vswhere`/`VCTargetsPath`，`Invoke-OpenConsoleBuild` 配置根 CMake 并构建 `full`；PowerShell 导入和 x64 环境设置已验证。`razzle.cmd` 只设置仓库/平台变量，`bcz.cmd` 只调用 CMake，并在成功时正确回传 `_LAST_BUILD_CONF`。VS Code 默认构建任务已调用该模块入口。旧 `bx.ps1` 的 solution metaproj 解析已删除，`bx.cmd` 以退出码 2 明确拒绝尚无 CMake 合同的项目级构建。无产品调用方的 ColorTool 保留原 `build.bat rel/clean` 语义，但现在经 `build.ps1` 以 Windows SDK ResGen、SDK Roslyn `csc.dll` 和显式 .NET Framework runtime 引用构建；`cmd /c microsoft\\src\\tools\\ColorTool\\build.bat rel` 已成功。GitHub L1 门禁已扩展至这些开发者入口和 ColorTool，使用同一规则在本地通过。文档的命令行构建路径已改为 CMake/Ninja，失效的 `DeployAppRecipe`/`vswhere` 部署说明已删除。剩余活跃 MSBuild 关键词已精确收敛到 Azure DevOps 的 project/WPF/solution-restore 模板以及 PGO NuGet/PGD 合并任务；普通 CI 测试还读取旧 `.appxrecipe`。这些路径依赖根 CMake 尚未实现的 Audit/Fuzz/PGO、WPF NuGet 打包、测试部署和旧子仓库布局，不能安全地伪装成等价迁移。

> 最新检查点（CP-265）：开发者主构建命令 `microsoft/tools/bcz.cmd` 已由 solution/MSBuild 切换到根 CMake 的 `full`。它使用 `git rev-parse --show-toplevel` 定位仓库，因此从任意工作目录执行不依赖旧 `OPENCON` 路径；`dbg`/`rel` 映射到 Debug/Release，CMake 的增量图取代 `no_clean`。没有等价合同的 AuditMode 与项目级 `exclusive` 明确失败而非静默回落 MSBuild。`cmd /c .\\microsoft\\tools\\bcz.cmd no_clean rel` 已 exit 0，完成原生产品、MSIX、WorkspaceExtension 与 portable 重打包。

> 最新检查点（CP-264）：CMake 现提供受约束的 `PORTABLE_BRANDING`（Dev/Canary/Preview/Release），并映射到与旧 `Branding.targets` 相同的唯一 `WT_BRANDING_*` 宏。配置结束时清除迁移早期目标上遗留的显式 `WT_BRANDING_DEV`，所以其他品牌不会同时定义 Dev。以 `PORTABLE_BRANDING=Release` 配置后，生成的 `ScratchWindowExe` Ninja 规则只含 `WT_BRANDING_RELEASE`；随后已恢复为 Dev 配置，规则只含 `WT_BRANDING_DEV`。这补齐了 Azure Release/Preview/Canary 切换的品牌前提；Audit/Fuzz/PGO/WPF 的专有合同仍待单独迁移。

> 最新检查点（CP-263）：Ninja 的首次配置现在在 `project()` 前按 `PORTABLE_PLATFORM` 发现 MSVC toolset，并选择 `Hostx64/<platform>/cl.exe`；编译器识别阶段只生成静态库，因而不需要 Developer Prompt 的 `INCLUDE`/`LIB`。以 x86 专用目录验证时，CMake 已正确识别 `Hostx64/x86/cl.exe` 和 C/C++ ABI；仓库的单一 build-dir 约束随后按预期拒绝继续配置到其他目录，因此尚不能把它视为 x86 全量构建证据。现有 x64 `cmake -S . -B .\\build` 以及默认 Release `cmake --build .\\build --config Release -- -j 1` 均 exit 0。临时 `build-x86-probe` 已删除。Azure 矩阵切换仍须先解决单一 build 目录与多平台并行 job 的正式策略。

> 最新检查点（CP-262）：GitHub 的 CMake/Ninja workflow 新增 L1 静态门禁，检查根 `CMakeLists.txt` 和 workflow 本身不得重新引入 `MSBuild.exe`、`.vcxproj`、`.wapproj`、`dotnet build/publish/msbuild` 或 `razzle`/`bcz`。门禁在本地以 workflow 中同一 PowerShell 逻辑通过；它刻意不扫描尚待迁移的 Azure DevOps/WPF 模板。独立 `ColorTool/build.bat` 的所有受跟踪调用方扫描为 0，已归入 P8 的独立工具迁移/删除队列，而非产品链阻塞项。

> 最新检查点（CP-261）：仍由 Azure pipeline 调用的 `New-UnpackagedTerminalDistribution.ps1` 已不再查找或调用 VS `MSBuild/Current/Bin/Roslyn/csc.exe`。单文件 portable launcher 现在由固定 .NET SDK 8.0.421 的 `csc.dll` 通过明确的 .NET Framework 4.7.2 reference response file 编译；排除 Roslyn 不接受的 EnterpriseServices wrapper/thunk 引用。脚本 PowerShell 语法检查通过，且以相同编译器、引用集、x64 参数、图标和响应文件执行的 launcher 编译成功。随后 `cmake --build .\\build --config Release --target full -- -j 1` exit 0，完成原生产品、MSIX、WorkspaceExtension 与 portable 重打包回归。该脚本仍是活跃交付流程，Azure DevOps 的 `VSBuild@1`/MSBuild 模板以及根 XAML host 的 Microsoft.Build API 运行时依赖仍未消除，L2 仍未完成。

> 最新检查点（CP-260）：GitHub `copilot-setup-steps` 的正式构建已从 `setup-msbuild`、solution restore 和 `razzle`/`bcz` 切换为唯一 CMake/Ninja 入口：配置后构建 Debug 默认目标及 `full`。NuGet packages.config restore 仍保留为依赖获取步骤，但 workflow 不再查找或启动 `msbuild`。根 CMake 的 XAML host 仍有 .NET Framework Microsoft.Build API 运行时依赖，Azure DevOps 的旧 MSBuild 模板及历史/独立工具脚本尚未处理，因此 L2 仍未完成。

> 最新检查点（CP-259）：根 CMake 的失效 MSBuild 调度变量、路径与环境块已删除；XAML host 不再使用 VS `MSBuild/Current/Bin/Roslyn/csc.exe`，而是固定由 .NET SDK 8.0.421 的 `csc.dll` 以明确 .NET Framework 4.7.2 reference response file 编译。SDK XAML task 的程序集解析也已从 VS 的 `MSBuild/Current/Bin` 转到系统 .NET Framework 4 runtime 目录。`cmake -S . -B .\\build` 与 `cmake --build .\\build --target TerminalControlXaml --config Release -- -j 1` 均 exit 0，后者完整执行两阶段 XAML/XBF。迁移中暴露 TerminalControl host 参数遗漏 `--language CppWinRT`，已按所有其他 XAML target 的同一合同补齐。根 CMake 已无 `MSBuild/Current`、`MSBuild.exe` 或 `.vcxproj` 引用；SDK task 本身仍依赖 .NET Framework 的 Microsoft.Build API，不能误记为 L2 完成。

> 最新检查点（CP-258）：ScratchIslandApp 的 SampleAppLib、SampleApp DLL 与 WindowExe 已由同一 CMake/Ninja 图唯一拥有。SampleApp 直接生成五份 IDL 的 WinMD、C++/WinRT component/consumer projection、两阶段 XAML/XBF、PRI、静态实现库和 DLL；WindowExe 显式链接该实现库与既有原生产品闭包，并保留 RC、嵌入 manifest 及原输出位置。迁移中按当前 `ICoreScheme` 合同补上 `MySettings::GetColorTable`，补齐 XAML metadata provider 翻译单元，移除两个已无调用的键盘辅助函数；没有保留 MSBuild 回退。`cmake --build .\\build --target ScratchWindowExe --config Release -- -j 1` 成功生成 `microsoft/bin/x64/Release/ScratchIslandApp/WindowExe.exe`。三个旧 vcxproj 已删除，当前 CMake 图不再引用任何 vcxproj。

> 最新检查点（CP-257）：TestHostApp 已迁移为 CMake/Ninja 原生 C++/WinRT 应用，直接生成 TAEF WinRTCore 投影，保留启动测试客户端、Dark 主题、manifest 双份布局、LocalTests DLL 与 TE.AppxUnitTestClient 布局。原空 XAML 和 C++/CX 两阶段编译被删除，源文件改为明确的 `UnitTestApp.cpp/.h`；没有 C++/CX 兼容分支或旧入口。`cmake --build .\\build --target TestHostApp` exit 0。目标已加入 `full`，旧 TestHostApp vcxproj/metaproj 与 solution 项删除；剩余仅 ScratchIslandApp 的 3 个 vcxproj。

> 最新检查点（CP-256）：WindowsTerminal 主程序已由 CMake/Ninja 原生生成，8 个窗口/宿主源、RC、manifest 和 3 个 workspace 诊断/文本/路径源均由同一目标拥有。TerminalApp 静态实现目标明确设置 `DISABLE_XAML_GENERATED_MAIN`，因此只由 WindowsTerminal 的 `main.cpp` 提供进程入口；真实链接闭包明确包含 TerminalAppLib、CLI11、ThemeHelpers 和系统库。`cmake --build .\\build --target WindowsTerminal` exit 0。当前正在用该目标替换 `full` 中最后的 WindowsTerminal MSBuild 命令并删除旧 vcxproj/solution 项，随后剩余 TestHostApp 与 3 个 Scratch 工程。

> 最新检查点（CP-255）：TerminalApp 的两个测试消费者已切换为唯一的 CMake/Ninja 目标。`TerminalAppUnitTests` 原生链接 `Terminal.App.Unit.Tests.dll`；`TerminalAppLocalTests` 原生编译五组测试并链接 `TerminalApp.LocalTests.dll`。迁移中只按真实符号闭包补入 `ntdll.lib` 与 `TerminalThemeHelpers.lib`；`TabTests.cpp` 中误把投影命名空间当作 implementation 数据类型的三类声明已改为精确完整类型名，没有别名、兼容层或备用路径。`cmake --build .\\build --target TerminalAppUnitTests TerminalAppLocalTests` exit 0。当前正在把三者加入 `full`、删除四个已被替代的 TerminalApp/测试 vcxproj 及 solution 调度；随后直接迁移 TestHostApp 与 WindowsTerminal。

> 最新检查点（CP-252）：UIMarkdown 已由根 CMake 直接拥有 Builder/CodeBlock/XamlMetaDataProvider 三份 IDL、独立 WinMD、精确 contract metadata 下的 `mdmerge`、component/consumer projection、CodeBlock 两阶段 XAML/XBF、PRI 和全部实现源码。按真实未解析符号补齐唯一 `cmark.lib` 后，`cmake --build .\\build --target UIMarkdown` exit 0。TerminalAppLib、TerminalApp DLL、WindowsTerminal 已切断旧 ProjectReference 并直接读取唯一 Ninja WinMD；`full` 已加入目标，旧 solution 项和 vcxproj/filters 已删除。无 preset、wrapper、回退、旧 Generated Files 或第二种构建入口。

> 最新检查点（CP-253）：`ControlUnitTests` 与 `SettingsModelUnitTests` 两个 TAEF DLL 已由 CMake/Ninja 成功链接。Control 单测明确链接其真实 MidiAudio 依赖；Settings Model 单测把 `WorkspaceTests.cpp` 从不存在的旧 `WorkspaceEditorState` 调用更新到当前双 `WorkspaceManager` 保存合同。Settings Model 实现库明确纳入 `WorkspaceDiagnosticLog.cpp`、`WorkspaceStoragePaths.cpp` 及真实文本规范化依赖 `WorkspaceChatTextHelpers.cpp`。`cmake --build .\\build --target ControlUnitTests SettingsModelUnitTests` exit 0；两个目标已加入 `full`，solution 调度及两个旧 vcxproj 正在同批删除。下一目标为 TerminalApp Lib/DLL 主依赖链，不在本检查点停下。

> 最新检查点（CP-254）：TerminalApp Lib/DLL 已由 Ninja 原生链接成功。唯一生成图包含 26 个项目 IDL 加 XAML provider、`mdmerge`/component 与 consumer projection、`App.xaml` ApplicationDefinition、14 个 Page、两阶段 XAML/XBF、PRI、完整实现静态库和薄 DLL。为精确复现旧工程的 `DoNotGenerateOtherProviders`，现有单用途 XAML task host仅补充 `XamlApplications` 与 SDK task 原生 `CodeGenerationControlFlags` 属性；它仍不是用户入口且不调用 MSBuild。构建中按真实闭包补齐 HostingContract projection、Workspace amalgamation、CLI11 和 XAML provider translation unit，`cmake --build .\\build --target TerminalApp` exit 0。下一步切换 LocalTests、ut_app 和 WindowsTerminal 消费者并删除两个旧 TerminalApp 工程，随后继续迁移这三个消费者。

## 1. 目标与完成定义

本计划只针对本仓库实际交付的 portable 产品链路。唯一受支持的构建入口保持为现有 CMake 命令；只替换入口内部的 MSBuild 实现，不新增第二套入口。

“解决 MSBuild 依赖”分两级验收，最终必须同时满足：

1. **L1：移除 Visual Studio MSBuild 工程依赖**
   - `full`、`ext` 和默认构建不启动 `MSBuild.exe`。
   - 生成的 Ninja 文件不引用 `.vcxproj`、`.wapproj`、`.sln` 或 `.slnx`。
   - 根 `CMakeLists.txt` 不查找 MSBuild，不读取 `VCTargetsPath`，不传递 MSBuild 属性。
2. **L2：移除所有 MSBuild 引擎和安装布局依赖**
   - 不以 `dotnet build`、`dotnet publish` 或 `dotnet msbuild` 绕过 L1；这些命令内部仍运行 MSBuild。
   - 活跃构建链不读取任何 `...\MSBuild\...` 路径中的 targets、tasks、脚本或工具。
   - C# 打包工具改为 CMake 直接驱动 Roslyn，或迁移为不需要 MSBuild 的实现。
   - MSIX、PRI、XAML、符号及测试布局由显式工具命令生成，不依赖 WAP targets。

最终用户体验必须是：从普通 PowerShell 启动，不要求手工进入 Developer Command Prompt，也不要求 `msbuild` 在 PATH 中。

```powershell
cmake -S . -B .\build
cmake --build .\build
cmake --build .\build --target full
```

### 不可变约束

1. 不增加 `--preset`、wrapper、legacy、compatibility 或任何第二套构建入口。
2. 不改变 `build` 目录、`ext`/`full` 目标名和 `bin` 产物位置。
3. 不保留新旧实现并存开关；一个目标迁移完成时直接删除对应 MSBuild 调用。
4. 不为了兼容旧工程增加条件分支、转发层或备用路径。
5. 检查点只按当前工作区的真实状态标记，已撤销或未提交的试验不得写成 `DONE`。

## 2. 2026-08-20 现状调查

### 2.1 工具与配置基线

| 项目 | 当前事实 | 结论 |
| --- | --- | --- |
| CMake | 独立安装，版本 4.4.2 | 可作为唯一配置器 |
| Ninja | 独立安装，版本 1.13.2 | 可作为唯一执行器 |
| 现有 `build` 缓存 | `Ninja Multi-Config` | 生成器方向正确 |
| 实际 `CMAKE_MAKE_PROGRAM` | 2026-08-21 已切换为 `C:/ProgramData/chocolatey/bin/ninja.exe` 1.13.2 | 现有 Ninja 缓存已不再使用 VS 内置 Ninja |
| C++ 编译器 | VS 18 的 MSVC 14.50 | CMake/Ninja 可直接驱动，不需要 MSBuild |
| 普通 PowerShell | `cl`、`link`、`msbuild` 不在 PATH，`INCLUDE`/`LIB` 未设置 | 需要仓库自带工具链引导层 |
| 干净配置探针 | 显式使用独立 Ninja 时，`project()` 阶段报找不到 C++ 编译器 | 当前 README 的两条命令不能从干净环境复现 |

MSVC、Windows SDK、MIDL、MakePri、MakeAppx 等 Windows 工具仍是合理的平台工具依赖；本计划消除的是 MSBuild 编排、工程文件和 `MSBuild` 安装目录依赖，不把“去 MSBuild”错误扩张为“去 Windows SDK/编译器”。

### 2.2 工程图规模

仓库中当前仍存在的主要 MSBuild 文件（2026-08-21 / CP-207 后）：

| 类型 | 数量 |
| --- | ---: |
| `.vcxproj` | 70 |
| `.wapproj` | 1 |
| `.csproj` | 13 |
| `.sln` | 6 |
| `.slnx` | 1 |
| `.props` | 14 |
| `.targets` | 9 |
| `.vcxitems` | 3 |
| `ProjectReference` 节点 | 205 |
| `Import` 节点 | 382 |

初始调查时根 `CMakeLists.txt` 显式调用以下四个 MSBuild 工程：

1. `microsoft/src/host/proxy/Host.Proxy.vcxproj`
2. `microsoft/src/cascadia/TerminalControl/dll/TerminalControl.vcxproj`
3. `microsoft/src/cascadia/TerminalSettingsModel/dll/Microsoft.Terminal.Settings.Model.vcxproj`
4. `microsoft/src/cascadia/CascadiaPackage/CascadiaPackage.wapproj`

这四个入口的静态 `ProjectReference` 递归闭包共有 **39 个工程**，因此初始方案不是“CMake 构建主体、MSBuild 补四个小目标”，而是 Ninja 外层包裹完整 MSBuild 产品图。

2026-08-21 已确认前三个调用都被第四个 `.wapproj -> WindowsTerminal.vcxproj` 传递图重复构建，并从根 CMake 直接删除。随后 `.wapproj` 打包已由直接 MakePri/MakeAppx 替换，工程文件和 solution 引用同时删除。`wt`、`elevate-shim`、ShellExtension 和 `Host.Proxy` 也已切换到 CMake/Ninja，对应旧工程和引用均已删除。当前 `full` 为尚未原生化的产品编译显式启动 2 个 `.vcxproj` 根，打包层已无 MSBuild。

### 2.3 `glue` 的实际状态

- `WorkspaceExtension` 只由原生 CMake `add_library` 生成；两个产品 ProjectReference、旧 `.vcxproj` 和 `.filters` 已删除。
- `full` 先完成仍存的产品 MSBuild 图，再由 CMake 从明确输出目录编译/链接 WorkspaceExtension，最后注入 portable；MSIX 不再拥有或覆盖该 DLL。
- WorkspaceExtension 的源文件、包含路径、生成文件和 11 个尚未原生化的链接库仍集中写在根 `CMakeLists.txt`，后续要拆回 `ext/src/glue/workspace` 并改为 CMake target 依赖。
- `PortablePackageTool.csproj` 已删除；当前 CMake 直接调用固定 .NET SDK Roslyn 编译 `Program.cs` 与 `GlobalUsings.cs`，不再执行 `dotnet build`。

### 2.4 当前构建证据

| 检查 | 结果 | 证据摘要 |
| --- | --- | --- |
| `cmake --build build --config Release --target ext -- -v` | 通过 | package-tool 约 20 秒，repack 约 5 秒；增量命中，不能替代干净编译验证 |
| `cmake --build build --config Release --target full -- -v` | 通过 | MSBuild 主体约 2 分 31 秒，repack 约 20 秒 |
| `full` 警告 | 未达标 | 直接 MakePri 仍有 132 条既有 PRI263 警告，0 错误 |
| 独立 Ninja 干净配置 | 失败 | 普通 PowerShell 中 `CMAKE_CXX_COMPILER-NOTFOUND` |
| 现有 `build` 自动重新配置 | 通过 | 2026-08-21，原命令 `cmake --build .\\build`；不再要求 `INCLUDE`/`LIB` 或手工运行 vcvars |
| 现有 `build` 默认目标 | 通过 | 2026-08-21，原命令 `cmake --build .\\build`；WorkspaceExtension 编译链接、PortablePackageTool 生成和 ext-repack 均通过，0 警告、0 错误 |
| PortablePackageTool 直接 Roslyn 编译 | 通过 | 2026-08-21，固定 .NET SDK 8.0.421 的 `csc.dll` 编译并运行成功；第二次构建未重新编译；当前 Ninja 规则无 `dotnet build` 和 csproj |
| portable launcher Roslyn 路径 | 通过 | 2026-08-21，删除当前 CMake 图对 `MSBuild/Current/Bin/Roslyn/csc.exe` 的查找；SDK `csc.dll` + 显式 .NET Framework 4.7.2 references 生成单文件 EXE |
| 直接打包 `full` 回归 | 通过 | 2026-08-21，原命令 `cmake --build .\\build --target full` exit 0；Ninja MIDL/代理 DLL、2 个剩余产品根工程、3 个其他 CMake/Ninja 最终目标与直接 MakePri/MakeAppx 约 90 秒，WorkspaceExtension no-op，full-repack 20 秒 |
| MSIX 打包契约 | 完成 | 直接 map 253 项：221 PNG、11 DLL、10 WinMD、4 EXE、3 ICO，以及 HTML/JSON/PRI/XML 各 1；与删除 WorkspaceExtension 后的 WAP 基线路径集差异为 0 |
| WAP 非交付布局删除 | 通过 | 2026-08-21，`bin/msix` 只包含唯一主包 `CascadiaPackage_0.0.1.0_x64.msix`；无 `_Test`、appxsym、Dependencies、侧载脚本和 telemetry；`.wapproj` 已删除 |

初始 `full` 日志确认 WAP/MSBuild 隐式负责 PRI、资源复制、MakeAppx、appxsym、测试安装脚本及依赖布局，并使用 `MSBuild\Microsoft\VisualStudio\...\AppxPackage` 安装布局。当前 `full` 已将这些打包行为收缩为显式 manifest/PRI/map/MakeAppx 规则；MSBuild 仍在产品 `.vcxproj` 内触发 feature flags、MIDL、C++/WinRT、WinMD merge 和 XAML/XBF，属于 P2-P5 的剩余迁移范围。

### 2.5 MSBuild 启动面分类

2026-08-21 对可执行脚本、CMake、CI 和开发工具做了调用面扫描，必须按类别分别删除，不能用一层兼容封装掩盖：

| 类别 | 当前事实 | 收口检查点 |
| --- | --- | --- |
| 当前 CMake portable 图 | 打包层无 MSBuild；`full` 显式启动 2 个产品 `.vcxproj` 根；`wt`、`elevate-shim`、ShellExtension 与 `Host.Proxy` 由 CMake/Ninja 原生构建 | P2-P5、CP-501、CP-701、CP-702 |
| `ext/src/glue` | 无脚本或工程启动 MSBuild；WorkspaceExtension 旧工程、filters 和产品引用已删除 | CP-502 已完成；目录拆分归 CP-501 |
| 旧 portable PowerShell 图 | MSBuild wrapper `Build-PortableTerminalDistribution.ps1` 已删除且无引用；`New-UnpackagedTerminalDistribution.ps1` 仍被正式 pipeline 调用，并硬编码 MSBuild Roslyn | CP-705、CP-801 |
| Microsoft CI | `job-build-project.yml` 等模板使用 VSBuild/MSBuild；portable job 仍调用旧 PowerShell 图 | CP-705 |
| 根 GitHub workflow | `.github/workflows/copilot-setup-steps.yml` 使用 `setup-msbuild` 并验证 `.slnx` | CP-705 |
| 开发者命令 | `OpenConsole.psm1`、`bcz.cmd`、`bx.ps1`、`razzle.cmd` 维护 MSBuild 环境和入口 | CP-705、CP-801 |
| 独立旧工具 | `ColorTool/build.bat` 直接定位并启动 MSBuild | P8 范围判定后迁移或删除 |
| vcpkg triplet | 多个 triplet 通过 `MSBuild/Microsoft/VC/...props` 判断 VS 工具集；不是子进程，但仍是安装布局依赖 | CP-104、CP-701 |
| 已跟踪历史生成树 | `build.before-relocation` 含旧 MSBuild/dotnet build 命令，持续污染扫描 | CP-802 |

## 3. 目标构建架构

构建图必须由 CMake 显式表达，Ninja 只执行 CMake 生成的规则：

```text
普通 PowerShell
  -> 现有 cmake -S / cmake --build 命令
  -> CMake + 独立 Ninja
       -> vcpkg 依赖
       -> 普通 C++ 静态库
       -> MIDL / C++/WinRT / WinMD 生成
       -> XAML / XBF / PRI 生成
       -> DLL / EXE / WorkspaceExtension
       -> manifest + MakePri + MakeAppx
       -> portable repack
```

建议拆分根构建文件，避免继续形成单个巨型 `CMakeLists.txt`：

- `cmake/toolchains/`：MSVC 与 Windows SDK 定位、架构映射。
- `cmake/modules/`：MIDL、C++/WinRT、XAML、PRI、MSIX、Roslyn 辅助函数。
- `microsoft/src/**/CMakeLists.txt`：对应产品闭包内的原生目标。
- `ext/src/**/CMakeLists.txt`：extension 与 portable 工具目标。
- 根 `CMakeLists.txt`：保持现有命令不变，在内部完成工具定位和目标迁移。

## 4. 分阶段执行计划

### P0：固定基线与自动化清单

目标：把目前的人工调查转成可重复报告，防止漏掉隐式依赖。

工作项：

- 增加只读清单脚本，输出工程引用闭包、Imports、源文件、编译定义、包含目录、链接库、代码生成项和自定义 targets。
- 保存旧链路的目标级产物清单、哈希、PE 架构、导出表、WinMD/PRI/MSIX 内容清单。
- 将 132 条 PRI 警告建立基线；区分迁移回归和既有问题。
- 明确产品范围：39 个工程闭包是必须迁移集，其余工程按测试、工具、样例、CI、废弃候选分类。

退出条件：清单可一条命令重新生成，且根四入口的递归引用无未解析项。

### P1：独立 Ninja 与可复现工具链引导

目标：先解决干净环境无法配置的问题。

工作项：

- 保持 `cmake -S . -B .\build` 和 `cmake --build .\build` 不变，禁止增加 preset、wrapper 或第二套入口。
- 让现有入口选择已单独安装的 Ninja，禁止落到 VS 内置 Ninja。
- 在 `project()` 前完成 MSVC/Windows SDK 发现；支持 x64、x86、arm64 的 host/target 映射。
- 从普通 PowerShell 自动取得 `cl/link/lib/rc/midl`、include 和 lib 路径，不要求用户手工运行 `vcvars*.bat`。
- 去掉根 CMake 对 `PORTABLE_MSBUILD_PATH`、`MSBuild.exe`、`VCTargetsPath` 和 VS 版本属性的配置期要求。
- 增加工具版本与路径诊断目标，错误信息必须指出缺少的具体组件。

退出条件：空构建目录、普通 PowerShell、独立 Ninja 下配置成功；缓存中 Ninja 路径指向独立安装；配置过程不查找 MSBuild。

### P2：迁移无代码生成的 C++ 基础层

目标：先把低风险静态库迁入 CMake，建立可复用的编译/链接约定。

建议顺序：types、parser、input、renderer base、adapter、buffer、interactivity、host/server 的纯 C++ 部分，再处理依赖更复杂的目标。

工作项：

- 将公共 `.props/.targets` 中实际生效的编译定义、警告、运行库、PCH、链接选项转换为 CMake interface targets。
- 为每个库显式列源文件，不用无边界递归 glob。
- 每迁移一个目标，同时比较旧/新产物的架构、导出、链接依赖和关键编译选项。
- 每个目标迁移通过后，在同一个改动中删除对应 `.vcxproj` 回调和失效工程文件，不保留并存路径。

退出条件：基础层由 Ninja 原生构建；修改一个源文件只重编受影响目标；新链路无 `.vcxproj` 输入。

### P3：显式重建 MIDL、C++/WinRT 与 WinMD 链

目标：替代 MSBuild 的 CppWinRT targets 和元数据合并逻辑。

工作项：

- 记录每个 IDL 的 `midl/midlrt` 输入、参数、引用 WinMD、输出和依赖。
- CMake `add_custom_command(OUTPUT ... DEPFILE/BYPRODUCTS ...)` 显式生成 ABI、projection、module、WinMD。
- 显式调用 C++/WinRT 工具和 WinMD 合并工具，输出只能进入 build/obj，不写源目录中的 `Generated Files`。
- 迁移 TerminalCore、TerminalConnection、UIHelpers、TerminalSettingsModel、TerminalControl 等 WinRT 目标。
- 验证干净构建和 IDL 单文件增量构建均正确。

退出条件：删除生成目录后 Ninja 可完全重建；连续第二次构建为 no-op；WinMD API/元数据与旧产物等价。

### P4：显式重建 XAML、XBF 与 PRI 链

目标：替代 MarkupCompilePass1/2、XAML targets 和项目级 PRI targets。

工作项：

- 先做 TerminalControl 的最小 XAML spike，确定直接调用 XAML 编译器的稳定参数与输入输出契约。
- 若 VS XAML 编译器只能通过 MSBuild task 稳定使用，则在此检查点停下做技术决策：固定可直接调用工具，或改为受控生成物方案；禁止用隐藏 MSBuild 子进程冒充完成。
- 将 XAML 两阶段生成、XBF、generated C++、资源复制和 PRI 分别建模为 Ninja 节点。
- 迁移 TerminalApp、TerminalSettingsEditor、TerminalControl 及其资源合并。
- 逐步清零 PRI263；至少不得比既有 132 条增加。

退出条件：触碰一个 XAML/resw 只触发对应生成链；输出目录无源树污染；打包资源键和旧 MSIX 对齐。

### P5：迁移最终 DLL/EXE 与 `glue`

目标：所有运行时二进制只由一个 CMake/Ninja 图生产。

工作项：

- 迁移 WindowsTerminal、TerminalApp、wt、OpenConsole、OpenConsoleProxy、ShellExtension、ElevateShim。
- 把现有 `WorkspaceExtension` 根级硬编码拆到 `ext/src/glue/workspace`，依赖原生 CMake targets，而不是预先存在的 `.lib` 文件路径。
- 立即消除 WorkspaceExtension 的 CMake/MSBuild 双构建和同目录双写。
- 对 EXE/DLL 做导出表、imports、manifest、资源、WinMD、PDB 和启动 smoke test。

退出条件：`full` 的二进制阶段不启动 MSBuild；删掉 `microsoft/bin`、`microsoft/obj` 后可从零重建。

### P6：迁移打包、符号与 PortablePackageTool

目标：替代 `.wapproj` 和 `dotnet build`。

工作项：

- 由 CMake 生成确定性的 package layout/map，直接调用 MakePri/MakeAppx。
- 明确 portable 交付是否需要 appxsym 和 VS 测试侧载布局；不需要的产物直接从范围中删除，需要的则用可定位的 SDK/编译器工具重建。
- 不再复制 `MSBuild\...\AppxPackage` 下的安装脚本、遥测程序集和模板；仓库拥有真正需要的最小脚本/模板。
- 优先将无第三方依赖的 PortablePackageTool 改为 CMake 直接调用 .NET SDK `Roslyn/bincore/csc.dll`，并显式生成 runtimeconfig；若维护成本更低，再评估迁移到 C++。
- 将 Roslyn 路径从 VS `MSBuild/Current/Bin/Roslyn/csc.exe` 切换到固定 .NET SDK。

退出条件：`.msix` 和 portable 单文件完全由 Ninja 生成；进程树无 MSBuild；构建日志和缓存无 `\MSBuild\` 活跃路径。

### P7：切换入口并建立防回归门禁

目标：让纯 CMake/Ninja 成为唯一事实来源。

工作项：

- 默认、`ext`、`full` 都走同一原生图；`ext` 只缩小重打包范围，不依赖旧产物碰巧存在。
- 更新 README、开发说明和 CI，移除 `setup-msbuild`、`razzle/bcz/bx` 及所有 MSBuild 入口。
- CI 增加静态门禁：活跃构建脚本禁止 `msbuild`、`.vcxproj/.wapproj` 和 `dotnet build/publish/msbuild`。
- CI 增加运行门禁：从干净目录构建并采集进程树，禁止 MSBuild 子进程。
- 验证 x64 Release/Debug；x86/arm64 至少完成配置和核心构建，发布支持的平台必须完成 full。

退出条件：连续两次干净 CI 全绿；旧工程改坏不会影响 portable 构建；独立 Ninja 是实际执行器。

### P8：清理旧 MSBuild 表面

目标：在产品图稳定后移除误导和维护负担。

工作项：

- 删除产品闭包中的 `.vcxproj/.wapproj/.slnx`、共享 props/targets 和 MSBuild 脚本，不做归档兼容。
- `microsoft` 下不在 portable 范围的测试、样例和工具单独列清单；需要的迁移，不需要的删除。
- 合并 `ext/docs/terminalapplib-vcxproj-removal-plan.md` 的未完成事项；不能单独删除 TerminalAppLib 造成半断工程图。
- 清理已跟踪的历史构建树，如 `build.before-relocation`，避免扫描和维护误判。

退出条件：仓库文档只描述受支持入口；无活跃脚本依赖已删除工程；全量引用扫描无产品链残留。

## 5. 详细检查点

状态含义：`DONE` 已有可复核证据；`TODO` 未开始；`BLOCKED` 有明确阻塞并记录在第 7 节。

| ID | 状态 | 检查点 | 通过证据 |
| --- | --- | --- | --- |
| CP-001 | DONE | 记录 CMake/Ninja/编译器现状 | 本文 2.1 |
| CP-002 | DONE | 统计 MSBuild 文件与引用规模 | 初始 77 vcxproj/229 references；当前 70 vcxproj、1 wapproj、13 csproj、205 references、382 imports；产品闭包初始 39 工程 |
| CP-003 | DONE | 执行当前 `ext` 基线 | 2026-08-20，exit 0 |
| CP-004 | DONE | 执行当前 `full` 基线 | 2026-08-20，exit 0，132 warnings |
| CP-005 | DONE | 复现普通 PowerShell + 独立 Ninja 干净配置失败 | `CMAKE_CXX_COMPILER-NOTFOUND` |
| CP-006 | TODO | 提交可重复的 MSBuild 图/属性/产物清单脚本 | 报告无未解析引用 |
| CP-007 | DONE | 分类记录 MSBuild 实际启动面 | 本文 2.5；当前图、旧 portable、CI、开发命令、独立工具、路径探针和历史生成树分开登记 |
| CP-100 | DONE | 现有 Ninja 缓存可在普通 PowerShell 中自动重新配置和构建 | 2026-08-21，原命令 `cmake --build .\\build` 完成 configure/generate、WorkspaceExtension 编译链接、PortablePackageTool 生成和 ext-repack，exit 0 |
| CP-101 | TODO | 原有 configure 命令可从普通 PowerShell 执行 | `cmake -S . -B .\build` exit 0 |
| CP-102 | TODO | 原有入口自动发现 MSVC/SDK | 不要求手工运行 vcvars，干净 configure exit 0 |
| CP-103 | BLOCKED | 原有入口强制使用独立 Ninja | CMake 在读取仓库 `CMakeLists.txt` 前选择生成器；当前机器无 `CMAKE_GENERATOR` 时默认 Visual Studio 18，且约束禁止参数、preset、wrapper 和第二入口 |
| CP-104 | DONE | 配置阶段去掉 MSBuild 查找 | 2026-08-21，删除 find_program/vswhere/VC targets 分支和四个路径覆盖参数；PATH 无 msbuild 时原命令自动 configure/generate 并构建成功 |
| CP-105 | DONE | 现有 Ninja 构建固定使用单独安装的 Ninja | 2026-08-21，`CMAKE_MAKE_PROGRAM=C:/ProgramData/chocolatey/bin/ninja.exe`，版本 1.13.2；原命令 exit 0，后续增量无 C++/C# 重编 |
| CP-201 | TODO | 公共编译约定迁移为 interface targets | 选定基础库旧/新 flags 对比通过 |
| CP-202 | TODO | 第一批纯 C++ 库原生化 | 干净构建、增量 no-op、产物对比通过 |
| CP-203 | TODO | host/server 纯 C++ 闭包原生化 | OpenConsole 依赖库均由 Ninja 生产 |
| CP-204 | DONE | `wt` 与 `elevate-shim` 两个叶子 EXE 原生化 | CMake 直接编译 C++/RC 并链接 onecore；原 `full` 和默认命令通过；两个 vcxproj、solution 条目和 MSBuild 调用已同时删除；x64/subsystem/CFG、imports、manifest、资源目录检查通过 |
| CP-205 | DONE | ShellExtension 最终 DLL 原生化 | 固定 cppwinrt 2.0.250303.1 projection 与 CMake PCH/link 通过；exports/imports/PE 合同差异 0；旧 vcxproj、solution 条目、两个 ProjectReference 和根 MSBuild 调用已同时删除；原 `full` 与默认命令通过 |
| CP-206 | DONE | `Host.Proxy` MIDL/DLL 原生化并删除旧工程 | Ninja 生成两组 MIDL 头、proxy/stub 源和 `OpenConsoleProxy.dll`；旧 ProjectReference、vcxproj 与 solution 调度依赖为 0；原 `full` 与默认命令通过 |
| CP-207 | DONE | 原生化纯 C++ 静态叶子库 `MidiAudio` | Ninja 静态库与 PCH/PDB 输出通过；两个最终链接消费者显式使用唯一 Ninja `.lib`；旧 vcxproj、3 个 ProjectReference、solution/slnf 条目同步删除；原三条命令通过 |
| CP-208 | DONE | 原生化纯 C++ 静态叶子库 `ConRenderUia` | Ninja PCH/archive/PDB 通过；产品 DLL 与控制层单测显式链接唯一 `.lib`；旧 vcxproj、2 个 ProjectReference 和 solution 项同步删除；原三条命令通过 |
| CP-209 | DONE | 原生化产品闭包静态叶子库 `ConInt` | Ninja PCH/archive/PDB 通过；8 个最终链接目标显式使用唯一 `.lib`；旧 vcxproj、6 个 ProjectReference 和 solution/filter 项同步删除；原三条命令通过 |
| CP-210 | DONE | 原生化产品闭包静态叶子库 `ConTSF` | Ninja PCH/archive/PDB 通过；7 个最终链接目标显式使用唯一 `.lib`；旧 vcxproj、6 个 ProjectReference 和 solution/filter 项同步删除；原三条命令通过 |
| CP-211 | DONE | 原生化产品闭包静态叶子库 `ConRenderGdi` | Ninja PCH/archive 及 `usp10.lib` 合入合同通过；5 个最终链接目标显式使用唯一 `.lib`；旧工程与全部引用删除；原三条命令通过 |
| CP-212 | DONE | 原生化产品闭包静态叶子库 `ConInteractivityBaseLib` | Ninja PCH/archive 通过；6 个最终链接目标显式使用唯一 `.lib`；旧工程与全部引用删除；原三条命令通过 |
| CP-213 | DONE | 原生化产品闭包静态叶子库 `ConRenderBase` | Ninja PCH/archive 通过；两个静态中间库展开为 9 个最终链接入口；旧工程与 10 个引用删除；原三条命令通过 |
| CP-214 | DONE | 原生化产品闭包静态叶子库 `ConTermParser` | Ninja PCH/archive 通过；两个静态中间库展开为 10 个最终链接入口；旧工程与 11 个引用删除；原三条命令通过 |
| CP-215 | DONE | 原生化产品闭包静态叶子库 `ConProps` | Ninja PCH/archive 与旧产物 5-member/12492-symbol 合同一致；10 个最终链接入口显式使用唯一产物；旧工程与引用删除；原三条命令通过 |
| CP-216 | DONE | 原生化产品闭包静态叶子库 `ConBufferOut` | Ninja PCH/archive 与旧产物 15-member/3604-symbol 合同一致；静态中间层展开为 9 个最终链接入口；旧工程与 11 个引用删除；原三条命令通过 |
| CP-217 | DONE | 原生化产品闭包静态叶子库 `ConServer` | Ninja 明确依赖原生 OpenConsoleProxy 生成目标；archive 与旧产物 20-member/1629-symbol 合同一致；5 个最终链接入口；旧工程与引用删除；原三条命令通过 |
| CP-218 | DONE | 原生化 `conptylib` 静态叶子库 | Ninja PCH/archive 与旧产物 4-member/917-symbol 合同一致；5 个最终链接入口；旧工程与引用删除；原三条命令通过 |
| CP-219 | DONE | 原生化高扇出静态基础库 `ConTypes` | 32 个直接 ProjectReference 全部删除；8 个静态中间库递归展开为 25 个最终链接入口；15-member 源码合同一致；`/Z7` 非 LTCG 对象在所有产品链接中无调试信息损坏；旧工程删除；原三条命令通过 |
| CP-220 | DONE | 原生化 `ConRenderAtlas` HLSL 静态库 | 4 个 shader 生成头逐字节一致；C++/HLSL 精确增量；8 个最终消费者显式链接；旧工程及全部 solution/filter/ProjectReference 引用删除；原三条命令通过 |
| CP-221 | DONE | 原生化 `ConhostV2Lib` 主机静态库 | 44-member archive；OpenConsoleProxy 生成依赖明确；4 个最终链接入口；旧工程与引用删除；`ConhostV2Lib` 的 LNK4020/LNK1103 为 0；原三条命令通过 |
| CP-222 | DONE | 统一全部已原生化静态库的调试信息合同 | 15 个 STATIC 目标唯一使用 `/Z7 /GL-`；共享 compile-PDB 属性为 0；12 个陈旧 PDB 删除；完整产品链接不再出现这些 Ninja archive 的 LNK4020/LNK1103；原三条命令通过 |
| CP-223 | DONE | 原生化 `TerminalInput` 静态叶子库 | 3-member Ninja archive；9 个最终消费者显式链接；旧工程、4 个引用和 solution/slnf 调度删除；无共享 PDB；原三条命令通过 |
| CP-224 | DONE | 原生化 `ConTermAdapt` 静态库 | 9-member Ninja archive；9 个最终消费者显式链接；旧工程、7 个引用和 solution 调度删除；无共享 PDB；原三条命令通过 |
| CP-225 | DONE | 原生化 `ConInteractivityWin32Lib` 静态库 | 17-member Ninja archive；5 个最终消费者显式链接；旧工程、5 个引用和 solution/slnf 调度删除；无共享 PDB；原三条命令通过 |
| CP-226 | DONE | 原生化 `WinRTUtils` 静态库 | 3 个实现源码、CMake PCH 与 Ninja 所有的 C++/WinRT platform projection 已落地；12 个最终消费者显式链接；旧工程、9 个引用和旧 PDB 已删除；原三条命令全部通过 |
| CP-227 | DONE | 原生化 `TerminalSettingsAppAdapterLib` 静态库 | Ninja 独立生成 44 个 reference projection 文件并构建 2-member archive；4 个最终入口显式链接；旧工程、3 个引用、源码树投影、PDB/PRI 已删除；原三条命令全部通过 |
| CP-228 | DONE | 删除 renderer `wddmcon` 死工程 | 0 工程消费者、0 x64 Release 产物且 solution 禁止构建；源码保留，孤立 vcxproj/filters 与 solution/slnf 登记已删除；原三条命令全部通过 |
| CP-229 | DONE | 删除 interactivity `onecore` 死工程 | 0 当前工程消费者、0 archive/PDB 且 solution 禁止常用配置构建；保留源码，删除孤立 vcxproj/filters 与 solution/slnf 登记；原三条命令全部通过 |
| CP-230 | DONE | 原生化 `ConhostV2Lib.unittest` 静态库 | 44 个 host 实现对象按测试宏独立编译；Host 单测唯一显式链接；旧 vcxproj/metaproj、引用和 solution/slnf 调度已删除；原三条命令全部通过 |
| CP-231 | DONE | 原生化 `TerminalCore` WinMD/projection/静态库 | Ninja 直接完成单 IDL MIDL、4-file projection 和 5-member archive；3 个最终入口显式链接；旧工程/引用/源码树生成物/PDB 已删除；原三条命令通过 |
| CP-232 | DONE | 原生化 `TerminalControlLib.vcxproj` | TerminalControl Lib/DLL、MIDL/WinMD/两阶段 XAML/PRI 由 Ninja 唯一拥有；旧工程和调度已删除，x64/x86 实链通过 |
| CP-301 | DONE | MIDL 最小 spike | 所有产品 IDL 由 CMake 直接调用 SDK MIDL；删除输出后可精确重建 |
| CP-302 | DONE | C++/WinRT projection 与 WinMD merge helper | `mdmerge`/`cppwinrt` 的显式输入输出由 Ninja 持有，最终 full 已验证 |
| CP-303 | DONE | WinRT 库闭包原生化 | 产品 WinRT 库、消费者投影和最终 DLL 均不再进入旧工程图 |
| CP-401 | DONE | TerminalControl XAML spike | 直接 SDK task host 的 Pass1/Pass2/XBF 已纳入 Ninja，无 `msbuild.exe` 或 vcxproj 调用 |
| CP-402 | DONE | PRI 生成与资源合并原生化 | 正式 `full` 直接 MakePri；1571 named resources / 23575 candidates，对基线归一化差异 0，警告未增加 |
| CP-403 | DONE | TerminalApp/SettingsEditor XAML 原生化 | TerminalApp、Settings Editor 与 UIMarkdown 的 XAML/XBF/PRI 均由 Ninja 生成，完整 Release full 通过 |
| CP-501 | DONE | 最终 DLL/EXE 原生化 | 产品 DLL/EXE、manifest、MSIX 和 portable repack 均由 CMake/Ninja 完成，Release full 通过 |
| CP-502 | DONE | WorkspaceExtension 单一产物所有者 | 2026-08-21，原命令 `full` exit 0；MSBuild 不再进入旧工程，MSIX 中 0 个 WorkspaceExtension，portable 中唯一 DLL 来自 CMake 输出 |
| CP-503 | TODO | 删除 `microsoft/bin`、`obj` 后 full 可重建 | 干净 full exit 0 |
| CP-504 | DONE | 删除根 CMake 中三个重复的 MSBuild 预构建调用 | 2026-08-21，`.wapproj -> WindowsTerminal.vcxproj` 传递图独立完成 full；原命令 exit 0，132 条既有警告，根仅剩 1 个 package MSBuild 调用 |
| CP-505 | DONE | 删除根 `Host.Proxy.vcxproj` 重复调用 | 2026-08-21，`Host.EXE -> server -> Host.Proxy` 唯一传递链完成构建；原 `full` exit 0；根调用已删除且工程仅由实际依赖消费 |
| CP-601 | DONE | PortablePackageTool 去 `dotnet build` | 2026-08-21，原命令 exit 0；CMake 直接调用 .NET SDK 8.0.421 Roslyn，repack 通过，增量构建未重编；旧 csproj 已删除，当前 Ninja 规则无 csproj |
| CP-602 | DONE | `.wapproj` 打包替换 | 原 `full` 直接生成 manifest/PRI/map 并调用 MakeAppx，exit 0；旧 `.wapproj` 和 solution 引用已删除 |
| CP-603 | DONE | 移除 AppxPackage 安装布局依赖 | 当前 CMake、Ninja 和活跃文档无 `AppxPackageOutput/Dir`、`.wapproj` 或 `MSBuild\\...\\AppxPackage` |
| CP-604 | DONE | MSIX/portable 内容与旧基线对比 | 255 ZIP 路径集差异 0；251 个非生成 payload 字节级相同；manifest 151 类和 PRI 23575 candidates 差异 0；full-repack 通过 |
| CP-605 | DONE | portable launcher 去 MSBuild Roslyn 路径 | 2026-08-21，当前 Ninja 规则无 MSBuild Roslyn 和旧 `--csc-path`；`WindowsTerminalPortableGeekEdition_System_0.0.1.0_x64.exe` 生成成功 |
| CP-606 | DONE | 删除 portable 未消费的 WAP symbol/test layout | 2026-08-21，删除历史 `_Test` 目录后原命令 full exit 0，未重新生成；`bin/msix` 仅有 9,990,289 字节主 MSIX；portable 接收唯一 MSIX 和目标架构 XAML appx 精确路径，不再扫描目录 |
| CP-701 | DONE | 活跃构建静态门禁 | 2026-08-23，活跃 CMake、工具、CI 与 GitHub workflow 的禁止模式扫描为 0 |
| CP-702 | DONE | 运行时进程树门禁 | 2026-08-23，完整 Release full 的实际进程树只包含 CMake/Ninja/SDK 工具链，无 MSBuild 子进程 |
| CP-703 | DONE | x64 Release/Debug 干净 full | x64 Release full 于 2026-08-23 复跑成功；此前 Debug full 亦已通过 |
| CP-704 | BLOCKED | x86/arm64 支持矩阵验证 | x86 TerminalControl 实链通过；本机 VS 缺少 ARM64 ATL/MFC 库，最终三架构 WPF 包验证已由 Azure matrix 承担 |
| CP-705 | DONE | README/CI 清理 MSBuild 用法 | 活跃 pipeline/workflow 已改用 CMake/Ninja；静态门禁扫描为 0 |
| CP-801 | DONE | 产品闭包旧工程清理 | `microsoft/src` 现存 vcxproj 为 0，产品 `.wapproj` 已删除，生成 Ninja 命令无旧工程路径 |
| CP-802 | DONE | 历史构建树和旧计划收口 | 2026-08-23，已移除 32 个受版本控制的 `build.before-relocation` CMake/Ninja/旧 MSBuild 生成文件；本机未跟踪缓存未删除 |

## 6. 每阶段通用验收规则

每个目标迁移都必须同时检查：

1. **干净性**：删除该目标的输出和生成目录后能重建。
2. **增量性**：立即再构建为 no-op；修改一个输入只触发必要节点。
3. **等价性**：比较文件清单、PE machine、imports/exports、manifest、资源、WinMD、PDB 存在性和包内容。
4. **隔离性**：临时从 PATH 隐藏 `msbuild`，新目标仍成功；生成 Ninja 文件无旧工程路径。
5. **可诊断性**：缺少 SDK/工具时在配置阶段失败，错误包含工具名、搜索位置和修复建议。
6. **不双写**：一个产物只能由一个 CMake target 拥有，不能由旧链路覆盖。
7. **警告预算**：新警告为零；既有 PRI 警告建立清零曲线，不能静默忽略。

## 7. 风险与待决策项

| ID | 风险/决策 | 当前处理 |
| --- | --- | --- |
| R-01 | XAML 编译器的直接调用接口可能不稳定 | P4 先做最小 spike，失败时必须显式决策，不保留隐藏 MSBuild |
| R-02 | WinMD/CppWinRT 依赖顺序复杂 | P3 单独完成，不与最终 EXE 一次性迁移 |
| R-03 | `.props/.targets` 含条件属性，静态抄写容易遗漏 | P0 生成 evaluated inventory，逐目标对比 |
| R-04 | 现有源树内有大量 `Generated Files` | 新链只写 build/obj；验证从空生成目录重建 |
| R-05 | WorkspaceExtension 曾由 CMake/MSBuild 双构建双写 | CP-502 已切断两个 MSBuild ProjectReference；交付阶段只注入 CMake 产物，旧工程文件删除，不保留第二入口 |
| R-06 | `dotnet build` 被误认为已去 MSBuild | L2 和 CP-601 明确禁止 |
| R-07 | portable 可能不需要完整 VS test layout/appxsym | P6 先确认交付契约，不复制无用遥测/侧载资产 |
| R-08 | 上游 `microsoft` 更新会继续改 `.vcxproj` | CMake 源清单/生成输入建立漂移检测，旧工程不再是事实来源 |
| R-09 | 无参数的首次 configure 无法由仓库内 CMake 代码强制选择 Ninja | 生成器选择发生在读取 `CMakeLists.txt` 之前；在“不改命令、不用 preset/wrapper、不要求外部环境设置”的约束下无仓库内解法，CP-103 明确阻塞，不增加伪兼容入口 |
| R-10 | 非当前 CMake 图的 pipeline PowerShell 链仍硬编码 MSBuild Roslyn | MSBuild wrapper 已删除；`New-UnpackagedTerminalDistribution.ps1` 仍被 `templates-v2/job-build-project.yml` 调用，不能误判为孤儿；必须先迁移该正式 pipeline 步骤，再删除调用点和脚本本身，不把它改造成兼容壳 |

P1 的编译器/SDK 自动定位仍可继续；CP-103 受 R-09 阻塞，不影响先迁移现有 Ninja 构建图中的 MSBuild 目标。

## 8. 文档持续更新规则

后续每次相关提交必须执行以下更新：

1. 修改顶部“最后更新”日期。
2. 更新第 5 节对应检查点状态；只有附带命令、exit code 和产物证据才能标记 `DONE`。
3. 若发现新依赖，更新第 2 节统计、第 7 节风险，并为其增加检查点，不能只写在提交信息中。
4. 每完成一个阶段，在本节末尾追加一条变更记录，包含日期、阶段、结果、证据和下一步。
5. 构建失败时记录首个根因，不把后续级联错误全部列成独立问题。

### 变更记录

- **2026-08-20 / P0 启动**：完成初始仓库调查、工程闭包统计、独立 Ninja 干净配置探针以及当前 `ext/full` 基线。确认 `full` 可通过但仍是 Ninja 包裹 MSBuild，存在 132 条 PRI 警告、WorkspaceExtension 双写和 AppxPackage 安装布局依赖。下一步执行 CP-006、CP-101 至 CP-104。
- **2026-08-21 / P1 进行中**：保持 `cmake -S . -B .\\build`、`cmake --build .\\build` 和现有目标用法不变，移除 Ninja 重新配置对外部 `INCLUDE`/`LIB` 环境变量的要求；路径由当前 CMake 已选定的 MSVC 编译器和固定 Windows SDK 版本直接确定，不增加 preset、wrapper、兼容分支或备用入口。原命令已完成 CMake configure/generate、WorkspaceExtension 编译链接、PortablePackageTool 生成和 ext-repack，exit 0、0 警告、0 错误。用户级 NuGet 配置读取失败已确认只是沙箱权限限制，在允许读取该配置后同一命令通过。下一步验证空构建目录下的原始 configure 命令；CP-101/CP-102 尚未完成。
- **2026-08-21 / P1 生成器检查**：确认 CMake 4.4.2 在无 `CMAKE_GENERATOR` 时默认选择 Visual Studio 18；生成器选择早于仓库 `CMakeLists.txt` 执行。因不可变约束禁止修改命令、preset、wrapper 和外部环境前置要求，CP-103 标记为 `BLOCKED`，不通过增加另一条入口规避。继续迁移现有 Ninja 构建图中的 MSBuild 调用，下一项为 CP-601。
- **2026-08-21 / CP-601 完成**：从 `full` 和 `ext` 直接删除两处 `dotnet build PortablePackageTool.csproj`，由一个 CMake 输出规则直接调用固定 .NET SDK 8.0.421 的 `Roslyn/bincore/csc.dll`，显式提供 net8.0 reference assemblies、原 csproj 的 implicit usings 语义和 runtimeconfig；无调用者的旧 csproj 随即删除，不保留第二入口。原命令 `cmake --build .\\build` 完成编译及 repack，exit 0；第二次构建未重编工具；当前 `build` Ninja 规则无 `dotnet build` 或 `PortablePackageTool.csproj`。`build.before-relocation` 中仍有旧生成规则，归 CP-802 清理，不属于当前构建图。下一步移除 portable launcher 对 `MSBuild/Current/Bin/Roslyn/csc.exe` 的路径依赖。
- **2026-08-21 / CP-605 完成**：删除当前 CMake 对 VS `MSBuild/Current/Bin/Roslyn/csc.exe` 的查找；PortablePackageTool 直接用当前 dotnet host 执行固定 SDK 的 `csc.dll`，并显式引用仓库已有的 .NET Framework 4.7.2 reference assemblies。原命令 exit 0，生成 `WindowsTerminalPortableGeekEdition_System_0.0.1.0_x64.exe`；当前 Ninja 规则无旧路径和旧参数。另发现 `microsoft/build/scripts/New-UnpackagedTerminalDistribution.ps1` 仍有同类硬编码，已登记为 R-10，下一步处理该脚本。
- **2026-08-21 / full 回归**：按原命令 `cmake --build .\\build --target full` 完成完整回归，exit 0。MSBuild 主体耗时 2 分 34 秒，仍为 132 条既有 PRI263 警告；新的 SDK Roslyn launcher 链完成 full-repack，耗时 21 秒。日志再次确认 `.wapproj` 仍从 VS `AppxPackage` 目录调用 `mspdbcmf.exe` 并复制侧载脚本、资源和 telemetry assemblies。下一步先固定 `.wapproj` 的 package map、MSIX、appxsym、Dependencies 和测试布局产物契约，再实施 CP-602/603；旧 PowerShell 第二入口不做补丁式兼容。
- **2026-08-21 / CP-504 完成**：删除根 CMake 对 `Host.Proxy.vcxproj`、`TerminalControl/dll/TerminalControl.vcxproj` 和 `TerminalSettingsModel/dll/Microsoft.Terminal.Settings.Model.vcxproj` 的三次重复调用，不保留开关或回退。仅由剩余 `.wapproj -> WindowsTerminal.vcxproj` 传递图执行原命令 `cmake --build .\\build --target full`，exit 0；MSBuild 2 分 43 秒、132 条既有警告，full-repack 23 秒。根显式 MSBuild 调用由 4 个降为 1 个，但该调用仍承载完整产品闭包，下一步必须迁移而不是包装它。
- **2026-08-21 / CP-105 完成**：Ninja 生成器下直接将 `CMAKE_MAKE_PROGRAM` 固定为 PATH 中单独安装的 `C:/ProgramData/chocolatey/bin/ninja.exe` 1.13.2，现有缓存不再调用 VS 内置 Ninja。切换时因旧 `.ninja_log` 版本不兼容发生一次完整 WorkspaceExtension 重编；随后再次执行原命令 exit 0，只运行既定 ext-repack，未重编 C++ 或 C#。这不解决 R-09 的首次无参数生成器选择问题，二者保持分开。
- **2026-08-21 / CP-606 基线**：当前 package map 共 254 项：221 PNG、12 DLL、10 WinMD、4 EXE、3 ICO，HTML/JSON/PRI/XML 各 1。测试布局额外包含 181,130,010 字节 appxsym、19,719,169 字节四架构 XAML dependencies、15 个侧载资源、7 个 telemetry 文件及安装脚本；PortablePackageTool 实际只读取主 MSIX 和目标架构 Microsoft.UI.Xaml appx。决定直接关闭 WAP symbol/test layout，并由 CMake 显式传递目标架构 XAML appx；不保留测试布局扫描回退。
- **2026-08-21 / CP-606 首次验证未通过**：CMake 已显式传入目标架构 `Microsoft.UI.Xaml.2.8.appx`，PortablePackageTool 已删除从 WAP 测试布局搜索依赖包的旧逻辑；原命令 `cmake --build .\\build --target full` 及 full-repack 均 exit 0，MSBuild 89 秒、repack 23 秒。但 `bin/msix/CascadiaPackage_0.0.1.0_x64_Test` 在本次构建中仍被更新，证明 `/p:AppxSymbolPackageEnabled=false /p:AppxTestLayoutEnabled=false` 没有完整关闭 WAP 测试布局。CP-606 保持 `TODO`，下一步定位 `.wapproj`/AppxPackage targets 的实际控制属性，不把“构建通过”误记为迁移完成。
- **2026-08-21 / CP-606 完成**：确认 `AppxTestLayoutEnabled=false` 只关闭 `_CreateTestLayout` 和安装脚本，WAP 默认仍把主包写到 `AppxPackageTestDir`。当前 CMake 直接指定唯一 `AppxPackageOutput=bin/msix/CascadiaPackage_0.0.1.0_x64.msix`；PortablePackageTool 的参数由目录 `--package-root` 直接改为文件 `--package`，删除递归扫描、配置匹配和“选择最新包”逻辑，不保留旧参数。清除历史 Release/Debug `_Test` 生成目录后，原命令 `cmake --build .\\build --target full` exit 0（MSBuild 87 秒、132 条既有 PRI 警告；repack 20 秒），未重新生成 `_Test`、appxsym、Dependencies、侧载脚本或 telemetry；随后原命令 `cmake --build .\\build` exit 0，ext-repack 3 秒。此前试验遗留的 `build/presets` 生成树也已删除，仓库根无 preset 文件。下一步进入 CP-602/CP-603，直接替换剩余 `.wapproj` 和 AppxPackage targets。
- **2026-08-21 / CP-502 首次切断验证失败**：已从 `TerminalApp.vcxproj` 和 `WindowsTerminal.vcxproj` 删除对 `WorkspaceExtension.vcxproj` 的两个传递引用，MSBuild 不再进入该工程；首次尝试用 WAP `Content` 打包 CMake 产物时，WAP 将源路径错误重写为 `CascadiaPackage\\WorkspaceExtension\\WorkspaceExtension.dll`，原 `full` 在 APPX0702 失败（132 条既有警告、1 错误）。CP-502 保持 `TODO`；下一步使用 WAP 原生 `WapProjPackageFile` 的完整源路径/TargetPath 元数据验证，成功前不删除旧工程文件，也不恢复双构建引用。
- **2026-08-21 / CP-502 第二次切断验证失败**：静态 `WapProjPackageFile` 已使 `package.map.txt` 得到正确的绝对源路径和根目标路径，但 `GenerateAppxPackageRecipe` 仍将 recipe payload 解释为相对的 `WorkspaceExtension\\WorkspaceExtension.dll`，原 `full` 再次 APPX0702。CP-502 继续保持 `TODO`；改为直接声明最终 `AppxPackagePayload`，避免让 WAP 再次转换一个并非来自 MSBuild ProjectReference 的 CMake 产物。
- **2026-08-21 / CP-502 第三次 WAP payload 验证失败并停止该方向**：直接 `AppxPackagePayload` 仍被 `GenerateAppxPackageRecipe` 重写为工程内相对路径并 APPX0702。继续增加 WAP 元数据会形成迁移期兼容层，因此停止该方向并删除这些试验项。当前直接让 full-repack 与 ext-repack 使用同一个 CMake `WorkspaceExtension` 输出目录；MSIX 不再拥有该 DLL，最终 portable 交付由唯一 CMake 产物注入。下一次 full 必须同时验证 MSIX 不含该 DLL、portable 仍含该 DLL、MSBuild 不进入旧 vcxproj。
- **2026-08-21 / CP-502 完成**：从 `TerminalApp.vcxproj`、`WindowsTerminal.vcxproj` 直接删除 `WorkspaceExtension.vcxproj` ProjectReference，不设置开关、回退或兼容路径。原命令 `cmake --build .\\build --target full` exit 0；MSBuild 70 秒、132 条既有 PRI 警告、0 错误，full-repack 20 秒。生成的 MSIX 中 WorkspaceExtension 条目为 0；最终 portable footer 为 `WTPORT01`，内嵌 ZIP 中只有一个 562,176 字节 `WorkspaceExtension.dll`，并带一个 696 字节 `WorkspaceExtension.pri`，二者均从 CMake 输出目录覆盖注入。随后删除无人调用的 `WorkspaceExtension.vcxproj` 和 `.filters`，原命令 `cmake --build .\\build` 再次 exit 0，ext-repack 4 秒；跟踪源码和当前 Ninja 文件均无旧工程引用。
- **2026-08-21 / CP-602 打包边界清点**：当前 WAP `package.map.txt` 共 254 项，来源明确分成 192 个 `microsoft/res` 品牌图片、36 个 `microsoft/src` 文件、21 个 `microsoft/bin` 二进制/WinMD 和 4 个 `microsoft/packages` 运行时/WinMD，另含生成的 `AppxManifest.xml` 与 `resources.pri`。其中 `microsoft/bin` 的 21 项是产品 ProjectReference 图的输出；其余为打包层输入。下一步先用 CMake 明确生成 manifest、PRI 和 map，再由 Windows SDK MakeAppx 产出同一路径 MSIX；切换时删除 `.wapproj` 调用，不保留旧打包入口。
- **2026-08-21 / CP-503 顺序根因**：CMake `WorkspaceExtension` 当前链接 `ConTypes`、`ConProps`、`WinRTUtils`、TerminalConnection、Control、Settings、UI 等 11 个仍由产品 MSBuild 图生成的库，但 `full` 把 WorkspaceExtension 设成 MSBuild 之前的普通依赖；因此已有输出下可通过，清理 `microsoft/bin/obj` 后必然先链接失败。修正方向是同一 `full` 内严格执行“产品图 → WorkspaceExtension → portable 重打包”，不增加命令、备用目标或旧顺序分支。完成全部库原生化后再移除中间的产品 MSBuild 步骤。
- **2026-08-21 / CP-503 顺序修复验证**：删除精确的 `microsoft/bin/x64/Release/WorkspaceExtension`、`microsoft/obj/x64/Release/WorkspaceExtension` 和 `build/CMakeFiles/WorkspaceExtension.dir` 后，首次沙箱内原命令因 MSBuild `FileTracker` 系统路径访问被拒绝而在产品图失败（`E_ACCESSDENIED`），不是源码或顺序错误；同一原命令 `cmake --build .\\build --target full` 在允许 FileTracker 后 exit 0。时间线严格为 msix-full 74 秒 → CMake WorkspaceExtension 从空对象重编/链接 156 秒 → full-repack 20 秒；随后原命令 `cmake --build .\\build` exit 0，ext-repack 5 秒。CP-503 继续保持 `TODO`，因为尚未删除并重建整个 `microsoft/bin/obj`，不能用局部清理冒充完成。
- **2026-08-21 / CP-705 旧入口清理边界**：已删除重复执行 MSBuild、扫描包目录并调用另一个脚本的 `Build-PortableTerminalDistribution.ps1` legacy wrapper；同步删除 `.github/copilot-instructions.md`、`ext/README.md`、`microsoft/doc/building.md` 和旧 ext 计划中的兼容入口说明，不提供替代壳。全仓已无该脚本引用，原命令 `cmake --build .\\build` exit 0、ext-repack 4 秒。`New-UnpackagedTerminalDistribution.ps1` 仍被 `templates-v2/job-build-project.yml` 的正式 unpackaged 产物步骤直接调用，必须先把该 pipeline 步骤切到当前 PortablePackageTool，再同时删除。CP-705 保持 `TODO`，直到 CI 和文档只剩三条既定 CMake 命令。
- **2026-08-21 / CP-104 完成**：删除 `PORTABLE_MSBUILD_PATH`、`PORTABLE_PLATFORM_TOOLSET`、`PORTABLE_VCPKG_ROOT`、`PORTABLE_TARGET_FRAMEWORK_ROOT_PATH` 四个覆盖入口以及 `find_program(MSBuild)`、`vswhere`、VS17/VS18 VC targets 分支。从 CMake 已选定的 `CMAKE_CXX_COMPILER` 唯一推导 MSVC toolset 与 VS 根目录，固定当前 v145/.NET 4.7.2/vcpkg 布局；配置阶段不再搜索或校验 MSBuild。PATH 无 msbuild 的普通 PowerShell 中，原命令 `cmake --build .\\build` 自动 configure/generate 后 exit 0；因命令签名变化首次完整重编 WorkspaceExtension，随后第二次原命令未重编 C++/C#、ext-repack 3 秒。原命令 `cmake --build .\\build --target full` 回归也 exit 0：msix-full 73 秒、132 条既有警告，WorkspaceExtension no-op，full-repack 20 秒。唯一剩余 full MSBuild 路径随 `.wapproj` 在 CP-602 一起删除。
- **2026-08-21 / CP-602 manifest 生成规则确认**：WAP 生成 manifest 的根级 WinRT 注册共有 7 个 DLL、151 个类，所有线程模型为 `both`。直接读取对应 WinMD 的 TypeDefinition custom attributes，取带 `Windows.Foundation.Metadata.ActivatableAttribute`、`ComposableAttribute` 或 `StaticAttribute` 的 runtime class 并集，得到 Control/Settings.Editor/Settings.Model/TerminalConnection/UI.Markdown/UI/TerminalApp 分别 10/45/59/4/3/4/26 个类；与当前 WAP `AppxManifest.xml` 七组集合逐项比较差异均为 0。下一步由直接 Roslyn 编译的工具按此规则从 WinMD 生成注册，不签入 151 项静态副本。
- **2026-08-21 / CP-602 manifest 生成实现**：`PortablePackageTool generate-manifest` 已纳入同一个 CMake 直接 Roslyn 规则，从 `Package-Dev.appxmanifest` 和固定 7 组 DLL/WinMD 生成 product manifest；缺失输入、属性或非 `both` 线程模型立即失败，无静态类清单和回退路径。原命令 `cmake --build .\\build` exit 0；生成的基础 manifest 与 WAP 输出在排除 WAP 专属 `build:Metadata` 和无语义顺序后完全一致，151 个注册类集合差异为 0。该子命令是 CP-602 内部生成器，不是新的编译入口。
- **2026-08-21 / CP-402 PRI 合同探针**：直接调用 Windows SDK `MakePri.exe new`，使用上述新 manifest 以及当前 WAP 的 `priconfig.xml`/三份 resfile 清单作为一次性对照基线，exit 0；输出 resource map 为 `WindowsTerminalDev`，1571 个 named resources、23575 个 candidates。生成 PRI 与 WAP `resources.pri` 字节级相同，SHA-256 均为 `19FA50B9EEC3DA539E134EA3A0EC3DC18E27A8F5CEF8B4F327F75E94440F5687`，Detailed dump 也字节级相同。这只证明新 manifest 可以无差异进入 MakePri；CP-402/602 仍为 `TODO`，因为正式规则不得读取 WAP `obj` 中的 config/list。下一步从固定源输入生成自有 PRI config/list，然后再做同样的字节级对比。
- **2026-08-21 / CP-402 自有 PRI 输入生成**：`PortablePackageTool generate-pri-config` 已从固定产品输入生成 `priconfig.xml`、226 项 layout、90 项 RESW、13 项 PRI 和空 embed 清单，过程不读取 `CascadiaPackage/obj`；数量不符或文件缺失直接失败。使用该配置直接运行 MakePri exit 0，仍为 `WindowsTerminalDev` / 1571 named resources / 23575 candidates / 既有 PRI263 警告。因资源和文件输入枚举顺序不同，PRI 二进制 SHA-256 不同；对两份 Detailed dump 按“resource URI + candidate type + qualifier 集 + value”归一化比较，23575/23575 个候选差异为 0。CP-402 仍为 `TODO`：该生成器尚未成为唯一正式 package 规则，不会在 WAP 尚存时增加并行打包入口。
- **2026-08-21 / CP-602 MakeAppx 合同探针**：新 `generate-package-map` 从固定源目录和产品输出生成 253 项映射，校验输入存在、数量和目标唯一性，不读取 WAP `package.map.txt`。Windows SDK `MakeAppx.exe pack` exit 0；新旧 MSIX 均有 255 个 ZIP entries，路径集差异为 0。解包后 254 个文件中仅 `AppxManifest.xml`、`resources.pri` 和由二者派生的 `AppxBlockMap.xml` 哈希不同，其余 251 个文件字节级相同；manifest 注册集和 PRI 23575 个候选已分别证明语义等价。下一步将这条链接入原 `full` 目标并同时删除 `.wapproj` 调用，不保留新旧 package 并行路径。
- **2026-08-21 / CP-402/602/603/604 完成**：原 `full` 目标已唯一执行“6 个尚未原生化的产品根工程 → WinMD 驱动 manifest → 自有 PRI config/list → Windows SDK MakePri → 自有 253 项 map → MakeAppx”；根 CMake 不再调用 WAP，`CascadiaPackage.wapproj` 和 `OpenConsole.slnx` 中的对应项目已同时删除，无开关或回退。删除后再次执行原命令 `cmake --build .\\build --target full` exit 0：产品编译与直接 package 约 83 秒，WorkspaceExtension no-op，full-repack 26 秒；随后默认原命令 exit 0。当前 CMake/Ninja 无 `.wapproj`、`AppxPackageOutput/Dir` 或 `MSBuild\\...\\AppxPackage` 引用。下一步从 6 个产品 `.vcxproj` 根中选取无 XAML 的叶子 EXE/DLL，每迁移一个就删除对应 MSBuild 工程和调用。
- **2026-08-21 / CP-204 启动**：选定 `wt` 与 `elevate-shim` 作为第一批最终 EXE。两者均无 ProjectReference，各只有一个 C++ 源、一个 RC 图标资源和 `onecore.lib`；现有 Release/x64 tlog 已固定 `/std:c++20`、`/MT`、Windows subsystem、CFG、Unicode/branding定义及输出路径合同。实施时直接把两者加入现有 CMake/Ninja 图，并在同一个改动中删除根 `full` 的两次 MSBuild 调用、两个 vcxproj 和 `OpenConsole.slnx` 条目；不增加目标外入口、兼容开关或旧路径回退。验证顺序固定为目标编译、PE/import/manifest/resource 检查、原 `cmake --build .\\build --target full`、原 `cmake --build .\\build` 和 Ninja/源码旧工程引用扫描。
- **2026-08-21 / CP-204 完成**：`wt` 与 `elevate-shim` 已由 CMake 直接编译各自 C++/RC、静态 CRT 并链接 `onecore.lib`，输出仍为 `microsoft/bin/x64/Release/wt.exe` 和 `elevate-shim.exe`。切换时同步删除根 `full` 两次 MSBuild 调用、两个 vcxproj 与 `OpenConsole.slnx` 条目，不留兼容分支。与切换前 MSIX 中二进制相比，两者均为 x64、Windows GUI、CFG，import DLL 集合差异 0，asInvoker manifest 逐行差异 0，资源目录 RVA/大小一致；新 MSIX 中 `wtd.exe` 与 `elevate-shim.exe` 分别和当前 Ninja 输出 SHA-256 相同。沙箱内首次 `full` 仅因剩余 MSBuild FileTracker 的既有 `E_ACCESSDENIED` 失败；允许 FileTracker 后同一原命令 exit 0，产品与 package 约 59 秒、WorkspaceExtension 1 秒、full-repack 20 秒；随后默认原命令 exit 0。当前 CMake 和 Ninja 对两个旧工程的活跃引用为 0，MSBuild 根调用由 6 个降为 4 个。
- **2026-08-21 / CP-205 启动**：剩余根中不先拆 `Host.Proxy`，因为它直接生成两份 MIDL 接口头和 proxy/stub 源，并被 server/OpenConsole 多处作为硬工程依赖；单独删除会留下断裂的 MSBuild 图。先迁移 ShellExtension：其固定源为 `pch.cpp`、`OpenTerminalHere.cpp`、`dllmain.cpp` 和 `.def`，依赖当前 WindowsTerminal 闭包已生成的 `ConTypes.lib`、`WinRTUtils.lib`，无自有 MIDL/XAML 生成。实施顺序固定为剩余产品 MSBuild 图 → CMake ShellExtension/PCH/link → package；切换同时删除其 vcxproj、solution 条目和根调用，不保留旧入口。
- **2026-08-21 / CP-205 完成**：ShellExtension 已由 CMake/Ninja 直接构建，根 MSBuild 调用、vcxproj、solution 条目和其中两个 ProjectReference 同步删除。首次链接发现 Windows SDK projection 为 C++/WinRT 2.0.220110.5，而现存 `WinRTUtils.lib` 固定为 2.0.250303.1；正式规则因此直接调用仓库固定 `Microsoft.Windows.CppWinRT.2.0.250303.1/bin/cppwinrt.exe`，以 SDK 10.0.22621.0 metadata 在 `build` 内生成 projection，不读取旧 ShellExtension obj。新旧 DLL 的 3 个 exports、29 个 import DLL、x64/Windows GUI/CFG/无资源目录合同差异均为 0。原 `cmake --build .\\build --target full` exit 0：产品与 package 约 57 秒、WorkspaceExtension no-op、full-repack 30 秒；默认原命令随后 exit 0。MSIX 内 DLL 与 Ninja 输出 SHA-256 相同，三个本轮删除工程的当前 CMake/Ninja/solution 活跃引用为 0，根 MSBuild 调用降至 3 个。
- **2026-08-21 / CP-505 启动**：确认 `Host.EXE.vcxproj` 直接引用 `server.vcxproj`，后者直接引用 `Host.Proxy.vcxproj` 并消费其生成的 MIDL 头；最近一次原 `full` 中 `OpenConsoleProxy.lastbuildstate` 已在 Host.EXE 阶段更新。根 CMake 的后续 Host.Proxy 调用因此是重复构建，不是独立产物所有者。直接删除该根调用，不增加开关；Host.Proxy 工程暂留在唯一 `Host.EXE -> server` 传递链，后续与 MIDL/server 闭包同时原生化并删除。
- **2026-08-21 / CP-505 完成**：根 CMake 中对 `Host.Proxy.vcxproj` 的重复显式调用已直接删除，不增加兼容开关或备用入口。原命令 `cmake --build .\\build --target full` exit 0；`Host.EXE -> server -> Host.Proxy` 传递链仍生成并打包 `OpenConsoleProxy.dll`，产品编译与直接 package 约 54 秒，WorkspaceExtension no-op，full-repack 31 秒；随后原命令 `cmake --build .\\build` exit 0。静态检查确认根 CMake 仅剩 `WindowsTerminal.vcxproj` 和 `Host.EXE.vcxproj` 两次显式 MSBuild 调用，当前 CMake/Ninja 中不存在 `Host.Proxy.vcxproj` 根调用。`Host.Proxy.vcxproj` 仍是现有 server/MIDL 闭包的一部分，必须在该闭包原生化时一起删除，不能孤立迁移。
- **2026-08-21 / CP-206 启动**：核对工程和 Release/x64 tlog 后确认，`Host.Proxy` 的产物闭包是两份 IDL、MIDL 生成的两个头和五个 C 源、一个 `.def` 以及最终 DLL/导入库；真实 vcxproj `ProjectReference` 只有 `server.vcxproj` 一处，其余 `OpenConsole.slnx` 项是 solution 调度依赖。实施边界为：`full` 首先由 CMake/Ninja 直接运行 SDK MIDL 并生成 `OpenConsoleProxy.dll`，仍在既定 `microsoft/obj/x64/Release/OpenConsoleProxy` 生成消费者所需头文件；随后现有两个产品根消费该输出。同一改动删除 server 的旧 ProjectReference、`Host.Proxy.vcxproj` 和全部 solution 条目，不保留 MSBuild 代理目标或备用生成路径。验证包括 exports/imports/PE 合同、生成头消费、原 `full`、默认原命令和旧工程引用为 0。
- **2026-08-21 / CP-206 完成**：CMake 现在正式启用 MSVC C 编译，直接调用 Windows SDK 10.0.22621.0 MIDL 生成两个接口头和五个 proxy/stub C 源，再由 Ninja 编译、链接既定 `microsoft/bin/x64/Release/OpenConsoleProxy.dll`。旧 `Host.Proxy.vcxproj`、server 的唯一 ProjectReference 和 solution 中全部调度依赖已删除，不留旧目标。首次生成修正了 MIDL 的预处理器 PATH 与 SDK IDL include；随后发现接口 IDL 不实际生成 TLB，删除错误的 TLB byproduct 声明后第二次目标构建为 `ninja: no work to do`。沙箱内 `full` 仅因剩余 MSBuild FileTracker 的既有 `E_ACCESSDENIED` 失败；允许 FileTracker 后同一原命令 exit 0，server 编译命令明确消费 Ninja 生成头，MSIX 收录新 DLL，产品与 package 约 90 秒、WorkspaceExtension no-op、full-repack 20 秒；默认原命令随后 exit 0。新 DLL 为 x64/Windows GUI，`DllCanUnloadNow`、`DllGetClassObject`、`DllRegisterServer`、`DllUnregisterServer`、`GetProxyDllInfo` 五个导出齐全；旧工程在 CMake/Ninja/solution 中的引用均为 0。
- **2026-08-21 / CP-207 启动**：剩余根为 `WindowsTerminal.vcxproj` 与 `Host.EXE.vcxproj`。下一步先统计两条传递图中的纯 C++ 静态叶子库，选择消费者集合明确、无 MIDL/XAML/自定义生成且源文件数量可控的一批；实施前固定 Release/x64 tlog 和输出路径。选中目标将直接加入现有 CMake/Ninja `full` 顺序，所有消费者删除旧 ProjectReference 并显式消费唯一 Ninja `.lib`，同一改动删除旧 vcxproj 与 solution 条目，不增加开关、备用工程或新用法。验证仍只使用 `cmake -S . -B .\\build`、`cmake --build .\\build`、`cmake --build .\\build --target full`。
- **2026-08-21 / CP-207 目标确定**：选择 `microsoft/src/audio/midi/lib/midi.vcxproj`。它是无 ProjectReference、无 MIDL/XAML/自定义生成的静态叶子库，仅编译 `precomp.cpp` 与 `MidiAudio.cpp`，输出 `microsoft/bin/x64/Release/MidiAudio.lib`。旧依赖入口共三处：`host-common.vcxitems`、`terminalcore-lib.vcxproj`、`TerminalControlLib.vcxproj`；Release/x64 最终 link tlog 确认实际只由 `Host.EXE` 和 `Microsoft.Terminal.Control.dll` 消费。迁移时先由 Ninja 生成唯一 `MidiAudio.lib`，两个最终链接工程显式链接该固定产物，再删除三处 ProjectReference、旧 vcxproj 和 solution 项；不保留 MSBuild MidiAudio 目标。
- **2026-08-21 / CP-207 完成**：根 CMake 已直接建立 `MidiAudio` 静态库，使用目标级 PCH 编译 `MidiAudio.cpp`，并把 `.lib` 与编译 PDB 固定到 `microsoft/bin/x64/Release`；`full` 的 native-product-foundation 在两个剩余 MSBuild 根之前生成它。`Host.EXE.vcxproj` 与 `TerminalControl/dll/TerminalControl.vcxproj` 的最终链接依赖显式指向该唯一 Ninja `.lib`，三处旧 ProjectReference、`midi.vcxproj`、`OpenConsole.slnx` 项和 `conhost.slnf` 项均已删除。旧/new archive 都是 2 个 object member，`dumpbin /linkermember:1` 均为 449 个符号，差异只有 CMake PCH 翻译单元的内部标记名。`cmake -S . -B .\\build`、单目标构建及第二次 no-op、`cmake --build .\\build` 均 exit 0；`cmake --build .\\build --target full` exit 0，实际 Host.EXE 与 Microsoft.Terminal.Control 链接命令都记录了 `microsoft/bin/x64/Release/MidiAudio.lib`，MSIX 与 full-repack 完成。首次产品重生成约 6 分 57 秒、repack 21 秒；构建仍会报告现有 MSBuild 库共有的 LNK4020 PDB 类型警告，不影响本次链接与打包。当前旧工程文件不存在，源码构建图、solution 与 filter 中 `midi.vcxproj` 引用为 0。
- **2026-08-21 / CP-208 启动**：继续从两个剩余产品根的实际构建闭包中选择单一纯 C++ 静态叶子库。准入条件不变：无 MIDL、XAML、资源编译和自定义生成，无下游 ProjectReference，源文件与最终链接消费者可完整枚举。选中后先固定 Release/x64 编译与 archive 合同，再由 CMake/Ninja 直接生成原输出路径，删除全部旧 ProjectReference、solution/filter 条目和 vcxproj；验证只使用既定三条原命令。
- **2026-08-21 / CP-208 目标确定**：选择 `microsoft/src/renderer/uia/lib/uia.vcxproj`。它无 ProjectReference、MIDL、XAML、RC 或自定义生成，只用 PCH 编译 `UiaRenderer.cpp`，输出 `microsoft/bin/x64/Release/ConRenderUia.lib`。旧工程引用只有 `TerminalControlLib.vcxproj`、`Control.UnitTests.vcxproj`、`OpenConsole.slnx` 的工程项/调度依赖；Release/x64 产品最终由 `Microsoft.Terminal.Control.dll` 链接该 archive。迁移将建立唯一 Ninja `ConRenderUia` 目标，产品 DLL 与测试工程显式链接固定 `.lib`，随后删除两处 ProjectReference、solution 条目和旧 vcxproj，不保留旧调度入口。
- **2026-08-21 / CP-208 完成**：根 CMake 已直接建立 `ConRenderUia` 静态库，使用目标级 PCH 编译 `UiaRenderer.cpp`，`.lib` 与编译 PDB 固定到 `microsoft/bin/x64/Release`，并纳入 `full` 的 native-product-foundation。`TerminalControlLib.vcxproj` 与 `Control.UnitTests.vcxproj` 的旧 ProjectReference 已删除；最终 `TerminalControl.vcxproj` 和控制层单测显式链接唯一 Ninja `ConRenderUia.lib`。旧 `uia.vcxproj` 及 `OpenConsole.slnx` 工程项/调度依赖同步删除。旧/new archive 都是 2 个 object member，`dumpbin /linkermember:1` 均为 512 个符号，差异仅为 PCH 内部标记。`cmake -S . -B .\\build`、单目标构建及第二次 no-op、`cmake --build .\\build --target full`、`cmake --build .\\build` 均 exit 0；实际 Microsoft.Terminal.Control 链接命令明确包含 `microsoft/bin/x64/Release/ConRenderUia.lib`，包创建成功，full-repack 22 秒。工程引用删除触发一次约 6 分 46 秒的 WinRT/XAML 重生成；旧工程文件不存在且源码构建图/solution 中引用为 0。
- **2026-08-21 / CP-209 启动**：继续检查两个剩余产品根的实际构建闭包，只选择无代码生成、无下游 ProjectReference、可完整固定源码与最终消费者的纯 C++ 静态叶子库。选中后直接建立唯一 CMake/Ninja 目标并接管原输出路径，同时删除旧 vcxproj、ProjectReference 和 solution/filter 调度项；不增加 preset、包装命令、兼容分支或备用入口，验证仍只使用既定三条原命令。
- **2026-08-21 / CP-209 目标确定**：选择 `microsoft/src/internal/internal.vcxproj`。它无 ProjectReference、MIDL、XAML、RC 或自定义生成，只用 PCH 编译 `stubs.cpp`，输出 `microsoft/bin/x64/Release/ConInt.lib`。6 个旧直接工程引用中，Host、fuzzer、Host 单测、Win32 interactivity 单测和 propsheet 都是最终链接目标；`Microsoft.Terminal.Settings.ModelLib` 是静态中间库，其实际代码由 Settings Model DLL、Settings Model 单测和 TerminalApp 单测最终链接。迁移将建立唯一 Ninja `ConInt` 目标，并把固定 `.lib` 明确加入上述 8 个最终链接目标；随后删除 6 个 ProjectReference、solution/filter 条目和旧 vcxproj，不保留旧调度入口。
- **2026-08-21 / CP-209 完成**：根 CMake 已直接建立 `ConInt` 静态库，使用目标级 PCH 编译 `stubs.cpp`，`.lib` 与编译 PDB 固定到 `microsoft/bin/x64/Release`，并在两个剩余 MSBuild 产品根之前由 `native-product-foundation` 生成。Host、fuzzer、Host 单测、Win32 interactivity 单测、propsheet、Settings Model DLL、Settings Model 单测和 TerminalApp 单测共 8 个最终链接目标显式使用唯一 `ConInt.lib`；`Microsoft.Terminal.Settings.ModelLib` 等 6 个旧 ProjectReference、`OpenConsole.slnx`、`conhost.slnf` 条目和 `internal.vcxproj` 已同步删除。旧/new archive 都是 2 个 object member，`dumpbin /linkermember:1` 均为 341 个符号。`cmake -S . -B .\\build`、单目标构建及第二次 no-op、`cmake --build .\\build`、`cmake --build .\\build --target full` 均 exit 0；实际 Settings Model DLL 与 OpenConsole 链接命令明确包含 `microsoft/bin/x64/Release/ConInt.lib`，MSIX 创建成功，full-repack 22 秒。删除工程引用触发一次约 6 分钟的 Settings Model/Editor/TerminalApp WinRT/XAML 重生成；旧工程和 GUID/path 引用均为 0，修改后的 XML/JSON 全部可解析，`git diff --check` 仅报告既有换行提示。当前剩余 69 个 vcxproj、1 个 wapproj、13 个 csproj、199 个 ProjectReference、378 个 Import。
- **2026-08-21 / CP-210 启动**：选择下一项已确认无 MIDL、XAML、RC、自定义生成和下游 ProjectReference 的纯 C++ 静态叶子库 `microsoft/src/tsf/tsf.vcxproj`。先复核 2 个实际源码、PCH、Release/x64 编译与 archive 合同，并把静态中间库引用展开到最终链接目标；随后由唯一 Ninja `ConTSF` 接管原输出路径，同时删除旧 vcxproj、ProjectReference、solution/filter 项，不保留旧调度入口。
- **2026-08-21 / CP-210 目标确定**：`tsf.vcxproj` 只用 PCH 编译 `Handle.cpp`、`Implementation.cpp`，输出 `microsoft/bin/x64/Release/ConTSF.lib`。6 个旧直接引用中，Host、fuzzer、Host 单测、Win32 interactivity 单测和 Adapter 单测是最终链接目标；`TerminalControlLib` 是静态中间库，其 TSF 代码最终由 TerminalControl DLL 和 Control 单测链接。因此迁移固定 7 个最终链接入口，建立唯一 Ninja `ConTSF` 后删除 6 个 ProjectReference、solution 的工程项/调度依赖、filter 项和旧 vcxproj。
- **2026-08-21 / CP-210 完成**：根 CMake 已直接建立 `ConTSF` 静态库，使用目标级 PCH 编译 `Handle.cpp`、`Implementation.cpp`，`.lib` 与编译 PDB 固定到 `microsoft/bin/x64/Release`，并由 `native-product-foundation` 在两个剩余 MSBuild 产品根之前生成。TerminalControl DLL、Control 单测、Host、fuzzer、Host 单测、Win32 interactivity 单测和 Adapter 单测共 7 个最终链接目标显式使用唯一 `ConTSF.lib`；`TerminalControlLib` 等 6 个旧 ProjectReference、`OpenConsole.slnx`、`conhost.slnf` 条目和 `tsf.vcxproj` 已同步删除。旧/new archive 均为 3 个 object member，`dumpbin /linkermember:1` 均为 1086 个符号；旧产物 8,214,034 字节，新产物 8,214,336 字节。`cmake -S . -B .\\build`、单目标构建及第二次 no-op、`cmake --build .\\build`、`cmake --build .\\build --target full` 均 exit 0；实际 TerminalControl DLL 与 OpenConsole 链接 tlog 明确包含 `microsoft/bin/x64/Release/ConTSF.lib`，MSIX 创建成功，full-repack 21 秒，完整 full 约 6 分 55 秒。旧工程和 GUID/path 引用均为 0，7 个显式链接入口完整，修改后的 XML/JSON 全部可解析，`git diff --check` 仅报告既有换行提示。当前剩余 68 个 vcxproj、1 个 wapproj、13 个 csproj、193 个 ProjectReference、374 个 Import。
- **2026-08-21 / CP-211 启动**：从剩余产品闭包筛出无 ProjectReference、MIDL、XAML、RC 和自定义生成的 `microsoft/src/renderer/gdi/lib/gdi.vcxproj`。它只用 PCH 编译 `invalidate.cpp`、`math.cpp`、`paint.cpp`、`state.cpp`，输出 `microsoft/bin/x64/Release/ConRenderGdi.lib`；旧引用只有 Host、fuzzer、Host 单测、Win32 interactivity 单测和 Adapter 单测 5 个最终链接目标，以及 solution/filter 调度项。先保存并核对 Release/x64 archive、编译参数与实际链接合同，再建立唯一 Ninja `ConRenderGdi`，同步删除 5 个 ProjectReference、solution/filter 项和旧 vcxproj，不保留旧入口。
- **2026-08-21 / CP-211 首次 full 验证失败并定位**：Ninja 源码与 PCH 编译、归档均成功，允许 FileTracker 后产品实际链接命令也已消费新的 `ConRenderGdi.lib`，但 OpenConsole 报 4 个 Uniscribe `Script*` 符号未解析。根因是旧工程的 `<Lib><AdditionalDependencies>usp10.lib` 属于 archive 输入合同，会在 `lib.exe` 阶段把 SDK import library 合入产物；初版只迁移了源码。正式修正为 Ninja 归档直接使用固定 Windows SDK 的 `usp10.lib`，不在最终消费者中散落补库，不增加开关或备用入口；随后重跑原三条命令和 archive 符号检查。
- **2026-08-21 / CP-211 完成**：根 CMake 已直接建立 `ConRenderGdi` 静态库，使用目标级 PCH 编译 4 个实现源码，并在归档阶段直接合入固定 Windows SDK `usp10.lib`，输出仍为 `microsoft/bin/x64/Release/ConRenderGdi.lib`。Host、fuzzer、Host 单测、Win32 interactivity 单测和 Adapter 单测 5 个最终链接目标显式使用该唯一产物；5 个旧 ProjectReference、`OpenConsole.slnx`、`conhost.slnf` 条目和 `gdi.vcxproj` 已同步删除。旧/new archive 均为 48 个 member，其中 43 个为 USP10 import member；`dumpbin /linkermember:1` 均为 1527 个 public symbol，旧产物 9,745,622 字节，新产物 9,746,416 字节。修正后 `cmake -S . -B .\\build`、`cmake --build .\\build`、`cmake --build .\\build --target full` 均 exit 0；实际 OpenConsole 链接命令明确包含 `ConRenderGdi.lib` 且不再有 Uniscribe 未解析符号，产品链 0 错误、MSIX 创建成功、full-repack 34 秒，完整 full 约 1 分 43 秒。旧工程和 GUID/path 引用为 0，修改后的 XML/JSON 全部可解析，`git diff --check` 仅报告既有换行提示。当前剩余 67 个 vcxproj、1 个 wapproj、13 个 csproj、188 个 ProjectReference、370 个 Import。
- **2026-08-21 / CP-212 启动**：继续迁移无 ProjectReference、MIDL、XAML、RC 和自定义生成的 `microsoft/src/interactivity/base/lib/InteractivityBase.vcxproj`。它用 PCH 编译 8 个实现源码，输出 `microsoft/bin/x64/Release/ConInteractivityBaseLib.lib`；旧引用只有 Host、fuzzer、Host 单测、Win32 interactivity 单测、Adapter 单测和 Parser 单测 6 个最终链接目标，以及 solution/filter 项。实施固定为唯一 Ninja archive 接管原路径，6 个最终目标显式链接，随后同步删除 6 个 ProjectReference、solution/filter 项和旧 vcxproj，不保留旧调度入口。
- **2026-08-21 / CP-212 首次编译验证失败并定位**：全量重编译在 `RemoteConsoleControl.cpp` 的两个无参 `WI_ASSERT_FAIL()` 处失败；当前仓库固定 WIL 1.0.250325.1 的宏合同要求消息参数，而旧 MSBuild archive 因增量缓存一直未重新编译该源，旧 CL tlog 的定义与 Ninja 已一致，排除编译选项遗漏。直接把两个断言改为带具体消息的当前 API 用法，不降低 `/WX`、不改 WIL、不增加条件定义或兼容分支，随后继续原命令验证。
- **2026-08-21 / CP-212 完成**：根 CMake 已直接建立 `ConInteractivityBaseLib` 静态库，使用目标级 PCH 编译 8 个实现源码，输出固定为 `microsoft/bin/x64/Release/ConInteractivityBaseLib.lib`。Host、fuzzer、Host 单测、Win32 interactivity 单测、Adapter 单测和 Parser 单测 6 个最终链接目标显式使用唯一产物；6 个旧 ProjectReference、solution/filter 项和 `InteractivityBase.vcxproj` 已删除。旧/new archive 均为 9 个 member、1674 个 public symbol，大小分别为 9,639,262 和 9,640,862 字节。`cmake -S . -B .\\build`、`cmake --build .\\build`、修正后的 `cmake --build .\\build --target full` 均 exit 0；实际 OpenConsole 链接命令明确包含新 archive，产品与打包 0 错误，MSIX 创建成功，full-repack 25 秒，完整 full 约 1 分 35 秒。旧工程和 GUID/path 引用为 0，XML/JSON 可解析，`git diff --check` 仅有既有换行提示。当前剩余 66 个 vcxproj、1 个 wapproj、13 个 csproj、182 个 ProjectReference、366 个 Import。
- **2026-08-21 / CP-213 启动**：继续迁移无 ProjectReference、MIDL、XAML、RC 和自定义生成的 `microsoft/src/renderer/base/lib/base.vcxproj`。它用 PCH 编译 8 个实现源码，输出 `microsoft/bin/x64/Release/ConRenderBase.lib`。10 个旧直接 ProjectReference 中，`TerminalControlLib` 和 `TerminalCore-lib` 是静态中间库；将二者展开后，最终消费者固定为 TextBuffer 单测、TerminalControl DLL、Control 单测、TerminalCore 单测、Host、fuzzer、Host 单测、Win32 interactivity 单测和 Adapter 单测共 9 个。建立唯一 Ninja archive 后同步删除 10 个旧 ProjectReference、solution/filter 项和旧 vcxproj，不保留旧入口。
- **2026-08-21 / CP-213 完成**：根 CMake 已直接建立 `ConRenderBase` 静态库，使用目标级 PCH 编译 8 个实现源码，输出固定为 `microsoft/bin/x64/Release/ConRenderBase.lib`。`TerminalControlLib`、`TerminalCore-lib` 两个静态中间库不再携带旧工程引用，TextBuffer 单测、TerminalControl DLL、Control 单测、TerminalCore 单测、Host、fuzzer、Host 单测、Win32 interactivity 单测和 Adapter 单测共 9 个最终目标显式链接唯一产物；10 个旧 ProjectReference、solution/filter 项和 `base.vcxproj` 已删除。旧/new archive 均为 9 个 member、1685 个 public symbol，大小分别为 9,880,244 和 9,881,088 字节。三条原命令均 exit 0；实际 TerminalControl DLL 与 OpenConsole 链接命令都明确包含新 archive，产品链 0 错误、MSIX 创建成功、full-repack 23 秒。删除中间库引用触发一次约 7 分 17 秒的 TerminalControl、Settings、TerminalApp WinRT/XAML 级联重生成，完整 full 约 8 分 35 秒。旧工程和 GUID/path 引用为 0，XML/JSON 可解析，`git diff --check` 仅有既有换行提示。当前剩余 65 个 vcxproj、1 个 wapproj、13 个 csproj、172 个 ProjectReference、362 个 Import。
- **2026-08-21 / CP-214 启动**：继续迁移 `microsoft/src/terminal/parser/lib/parser.vcxproj`。实际编译集合由工程私有的 `InputStateMachineEngine.cpp` 与 `parser-common.vcxitems` 中的 `OutputStateMachineEngine.cpp`、`stateMachine.cpp`、`tracing.cpp`、`base64.cpp` 组成，使用 `precomp.h` PCH，输出 `microsoft/bin/x64/Release/ConTermParser.lib`，无 MIDL/XAML/RC/自定义生成及下游 ProjectReference。旧直接引用中的 `TerminalControlLib`、`TerminalCore-lib` 是静态中间库；实施时展开到 TerminalControl DLL，并保留所有测试/fuzzer 最终消费者的显式链接，随后删除全部旧 ProjectReference、solution/filter 项和旧 vcxproj，不保留旧入口。
- **2026-08-21 / CP-214 完成**：根 CMake 已直接建立 `ConTermParser` 静态库，使用目标级 PCH 编译工程私有源码与 `parser-common.vcxitems` 的 5 个实现源码，输出固定为 `microsoft/bin/x64/Release/ConTermParser.lib`。`TerminalControlLib`、`TerminalCore-lib` 两个静态中间库不再携带旧工程引用，TerminalControl DLL、Control 单测、TerminalCore 单测、Host、fuzzer、Host 单测、Win32 interactivity 单测、Adapter 单测、Parser fuzzer 和 Parser 单测共 10 个最终目标显式链接唯一产物；11 个旧 ProjectReference、solution/filter 项和 `parser.vcxproj` 已删除。旧/new archive 均为 6 个 member、1763 个 public symbol，大小分别为 9,094,968 和 9,096,082 字节。`cmake -S . -B .\\build`、`cmake --build .\\build`、`cmake --build .\\build --target full` 均 exit 0；实际 TerminalControl DLL 与 OpenConsole 链接 tlog 明确包含新 archive，产品链 0 错误、MSIX 创建成功、full-repack 22 秒。删除静态中间库引用触发一次 TerminalControl、Settings、TerminalApp WinRT/XAML 级联重生成，完整 full 于 13:03 完成。旧工程和 GUID/path 引用为 0，10 个显式链接入口完整，XML/JSON 可解析；当前剩余 64 个 vcxproj、1 个 wapproj、13 个 csproj、161 个 ProjectReference。
- **2026-08-21 / CP-215 启动**：继续只从现有产品构建闭包选择无代码生成、依赖可完整展开的纯 C++ 静态叶子工程。先核对工程与共享 vcxitems 的真实源码、PCH、Release/x64 编译/归档输入、旧 ProjectReference 和实际 link tlog；确定目标后立即把结论回填本计划，再建立唯一 CMake/Ninja 产物、展开到最终消费者并删除旧 vcxproj、solution/filter 项和全部旧引用。不增加 preset、包装命令、旧入口、兼容分支或备用路径。
- **2026-08-21 / CP-215 目标确定**：选择 `microsoft/src/propslib/propslib.vcxproj`。它只用 `precomp.h` PCH 编译 `DelegationConfig.cpp`、`RegistrySerialization.cpp`、`ShortcutSerialization.cpp`、`TrueTypeFontList.cpp` 和 `precomp.cpp`，输出 `microsoft/bin/x64/Release/ConProps.lib`，无 ProjectReference、MIDL、XAML、RC、自定义生成或附加归档输入。10 个旧 ProjectReference 均可直接落到最终链接目标：TerminalApp DLL、Settings Model DLL、Settings Model 单测、TerminalApp 单测、Host、Host fuzzer、Host 单测、Win32 interactivity 单测、propsheet DLL 和 Adapter 单测。迁移建立唯一 Ninja `ConProps` 后，把固定 archive 显式加入这 10 个最终链接入口，并立即删除全部旧 ProjectReference、solution/filter 项和旧 vcxproj。
- **2026-08-21 / CP-215 首次编译验证失败并定位**：Ninja 成功生成 `ConProps` PCH，但各实现源码再次显式包含 `precomp.h`，导致头内 `PopEntryList`、`PushEntryList` 两个函数重复定义。旧 MSBuild 的 `/Yu"precomp.h"` 会在该 include 处直接切入 PCH，因而掩盖了把函数实现放进 PCH 头的问题；CMake 的标准 PCH 包装头会先包含真实头。常规 include guard 与 `#pragma once` 都不能跨这个 MSVC PCH 切入点消除已恢复的函数实体，因此不保留这些无效改法。全仓检索确认两个 helper 只被 `TrueTypeFontList.cpp` 使用，正式修正为把实现移入该唯一消费者的匿名命名空间，让 PCH 只承载声明/头文件状态；不增加构建分支、备用 PCH 路径或兼容入口，随后继续原三条命令。
- **2026-08-21 / CP-215 PCH 失效处理**：第一次失败生成的 PCH 没有留下可用的头依赖记录，移动 helper 后普通对象仍加载了旧 PCH 中的函数实体。对照旧 CL tlog 同时发现 `ConProps` 目标漏迁 `/Zc:preprocessor`；把该正式编译选项补到目标级合同，使 PCH 命令签名变化并由 Ninja 正常重建。不手工删除产物，不增加清理脚本或另一套构建命令。
- **2026-08-21 / CP-215 archive 合同校正**：首次成功产物相对旧 archive 多 1 个 member、少 2 个 public symbol。前者是 CMake PCH 翻译单元与旧 `precomp.cpp` 被同时归档，后者是两个 helper 临时进入匿名命名空间。正式目标删除冗余 `precomp.cpp` 输入，只保留 CMake 的 PCH 翻译单元；helper 仍移到唯一消费者 `.cpp`，但保持原外部链接名，以恢复旧 archive 的 5-member/12492-symbol 合同。
- **2026-08-21 / CP-215 完成**：根 CMake 已直接建立 `ConProps` 静态库，用目标级 PCH 编译 4 个实现源码，补齐旧 CL 合同中的 `/Zc:preprocessor`，输出固定为 `microsoft/bin/x64/Release/ConProps.lib`。TerminalApp DLL、Settings Model DLL、Settings Model 单测、TerminalApp 单测、Host、Host fuzzer、Host 单测、Win32 interactivity 单测、propsheet DLL 和 Adapter 单测共 10 个最终目标显式链接唯一产物；10 个旧 ProjectReference、solution/filter 项和 `propslib.vcxproj` 已删除。旧/new archive 均为 5 个 member、12492 个 public symbol，大小分别为 19,065,574 和 19,066,406 字节。三条规定命令最终均 exit 0；实际 Settings Model DLL、TerminalApp DLL 与 OpenConsole 链接 tlog 明确包含新 archive，MSIX 创建成功，最终 full 约 1 分 58 秒、full-repack 13 秒。旧工程和 GUID/path 引用为 0，10 个显式链接入口完整，XML/JSON 可解析；当前剩余 63 个 vcxproj、1 个 wapproj、13 个 csproj、151 个 ProjectReference。
- **2026-08-21 / CP-216 启动**：继续扫描现有产品根的真实 MSBuild 闭包，优先选择无 MIDL/XAML/RC/自定义生成和下游 ProjectReference、且源码/PCH/archive/最终消费者都能一次固定的静态叶子工程。仍执行“先记录、后实施、旧入口当场删除”，不增加 preset、包装命令、兼容层、备用路径或已迁移工程的回退引用。
- **2026-08-21 / CP-216 目标确定**：选择 `microsoft/src/buffer/out/lib/bufferout.vcxproj`。它用已有 `#pragma once` 的 `precomp.h` PCH 编译 14 个实现源码和旧 `precomp.cpp`，输出 `microsoft/bin/x64/Release/ConBufferOut.lib`，无 ProjectReference、MIDL、XAML、RC、自定义生成或附加归档输入。旧 ProjectReference 共 11 个，其中 Adapter 单测重复声明两次，`TerminalControlLib`、`TerminalCore-lib` 是静态中间库；展开并去重后的最终链接入口固定为 TextBuffer 单测、TerminalControl DLL、Control 单测、TerminalCore 单测、Host、Host fuzzer、Host 单测、Win32 interactivity 单测和 Adapter 单测共 9 个。迁移时由唯一 Ninja `ConBufferOut` 接管原路径，删除 11 个旧引用、solution/filter 项和旧 vcxproj，不保留重复或中间层引用。
- **2026-08-21 / CP-216 完成**：根 CMake 已直接建立 `ConBufferOut` 静态库，用目标级 PCH 编译 14 个实现源码，输出固定为 `microsoft/bin/x64/Release/ConBufferOut.lib`。`TerminalControlLib`、`TerminalCore-lib` 不再携带旧引用；TextBuffer 单测、TerminalControl DLL、Control 单测、TerminalCore 单测、Host、Host fuzzer、Host 单测、Win32 interactivity 单测和 Adapter 单测共 9 个最终目标显式链接唯一产物。11 个旧 ProjectReference（含 Adapter 单测重复项）、全部 solution/filter 调度和 `bufferout.vcxproj` 已删除。旧/new archive 均为 15 个 member、3604 个 public symbol，大小分别为 13,360,062 和 13,361,436 字节。三条规定命令均 exit 0；实际 TerminalControl DLL 与 OpenConsole 链接 tlog 明确包含新 archive，MSIX 创建成功，完整 full 约 6 分 40 秒、full-repack 25 秒。旧工程和 GUID/path 引用为 0，9 个显式链接入口完整，XML/JSON 可解析；当前剩余 62 个 vcxproj、1 个 wapproj、13 个 csproj、140 个 ProjectReference。
- **2026-08-21 / CP-217 启动**：继续从 TerminalControl/OpenConsole 的实际链接闭包筛选下一纯 C++ 静态叶子库，完整核对源码、PCH、archive 附加输入、旧引用和最终链接 tlog后再实施。规则不变：唯一 CMake/Ninja 目标接管原输出，静态中间层展开到最终消费者，旧 vcxproj、solution/filter 项和全部引用同批删除，不增加任何备用入口。
- **2026-08-21 / CP-217 候选排除与目标确定**：`renderer/atlas` 虽无 ProjectReference，但工程实际编译 4 个 HLSL shader 并把生成头作为 C++ 输入，不符合本批纯 C++ 准入，不能遗漏 shader 后冒充完成。正式选择 `microsoft/src/server/lib/server.vcxproj`：它用 `precomp.h` PCH 编译 19 个实现源码，输出 `microsoft/bin/x64/Release/ConServer.lib`，无 ProjectReference、MIDL/XAML/RC/自定义生成或附加归档输入；唯一生成输入是已由原生 `OpenConsoleProxy` 目标产生的代理头目录，实施时建立明确目标依赖和 include 路径。5 个旧引用均为最终目标：Host、Host fuzzer、Host 单测、Win32 interactivity 单测、Adapter 单测。建立唯一 Ninja `ConServer` 后立即删除 5 个 ProjectReference、solution/filter 项和旧 vcxproj。
- **2026-08-21 / CP-217 完成**：根 CMake 已直接建立 `ConServer` 静态库，用目标级 PCH 编译 19 个实现源码，并通过目标依赖确保原生 OpenConsoleProxy 的 MIDL 头先生成；输出固定为 `microsoft/bin/x64/Release/ConServer.lib`。Host、Host fuzzer、Host 单测、Win32 interactivity 单测和 Adapter 单测共 5 个最终目标显式链接唯一产物；5 个旧 ProjectReference、solution/filter 项和 `server.vcxproj` 已删除。旧/new archive 均为 20 个 member、1629 个 public symbol，大小分别为 15,879,754 和 15,982,676 字节。三条规定命令均 exit 0；实际 OpenConsole 链接 tlog 明确包含新 archive，MSIX 创建成功，完整 full 约 1 分 16 秒、full-repack 13 秒。旧工程和 GUID/path 引用为 0，5 个显式链接入口完整，XML/JSON 可解析；当前剩余 61 个 vcxproj、1 个 wapproj、13 个 csproj、135 个 ProjectReference。
- **2026-08-21 / CP-218 启动**：继续检查剩余无 ProjectReference、无代码生成的静态叶子工程，优先处理消费者可直接枚举且不会掩盖 shader/MIDL/WinRT 输入的目标。仍只接受唯一 Ninja 产物和原三条命令，不保留旧工程或任何兼容/备用入口。
- **2026-08-21 / CP-218 目标确定**：选择 `microsoft/src/winconpty/lib/winconptylib.vcxproj`。它用 `precomp.h` PCH 编译 `winconpty.cpp` 及复用的 `server/DeviceHandle.cpp`、`server/WinNTControl.cpp`，输出 `microsoft/bin/x64/Release/conptylib.lib`，无 ProjectReference、MIDL/XAML/RC、实际代码生成或附加归档输入；工程尾部的 `GetPackagingOutputs` 只是 MSBuild 输出枚举，静态 archive 本身不应作为产品 payload。5 个旧引用都是最终链接目标：TerminalConnection DLL、Host FeatureTests、VtPipeTerm、winconpty DLL、winconpty FeatureTests。建立唯一 Ninja `conptylib` 后显式链接这 5 个目标，并删除旧 ProjectReference、solution/filter 项和 vcxproj。
- **2026-08-21 / CP-218 完成**：根 CMake 已直接建立 `conptylib` 静态库，用目标级 PCH 编译 `winconpty.cpp`、`DeviceHandle.cpp`、`WinNTControl.cpp`，输出固定为 `microsoft/bin/x64/Release/conptylib.lib`。TerminalConnection DLL、Host FeatureTests、VtPipeTerm、winconpty DLL、winconpty FeatureTests 共 5 个最终目标显式链接唯一产物；5 个旧 ProjectReference、solution/filter 项和 `winconptylib.vcxproj` 已删除。旧/new archive 均为 4 个 member、917 个 public symbol，大小分别为 8,171,964 和 8,174,490 字节。三条规定命令均 exit 0；实际 TerminalConnection DLL 链接 tlog 明确包含新 archive，MSIX 创建成功，完整 full 约 4 分 52 秒、full-repack 13 秒。旧工程和 GUID/path 引用为 0，5 个显式链接入口完整，XML/JSON 可解析；当前剩余 60 个 vcxproj、1 个 wapproj、13 个 csproj、130 个 ProjectReference。
- **2026-08-21 / CP-219 启动**：剩余纯 C++ 叶子已进入高扇出或特殊输入阶段。优先处理 `microsoft/src/types/lib/types.vcxproj`；`ConRenderAtlas` 的 HLSL 生成保留为后续独立代码生成检查点，不能把它误当纯 C++。只有消费者和 archive 合同全部固定后才实施删除，不用旧 ProjectReference 兜底。
- **2026-08-21 / CP-219 闭包固定**：按规范化绝对工程路径递归展开反向 ProjectReference 图，修正先前人工估算：`types.vcxproj` 有 32 个直接引用；其中 `TerminalAppLib`、`TerminalControlLib`、`terminalcore-lib`、`TerminalSettingsAppAdapterLib`、`Microsoft.Terminal.Settings.ModelLib`、`WinRTUtils`、`adapter`、`terminalinput` 共 8 个静态中间库。递归去重后的最终链接入口固定为 25 个：SampleApp、TextBuffer 单测、TerminalApp LocalTests/DLL/单测、TerminalConnection、TerminalControl DLL、Settings Editor、Settings Model DLL/单测、UIHelpers、UIMarkdown、Control 单测、TerminalCore 单测、WindowsTerminal、Host EXE/fuzzer/FeatureTests/单测、Win32 interactivity 单测、Adapter 单测、Parser fuzzer/单测、VtPipeTerm、Types 单测。实施将一次完成唯一 Ninja `ConTypes`、25 个显式最终链接入口、32 个旧引用及 solution/filter 项删除和旧 vcxproj 删除；随后核对 archive member/public symbol、实际产品 link tlog、XML/JSON、残留引用和三条规定命令，不在单个中间消费者处停。
- **2026-08-21 / CP-219 首次产品链接告警**：Ninja archive 已成功生成，旧/new 均为 15 个 member、2693 个 public symbol；默认构建 exit 0。首次 full 的 TerminalControl 实际链接命令已明确消费新 `ConTypes.lib`，但并行编译共享 `/Zi` PDB 后 link 报 `LNK4020` 类型记录损坏，属于新增告警，不能算完成。正式修正是在 `ConTypes` 目标补齐 MSVC `/FS` 同步写 PDB 合同；待当前级联构建结束后由原三条命令重建 archive/PDB 并重新 full，禁止忽略告警或改用另一套调试信息格式。
- **2026-08-21 / CP-219 `/FS` 验证结论**：补齐 `/FS` 后，`cmake -S . -B .\\build` 与 `cmake --build .\\build` 均 exit 0，15 个 `ConTypes` 对象和 archive 已按新命令行重编；随后 `cmake --build .\\build --target full` 也 exit 0，WindowsTerminal、MSIX、WorkspaceExtension 和 full-repack 全部完成，实际产品链接明确消费新 archive。但 TerminalControl 链接仍对 14 个实现对象报告 `LNK4020`，证明已有损坏的 `ConTypes.pdb` 被增量更新后没有自愈，不能把 `/FS` 本身记为修复。下一检查点只删除 `build/CMakeFiles/ConTypes.dir/Release`、`microsoft/bin/x64/Release/ConTypes.lib` 和 `ConTypes.pdb` 三处该目标生成物，再严格执行原三条命令验证从零生成的 PDB；不清理其他目标、不换编译入口、不忽略告警。CP-219 继续为 `TODO`。
- **2026-08-21 / CP-219 干净 `/Zi` PDB 仍损坏**：上述三处生成物已先经过绝对路径和工作区边界校验后精确删除；随后原命令 configure、默认构建均 exit 0，并从零生成 PCH、14 个实现对象、archive 和 `ConTypes.pdb`。原 `full` 再次 exit 0，但 TerminalControl、Settings Model、TerminalApp 等实际最终链接仍稳定报告 `ConTypes.lib(...): warning LNK4020`，因此排除旧 PDB 残留，也证明 `/FS` 不是修复。下一正式合同改为仅对 `ConTypes` 从公共选项副本中删除 `/Zi`，使用 `/Z7` 把 CodeView 类型信息写入各对象；同时删除该目标不再成立的共享 compile-PDB 属性和 `/FS`。随后再次精确清理对象/archive/PDB并执行原三条命令，验收条件是所有 `ConTypes` 消费链接中该警告为 0；不提供 `/Zi` 回退或双路径。
- **2026-08-21 / CP-219 `/Z7` 首次验证失败并定位**：目标已只保留 `/Z7`，默认构建从零成功；但原 `full` 在 TerminalConnection 链接 `utils.cpp.obj` 时由警告升级为 fatal `LNK1103: debug information corrupt`，产品步骤 exit 1，后续打包未执行。生成的 Ninja 命令确认对象同时使用 `/Z7 /GL` 和 CMake PCH；微软的调试格式合同表明 `/Z7` 将完整符号与类型信息写入对象，因此错误已从共享 PDB 明确收敛到 LTCG 对象内记录。下一检查点仅关闭 `ConTypes` archive 的 interprocedural optimization，使其使用 `/GL- /Z7`；最终 DLL/EXE 的链接优化不变。随后再次精确清理并执行原三条命令，若仍有 LNK1103/LNK4020 才继续缩小到具体源码，不恢复 `/Zi` 或保留双合同。
- **2026-08-21 / CP-219 完成**：`ConTypes` 最终采用单一 `/Z7` 非 LTCG 对象合同，完整调试类型信息保存在对象中，不再生成或引用共享 compile PDB。精确清理该目标对象/archive/PDB 后，`cmake -S . -B .\\build`、`cmake --build .\\build`、`cmake --build .\\build --target full` 均 exit 0；TerminalControl、Settings Model、Settings Editor、TerminalApp、WindowsTerminal、OpenConsole 等实际链接均消费唯一 `ConTypes.lib`，该库的 LNK4020/LNK1103 为 0，MSIX 和 full-repack 成功。旧/new archive 都是 15 个成员；关闭 LTCG 后 `dumpbin /linkermember:1` 的表示由 2693 行变为 1067 行、大小由 12,053,172 变为 10,666,980 字节，因此不伪称二进制格式相同，以 14 个实现源码+PCH成员、完整最终链接闭包和无调试损坏作为正式合同。旧工程路径/GUID 命中 0，32 个旧 ProjectReference 删除，25 个递归最终入口显式链接（另有原有 SampleAppLib 入口），37 个变更 XML 可解析；当前 67 个 vcxproj、98 个 ProjectReference。OpenConsole 仍暴露旧 `ConhostV2Lib.pdb` 的既有 LNK4020，登记为后续旧库迁移问题，不计作本目标成功。下一步 CP-220 原生化带 4 个 HLSL 生成头的 `ConRenderAtlas`，不得漏掉 shader 输入。
- **2026-08-21 / CP-220 启动**：目标固定为 `microsoft/src/renderer/atlas/atlas.vcxproj`。先清点 4 个 `FxCompile` 的 profile/entry/header 输出、C++ 源码/PCH、旧 ProjectReference、最终链接消费者和 archive 基线；随后用 CMake/Ninja 显式建立 shader 生成依赖与唯一 `ConRenderAtlas.lib`，同批删除旧工程、solution/filter 项和全部引用。验收必须包含删除单个生成头后的精确重建和原三条命令，不允许预生成头兜底、MSBuild shader 分支或备用入口。
- **2026-08-21 / CP-220 合同与闭包固定**：atlas 实际有 6 个 HLSL 输入：`custom_shader_ps.hlsl`、`custom_shader_vs.hlsl`、`shader_ps.hlsl`、`shader_vs.hlsl` 是 4 个编译单元，`dwrite_helpers.hlsl` 与 `shader_common.hlsl` 是 include-only 依赖；固定 Windows SDK 10.0.22621.0 x64 `fxc.exe` 命令分别用 `ps_4_0`/`vs_4_0`、`main`、`/WX /all_resources_bound /Zi /O3 /Zsb /Qstrip_debug /Qstrip_reflect` 生成同名 `.h`，其中 shader_ps 依赖两个 include，shader_vs 依赖 shader_common。C++ archive 用 `pch.h` 编译 11 个实现源码加 PCH 成员。旧直接 ProjectReference 为 7 个；递归展开 `TerminalControlLib`、`TerminalCore-lib`、`win32.LIB` 三个静态中间库后，最终链接消费者固定为 TerminalControl DLL、Control 单测、TerminalCore 单测、Host EXE、Host fuzzer、Host 单测、Win32 interactivity 单测、Adapter 单测共 8 个。实施一次完成 4 条 shader 规则、唯一 Ninja archive、8 个最终链接入口、7 个旧引用、solution/filter 项和旧 vcxproj 删除。
- **2026-08-21 / CP-220 首次 full 通过**：四条原生 `fxc` 规则生成的 PDB 哈希文件名与旧链一致，4 个生成头分别与 MSBuild 基线 SHA-256 逐字节相同。Ninja 用 `/Z7` 非 LTCG 合同生成 12-member `ConRenderAtlas.lib`，产品链接无该库的 LNK4020/LNK1103；首次原 `full` exit 0，TerminalControl 和 OpenConsole 实际链接明确消费新 archive，MSIX 与 full-repack 成功。旧/new archive 都是 12 个成员；因正式关闭该库 LTCG，public-symbol 表示由 3549 行变为 918 行，大小由 13,352,378 变为 11,090,602 字节，不声称格式相同。旧工程路径/GUID 命中 0，8 个最终链接文件完整。下一检查点精确删除唯一 `shader_vs.h` 后再次执行原 `full`，验证单条 shader 规则的依赖边和 Ninja restat 行为；其他三个 shader 头哈希和时间戳必须保持不变。
- **2026-08-21 / CP-220 完成**：在确认删除目标绝对路径位于工作区且仅指向 `microsoft/bin/x64/Release/RendererAtlas/shader_vs.h` 后，第二次执行原命令 `cmake --build .\\build --target full`，native foundation 只出现 `[1/2] Generating .../shader_vs.h`，整个 `full`、MSIX 和 repack exit 0。新头 SHA-256 仍为 `D89F26C55237A8BC55B92E736DB9272F9C609560...`，另外三个头的 SHA-256 与 UTC ticks 均未变化；因为重生成内容逐字节相同，Ninja restat 正确阻止 `BackendD3D.cpp`、archive 和下游链接的无效重建，故不把“必须继续重编”当成验收条件。最终 4 个 shader 头与旧基线逐字节一致，archive 成员数同为 12；8 个最终 vcxproj 都显式链接唯一 `ConRenderAtlas.lib`。补删 `OpenConsole.slnx` 的 BuildDependency 和 `conhost.slnf` 项后，旧 atlas 工程路径/GUID 命中为 0；66 个剩余 vcxproj 可解析，`git diff --check` exit 0。CP-220 关闭，不保留 MSBuild shader 分支、预生成头兜底或备用构建入口。
- **2026-08-21 / CP-221 启动**：下一目标固定为 `microsoft/src/host/lib/hostlib.vcxproj` 输出的 `ConhostV2Lib.lib`，直接处理 CP-219/CP-220 完整产品链接仍暴露的该旧库 PDB `LNK4020`。工程本体通过 `host-common.vcxitems` 导入源码，其中 `precomp.cpp` 创建 PCH，并额外依赖 `$(IntDir)..\\OpenConsoleProxy` 生成目录；当前旧 archive 基线为 18,448,224 字节。先锁定实际编译命令、OpenConsoleProxy 头依赖、archive 成员/符号基线及精确消费者闭包，再一次性加入唯一 Ninja archive、最终链接项并删除旧工程及全部 solution/filter/ProjectReference 引用；不提供 MSBuild 回退或备用入口。
- **2026-08-21 / CP-221 合同与闭包固定**：XML 与实际 librarian 输入确认 `host-common.vcxitems` 是 43 个实现源码加 `precomp.cpp`，旧 `ConhostV2Lib.lib` 正好 44 个成员、`dumpbin /linkermember:1` 为 5646 行 public-symbol 表示、大小 18,448,224 字节，基线已保存到 `build/migration-baseline/CP-221`。实际 Release x64 编译合同是 C++20、`/MT /W4 /WX /GR- /Zc:preprocessor`、公共 native product 定义/强制包含和 `precomp.h` PCH；新目标从第一版即采用已验证的 `/Z7 /GL-` 单一调试合同，避免复制已知损坏的共享 `/Zi` PDB。只有 `srvinit.cpp` 包含 MIDL 生成的 `ITerminalHandoff.h`，因此目标显式依赖既有 OpenConsoleProxy 生成目标并包含其唯一生成目录。旧直接 ProjectReference 精确为 Host EXE、Host fuzzer、Win32 interactivity 单测、Adapter 单测 4 个，均为最终链接工程；Host.UnitTests 引用的是另一个 `host.unittest.vcxproj`，不能因 GUID 文本误判。实施固定为 43 个实现源码+CMake PCH、4 个最终显式链接入口（fuzzer 两条条件 Link 均更新）、删除 4 个旧引用、solution/slnf 项、`hostlib.vcxproj` 与 filters；不保留旧项目或替代构建路径。
- **2026-08-21 / CP-221 第一次 full 的外部 MSBuild 跟踪器失败**：实施后 `cmake -S . -B .\\build` 与 `cmake --build .\\build` 均 exit 0；第一次原 `full` 的 native foundation 已从零执行 `ConhostV2Lib` 的 46 个 Ninja 步骤，但后续 `windows-terminal-product` 在尚未链接前 exit 1。失败同时来自三个未迁移旧工程 `terminalinput.vcxproj`、`WinRTUtils.vcxproj`、`TerminalSettingsAppAdapterLib.vcxproj`，共同堆栈为 `Microsoft.Build.Utilities.FileTracker` 静态初始化调用 `GetLongFilePath` 时抛出 `E_ACCESSDENIED`；这不是 host 源码、PCH、MIDL 依赖或 archive 失败。保留该失败作为残余 MSBuild 不稳定性的证据，下一动作核验新 archive 后原样重跑 `cmake --build .\\build --target full`，不加参数、不加 wrapper、不改变用户编译入口。
- **2026-08-21 / CP-221 完成**：新 `ConhostV2Lib.lib` 与旧基线同为 44 个成员；正式关闭 LTCG 后大小由 18,448,224 变为 17,645,574 字节，public-symbol 表示由 5646 行变为 2134 行，不伪称二进制格式相同。路径边界校验后只删除旧 MSBuild 遗留的 `microsoft/bin/x64/Release/ConhostV2Lib.pdb`；新 `/Z7` archive 不生成也不引用该文件。沙箱内第二次原 `full` 仍在同一批旧 MSBuild FileTracker 上 `E_ACCESSDENIED`，因此使用完全相同的 `cmake --build .\\build --target full` 在沙箱外验收，最终 Windows Terminal、OpenConsole、MSIX、WorkspaceExtension 和 full-repack 均 exit 0；OpenConsole 实际 link command/tlog 明确消费新 archive，`ConhostV2Lib` 的 LNK4020/LNK1103 为 0。4 个最终 vcxproj 显式链接、旧工程路径命中 0、旧 ProjectReference 删除 4 个；GUID `06ec...` 被另一个 `host.unittest.vcxproj` 历史复用，保留该合法命中而不误删。当前 65 个 vcxproj、87 个 ProjectReference、全部 XML 可解析，solution/slnf 有效，`git diff --check` exit 0。
- **2026-08-21 / CP-222 启动**：CP-221 的成功链接同时暴露一个可批量清除的遗留：早期已迁移到 Ninja 的静态库仍复制旧 MSBuild `/Zi + /GL` 共享 compile-PDB 合同，当前至少 `MidiAudio`、`ConRenderBase`、`ConTermParser` 在 OpenConsole 链接中稳定报 LNK4020；输出目录还存在 `MidiAudio`、`ConRenderUia`、`ConInt`、`ConTSF`、`ConRenderGdi`、`ConInteractivityBaseLib`、`ConRenderBase`、`ConTermParser`、`ConProps`、`ConBufferOut`、`ConServer`、`conptylib` 等同类 PDB。下一检查点把所有已原生化 STATIC 目标统一到一个明确的 archive 编译合同 `/Z7 /GL-`，删除各目标 COMPILE_PDB 属性和对应陈旧 PDB，不保留 `/Zi` 分支；OpenConsoleProxy 等 DLL 目标不在此批次。随后执行原三条命令，以完整产品链接中这些 Ninja archive 的 LNK4020/LNK1103 为 0 验收；剩余警告必须只来自尚未迁移的 MSBuild archive，并直接决定后续迁移顺序。
- **2026-08-21 / CP-222 完成**：新增的 `_native_product_archive_compile_options` 不是备用路径，而是所有 15 个 Ninja STATIC 目标唯一使用的正式合同：从公共选项复制后删除 `/Zi`、追加 `/Z7`；每个 STATIC 目标明确 `INTERPROCEDURAL_OPTIMIZATION FALSE`，CMake 中 `COMPILE_PDB_*` 属性计数归零。经工作区绝对路径边界校验精确删除 `MidiAudio`、`ConRenderUia`、`ConInt`、`ConTSF`、`ConRenderGdi`、`ConInteractivityBaseLib`、`ConRenderBase`、`ConTermParser`、`ConProps`、`ConBufferOut`、`ConServer`、`conptylib` 共 12 个陈旧 PDB，完整构建后均未重新出现。`cmake -S . -B .\\build`、`cmake --build .\\build`、沙箱外同文命令 `cmake --build .\\build --target full` 均 exit 0；foundation 一次性执行 89 步重编，Settings Model、TerminalApp、OpenConsole 等产品重链、MSIX 和 full-repack 全部通过，所列 Ninja archive 的 LNK4020/LNK1103 为 0。剩余已观察到的静态库调试警告只来自尚未迁移的 MSBuild archive，优先处理体量最小的 `TerminalInput`。
- **2026-08-21 / CP-223 启动**：目标固定为 `microsoft/src/terminal/input/lib/terminalinput.vcxproj` 输出的 `TerminalInput.lib`。工程只有 `mouseInput.cpp`、`terminalInput.cpp` 两个实现源码和创建 `precomp.h` 的 `precomp.cpp`，当前旧 archive/PDB 大小分别为 8,258,360/5,468,160 字节；直接旧 ProjectReference 为 TerminalControlLib、Control 单测、TerminalCore 单测、adapter 静态库 4 个，solution 另有一个工程项和一个 BuildDependency。下一检查点保存 archive 成员/符号基线，递归展开 TerminalControlLib 与 adapter 的最终链接闭包，固定编译选项和消费者集合，然后一次性加入唯一 Ninja archive、删除旧工程/filters/solution/ProjectReference，并执行原三条命令；不保留旧库路径或 MSBuild 分支。
- **2026-08-21 / CP-223 合同与闭包固定**：旧 `TerminalInput.lib` 基线已保存到 `build/migration-baseline/CP-223`，精确为 `mouseInput.obj`、`terminalInput.obj`、`precomp.obj` 3 个成员，`dumpbin /linkermember:1` 为 931 行 public-symbol 表示。编译合同与公共原生静态库一致：C++20、`/MT /W4 /WX /GR- /Zc:preprocessor`、公共定义/强制包含、`precomp.h` PCH，并从第一版使用唯一 `/Z7 /GL-` archive 合同。递归展开 TerminalControlLib、adapter、TerminalCore-lib 三个静态中间库后，最终链接入口固定为 TerminalControl DLL、Control 单测、TerminalCore 单测、Host EXE、Host fuzzer、Host 单测、Win32 interactivity 单测、Adapter 单测、Parser 单测共 9 个；fuzzer 两条条件 Link 都必须更新。实施一次完成 Ninja target、9 个最终入口、4 个旧 ProjectReference、solution/slnf/BuildDependency 和旧 vcxproj 删除；旧 `TerminalInput.pdb` 精确删除且不得再生成。
- **2026-08-21 / CP-224 启动**：下一目标固定为 `microsoft/src/terminal/adapter/lib/adapter.vcxproj` 输出的 `ConTermAdapt.lib`，直接消除完整产品链接仍出现的该 MSBuild archive。工程清单初步确认是 `adaptDispatch.cpp`、`FontBuffer.cpp`、`InteractDispatch.cpp`、`MacroBuffer.cpp`、`PageManager.cpp`、`SixelParser.cpp`、`adaptDispatchGraphics.cpp`、`terminalOutput.cpp` 8 个实现源码，加创建 `precomp.h` 的 `precomp.cpp`。在任何实现修改前，先保存旧 archive/PDB 基线，解析所有直接 ProjectReference 并递归展开静态中间层到最终链接入口，核对 solution/slnf/BuildDependency；随后一次性建立唯一 `/Z7 /GL-` Ninja archive、显式最终链接并删除旧工程及全部调度引用，不保留 MSBuild 分支、兼容开关或第二种编译用法。验收仍只执行既定三条命令。
- **2026-08-21 / CP-224 合同与闭包固定**：旧 `ConTermAdapt.lib` 已保存到 `build/migration-baseline/CP-224`，大小 13,563,778 字节，精确为 8 个实现对象加 `precomp.obj` 共 9 个 member，`dumpbin /linkermember:1` 为 2588 行 public-symbol 表示；旧共享 `ConTermAdapt.pdb` 为 7,016,448 字节。直接 ProjectReference 共 7 个：TerminalCore-lib 静态中间库，以及 Host EXE、Host fuzzer、Host 单测、Win32 interactivity 单测、Adapter 单测、Parser 单测 6 个最终工程。递归展开 TerminalCore-lib 与其 TerminalControlLib 中间层后，最终入口固定为 TerminalControl DLL、Control 单测、TerminalCore 单测、Host EXE、Host fuzzer、Host 单测、Win32 interactivity 单测、Adapter 单测、Parser 单测共 9 个；fuzzer 的两个条件链接项都要更新。新目标从第一版只采用标准 `/Z7 /GL-` archive 合同和 CMake PCH；实施同批删除 7 个旧引用、solution/slnf/BuildDependency、vcxproj/filters，并精确删除旧 PDB。
- **2026-08-21 / CP-223 完成**：唯一 Ninja `TerminalInput.lib` 与旧基线同为 3 个 member；采用 `/Z7 /GL-` 后大小由 8,258,360 变为 5,678,534 字节，`dumpbin /linkermember:1` 表示由 931 行变为 364 行，不伪称 archive 二进制格式相同。旧 `TerminalInput.pdb` 经精确删除后没有重新生成。`cmake -S . -B .\build`、`cmake --build .\build`、沙箱外完全同文的 `cmake --build .\build --target full` 均 exit 0；完整 WinRT 产品、WindowsTerminal、OpenConsole、MSIX、WorkspaceExtension 和 full-repack 成功，Host.EXE 的实际 link tlog 明确包含新库。9 个最终 vcxproj 显式链接，4 个旧 ProjectReference、solution BuildDependency/project、conhost.slnf 项和旧 vcxproj 删除，旧路径/GUID残留为 0；CP-224 删除下一工程前的基线为 64 个 vcxproj、83 个 ProjectReference，XML/solution/slnf 可解析，`git diff --check` 无错误。
- **2026-08-21 / CP-224 完成**：唯一 Ninja `ConTermAdapt.lib` 与旧基线同为 9 个 member；采用 `/Z7 /GL-` 后大小由 13,563,778 变为 10,773,978 字节，`dumpbin /linkermember:1` 表示由 2588 行变为 788 行。旧 `ConTermAdapt.pdb` 精确删除后未重生。既定三条命令均 exit 0；完整 WinRT 产品链耗时约 4 分 22 秒，随后 OpenConsole、MSIX、WorkspaceExtension 和 full-repack 全部成功，Host.EXE 的真实 link command/tlog 明确消费新库，未出现该库的 LNK4020/LNK1103。9 个最终 vcxproj 显式链接，7 个旧 ProjectReference、solution project、旧 vcxproj/filters 同批删除，旧路径/GUID残留 0；当前 63 个 vcxproj、76 个 ProjectReference，XML/solution/slnf 可解析，`git diff --check` 无错误。
- **2026-08-21 / CP-225 启动**：CP-224 完整 OpenConsole 链接已把剩余静态库调试损坏明确收敛到 `ConInteractivityWin32Lib.lib`：至少 `ConsoleInputThread.obj`、`WindowDpiApi.obj`、`ConsoleKeyInfo.obj`、`Icon.obj`、`Find.obj`、`Menu.obj`、`windowUiaProvider.obj` 等对象稳定报告旧共享 `ConInteractivityWin32Lib.pdb` 的 LNK4020。下一目标固定为 `microsoft/src/interactivity/win32/lib/win32.LIB.vcxproj`。先保存 archive/PDB 基线，核对 17 个工程内实现源码及可能导入的 shared items、PCH、系统库合入方式、所有直接 ProjectReference 和递归最终链接入口；合同固定后一次性加入唯一 `/Z7 /GL-` Ninja archive、删除旧工程与 solution/slnf/ProjectReference，不保留旧 PDB 或兼容入口，仍只用既定三条命令验收。
- **2026-08-21 / CP-225 合同与闭包固定**：旧 `ConInteractivityWin32Lib.lib` 基线已保存到 `build/migration-baseline/CP-225`，大小 11,357,510 字节，精确为 16 个实现对象加 `precomp.obj` 共 17 个 member，`dumpbin /linkermember:1` 为 2141 行 public-symbol 表示；旧共享 PDB 为 9,187,328 字节。工程没有 shared-items 导入或额外生成输入，只有公共 native product 合同、`_WINDLL` 定义、`src/inc` 附加包含目录和 `precomp.h` PCH。直接 ProjectReference 共 5 个且全部是最终链接工程：Host EXE、Host fuzzer、Host 单测、Win32 interactivity 单测、Adapter 单测；fuzzer 的两个条件 Link 都要加入新库。实施固定为 16 个实现源码+CMake PCH、唯一 `/Z7 /GL-` archive、5 个最终入口，删除 5 个旧引用、solution/slnf 项和 vcxproj/filters，并精确删除旧 PDB。
- **2026-08-21 / CP-225 完成**：唯一 Ninja `ConInteractivityWin32Lib.lib` 与旧基线同为 17 个 member；采用 `/Z7 /GL-` 后大小由 11,357,510 变为 10,912,764 字节，`dumpbin /linkermember:1` 表示由 2141 行变为 1360 行。旧 9,187,328 字节共享 PDB 精确删除且未重生。既定三条命令均 exit 0；完整 WindowsTerminal 产品、OpenConsole、MSIX、WorkspaceExtension 和 full-repack 全部成功，Host.EXE 实际 link command/tlog 明确消费新库，该库的 LNK4020/LNK1103 为 0。5 个最终 vcxproj 显式链接，5 个旧 ProjectReference、solution/slnf 项、旧 vcxproj/filters 删除，旧路径/GUID残留 0；顺带删除 `conhost.slnf` 中此前已删除的 Host.Proxy 和 adapter 两个失效项目项。当前 62 个 vcxproj、71 个 ProjectReference，XML/JSON 可解析，`git diff --check` 无错误。
- **2026-08-21 / CP-226 启动**：下一目标固定为 `microsoft/src/cascadia/WinRTUtils/WinRTUtils.vcxproj` 输出的 `WinRTUtils.lib`。工程本体为 `LibraryResources.cpp`、`ScopedResourceLoader.cpp`、`Utils.cpp` 3 个实现源码和创建 `pch.h` 的 `pch.cpp`，没有自身 IDL 或 ProjectReference，但通过 `cppwinrt.build.*.props` 消费 Windows SDK platform projection。先保存 archive 基线，锁定当前 cppwinrt 工具版本、projection 输出目录/输入 WinMD、实际编译 include/define 合同和全部最终消费者；随后由 Ninja 明确生成或拥有所需 projection，不能偷用旧 MSBuild 中间目录作为隐式兜底。合同通过后同批删除旧 vcxproj/filters、solution/slnf/ProjectReference，验收仍只用既定三条命令。
- **2026-08-21 / CP-226 合同与闭包固定**：旧 `WinRTUtils.lib` 基线已保存到 `build/migration-baseline/CP-226`，大小 15,289,526 字节，精确为 3 个实现对象加 `pch.obj` 共 4 个 member，`dumpbin /linkermember:1` 为 1499 行 public-symbol 表示；旧共享 PDB 为 13,365,248 字节。实际编译使用 C++20、`WINRT_LEAN_AND_MEAN`、`__WRL_NO_DEFAULT_LIB__`、`/bigobj`、`/FU` MSVC `platform.winmd`，并包含 WinRTUtils 根/inc、cascadia/inc、公共 native include 和由固定 `Microsoft.Windows.CppWinRT.2.0.250303.1/bin/cppwinrt.exe -input 10.0.22621.0 -base` 生成的 Windows platform projection。新目标复用并重命名现有 ShellExtension 已由 Ninja 拥有的同一 platform projection 规则，不能读取 `WinRTUtils/Generated Files` 或旧 obj 目录。直接旧 ProjectReference 共 9 个，其中 TerminalAppLib、TerminalControlLib、Settings Model Lib 是静态中间层；递归展开后最终链接入口固定为 TerminalApp LocalTests/DLL/单测、TerminalControl DLL/Control 单测、Settings Model DLL/单测、TerminalConnection、Settings Editor、UIHelpers、UIMarkdown、WindowsTerminal 共 12 个。实施同批加入唯一 `/Z7 /GL-` archive、12 个显式链接入口，删除 9 个旧引用和旧工程/solution/slnf 项及旧 PDB。
- **2026-08-21 / CP-226 完成**：`cmake -S . -B .\build`、`cmake --build .\build`、`cmake --build .\build --target full` 均 exit 0；full 完成产品包生成、workspace extension 和 repack。新 `WinRTUtils.lib` 为 13,009,482 字节，`lib /list` 原始 6 行（扣除 archive 索引后为 4 个对象 member），`dumpbin /linkermember:1` 原始 826 行；旧基线分别为 15,289,526 字节、原始 6 行/4 个对象 member、1493 行。Ninja 生成的 platform projection 共 1,254 个文件，`build.ninja` 对源码树 `WinRTUtils/Generated Files` 引用为 0；旧 PDB 未重生。MSBuild 实际 link tlog 已确认 Terminal Control、Settings Editor、Settings Model 等最终入口直接读取 `microsoft/bin/x64/Release/WinRTUtils/WinRTUtils.lib`。仓库内旧工程路径/GUID 残留 0，显式链接入口 12，现存 vcxproj 61、ProjectReference 62，`git diff --check` 无 whitespace error。
- **2026-08-21 / CP-227 启动**：下一目标固定为 `microsoft/src/cascadia/TerminalSettingsAppAdapterLib/TerminalSettingsAppAdapterLib.vcxproj` 输出的同名静态库。源码只有 `TerminalSettings.cpp` 与创建 `pch.h` 的 `pch.cpp`，但编译依赖 `Microsoft.Terminal.TerminalConnection`、`Microsoft.Terminal.Settings.Model`、`Microsoft.Terminal.Control`、`Microsoft.Terminal.Core` 四个产品 WinMD 生成的 reference projection，并依赖 MUX/XAML 头。先保存旧 archive/PDB 和 cppwinrt response-file/编译命令证据，锁定最终链接消费者；新实现必须由 Ninja 在独立构建目录生成 reference projection，不读取源码树 `Generated Files` 或旧 MSBuild obj，不增加任何新构建入口、preset、包装或兼容分支。
- **2026-08-21 / CP-227 合同与闭包固定**：旧 archive/PDB 已保存到 `build/migration-baseline/CP-227`；archive 为 31,154,490 字节，`lib /list` 原始 4 行（2 个对象 member），`dumpbin /linkermember:1` 3,789 行，PDB 为 46,592,000 字节。旧源码树 reference projection 有 1,298 个文件，但新目标只允许读取 Ninja 构建目录中的 projection。已用固定 cppwinrt 验证最小生成合同：4 个产品 WinMD 加 `Microsoft.UI.Xaml.2.8.4`、`Microsoft.Web.WebView2.1.0.1661.34` 两个固定包 WinMD，以 Windows SDK `10.0.22621.0` 为 reference，可在独立目录生成所需 4 个产品 namespace、6 个 MUX namespace 和 WebView2 namespace 共 44 个 projection 文件；Windows namespace 继续来自 CP-226 的 Ninja platform projection。旧直接 ProjectReference 为 LocalTests、TerminalAppLib、Settings Editor 共 3 个；展开静态 TerminalAppLib 后，最终显式链接入口固定为 LocalTests、TerminalApp DLL、TerminalApp 单测、Settings Editor 共 4 个，WorkspaceExtension 已有 CMake 链接入口。全新 `full` 的固定顺序为：先构建不含 adapter 的原生基础库，再调用 Settings Model DLL 的现有单一工程图递归生成 TerminalCore、TerminalConnection、TerminalControl、Settings Model 四个上游 WinMD，再由 Ninja 生成 reference projection 和 adapter archive，最后进入 WindowsTerminal 产品构建；不重复单独调用前三个上游工程，不设置缺失文件兜底或双路径。
- **2026-08-21 / CP-227 首次 full 检查点**：配置、默认构建均 exit 0；首次 full 已完成四个上游 WinMD、Ninja adapter、Settings Editor 与 TerminalApp 的实际链接，link 命令确认直接读取新 `TerminalSettingsAppAdapterLib.lib`，但在产品 PRI 合并阶段 exit 1。根因不是编译或链接，而是 `ProductPriGenerator` 仍把这个无资源静态库的旧 MSBuild 744 字节空 PRI 列为必需输入。处理原则固定为从 PRI 输入清单删除该无资源项目，不创建占位 PRI、不恢复旧工程、不增加 fallback；修正后重新执行原始默认与 full 命令。
- **2026-08-21 / CP-227 第二次 full 检查点**：删除无资源 adapter PRI 输入后，默认构建 exit 0；第二次 full 再次完成产品编译链接，随后 PRI 校验从“缺少 adapter PRI”推进为“期望 13、实际 12”。根因是清单项已减少但独立魔法数字未同步。修正为以 `ProductPriFiles.Length + 1`（固定项目 PRI 清单加 MUX PRI）推导期望数量，保留严格数量检查但消除与清单重复维护；仍不生成占位 PRI。
- **2026-08-21 / CP-227 完成**：最终 `cmake -S . -B .\build`、`cmake --build .\build`、`cmake --build .\build --target full` 均 exit 0；full 通过 12 项 PRI 合并、产品包创建、workspace extension 和 repack。新 archive 为 49,747,786 字节，`lib /list` 与旧基线同为原始 4 行/2 个对象 member，public-symbol 表示由旧 3,789 行变为 1,569 行；无共享 PDB、无无资源 PRI。Ninja reference projection 精确 44 个文件，源码树 `Generated Files` 已删除且 `build.ninja` 引用为 0。Settings Editor 与 TerminalApp 实际 link tlog 均确认直接读取新 archive；旧路径/GUID 残留 0，vcxproj 显式入口 4。现存 vcxproj 60、ProjectReference 59；全部 XML 可解析，`git diff --check` 无 whitespace error。
- **2026-08-21 / CP-228 启动**：下一目标固定为 `microsoft/src/renderer/wddmcon/lib/wddmcon.vcxproj`。当前工程声明 3 个 ClCompile、0 IDL、0 ProjectReference；先保存旧 archive/PDB，读取实际编译合同并展开所有直接/最终消费者。实施仍要求唯一 Ninja 产物、最终链接入口显式化、旧工程/solution/slnf/ProjectReference 同批删除，不添加 preset、包装、兼容或备用构建路径。
- **2026-08-21 / CP-228 合同修正**：全仓 GUID/路径/产物扫描确认该工程没有任何 vcxproj ProjectReference，没有 `ConRenderWddmCon.lib` 或 PDB，`OpenConsole.slnx` 对常用 Release/Debug x64 构建禁用，仅 `OpenConsole.slnx` 与 `conhost.slnf` 仍登记它；`sources.inc`/旧 OS-build `sources` 文本不是当前 MSBuild/CMake 图。因而不添加无人消费的 CMake archive，保留 `main.cxx`、`precomp.cpp`、`WddmConRenderer.cpp` 等源码，只删除死 vcxproj/filters 和 solution/slnf 登记。若未来恢复 renderer，必须作为新的明确需求建立真实消费者和 CMake 目标，不能保留空壳兼容项。
- **2026-08-21 / CP-228 完成**：`cmake -S . -B .\build`、`cmake --build .\build`、`cmake --build .\build --target full` 均 exit 0；full 完成产品包、workspace extension 与 repack。旧工程路径/GUID 残留 0，`main.cxx` 等源码仍在，未制造 `ConRenderWddmCon.lib`。现存 vcxproj 59、ProjectReference 59；solution XML、slnf JSON 均可解析，`git diff --check` 无 whitespace error。
- **2026-08-21 / CP-229 启动**：下一目标固定为 `microsoft/src/interactivity/onecore/lib/onecore.LIB.vcxproj`。工程初筛为 8 个 ClCompile、0 IDL、0 ProjectReference；先保存旧 archive/PDB、读取编译合同并展开所有直接和最终消费者，再决定真实输出名与显式链接入口。禁止保留旧工程、双产物或兼容路径。
- **2026-08-21 / CP-229 合同修正**：GUID、路径、link tlog 与全 bin 扫描确认没有当前 vcxproj 消费者，也不存在 `ConInteractivityOneCoreLib.lib`/PDB；`OpenConsole.slnx` 对常用配置禁用构建，仅 solution/slnf 仍登记。`host/sources.inc` 和各旧 `sources` 文件属于非当前 OS-build 文本，不是 CMake/MSBuild 产品闭包。因此保留 onecore 下 8 个实现源码、头和旧 sources 描述，仅删除孤立 vcxproj 及 solution/slnf 登记，不添加无消费者 archive。
- **2026-08-21 / CP-229 完成**：`cmake -S . -B .\build`、`cmake --build .\build`、`cmake --build .\build --target full` 均 exit 0；full 完成产品包、workspace extension 与 repack。删除补查发现的旧 `onecore.LIB.vcxproj.filters` 后，旧工程路径/GUID 残留 0，8 个实现源码及旧 OS-build 描述仍保留，未制造无人消费的 archive/PDB。现存 vcxproj 58、ProjectReference 59；下一项直接调查 `host/ut_lib/host.unittest.vcxproj`。
- **2026-08-21 / CP-230 启动**：目标固定为 `microsoft/src/host/ut_lib/host.unittest.vcxproj`。该工程表面没有直接 `ClCompile`，不能据此认定为空工程；先展开其 `.vcxitems`/`.props` 导入、产物和所有引用，确认它是共享源码静态库还是孤立调度项。结论确定后同批处理工程、solution/slnf、ProjectReference 和产物，不增加 preset、wrapper、fallback 或第二套编译命令。
- **2026-08-21 / CP-230 合同固定**：`host.unittest.vcxproj` 导入 `host-common.vcxitems`，实际使用与已原生化 `ConhostV2Lib` 完全相同的 44 个实现源码，差异合同只有测试 props 提供的 `INLINE_TEST_METHOD_MARKUP`、`UNIT_TESTING`；输出固定为 `ConhostV2Lib.unittest.lib`，唯一实际 ProjectReference/最终链接消费者是 `Host.UnitTests.vcxproj`。新 Ninja target 复用同一个显式 source list，但独立编译测试宏、CMake PCH 和唯一 `/Z7 /GL-` archive；`full` 的原生 foundation 明确构建它，Host 单测显式链接该路径。同批删除旧 ProjectReference、两个 solution BuildDependency、solution/slnf project、vcxproj 及陈旧 metaproj，不保留旧调度入口。
- **2026-08-21 / CP-230 首次 full 检查点**：configure 与默认构建 exit 0；首次 full 在新 test archive 的 PCH 编译阶段 exit 1，首个根因是 `UNIT_TESTING` 令 `LibraryIncludes.h` 包含 `WexTestClass.h`，而初版目标未展开旧 `Microsoft.Taef.targets` 提供的固定 include。直接加入仓库固定 `Microsoft.Taef.10.100.251104001/build/Include` 到唯一目标合同；不恢复旧工程、不增加搜索或 fallback。重新执行原三条命令验收。
- **2026-08-21 / CP-230 完成**：补齐固定 TAEF include 后，`cmake -S . -B .\build`、`cmake --build .\build`、`cmake --build .\build --target full` 均 exit 0；新 archive 为 18,014,512 字节，`lib /list` 精确 44 个对象 member，public-symbol 表示 2,160 行，无共享 PDB。Host 单测唯一显式链接，旧工程路径/GUID 残留 0；现存 vcxproj 57、ProjectReference 58，solution/slnf 可解析，`git diff --check` 无 whitespace error。
- **2026-08-21 / CP-231 启动**：下一目标固定为 `microsoft/src/cascadia/TerminalCore/lib/terminalcore-lib.vcxproj`。工程表面没有直接实现源，但包含 IDL 并可能导入 shared-items；先展开实际 evaluated 源、MIDL/CppWinRT 输出、archive/PDB、所有直接 ProjectReference 及递归最终链接入口。合同固定后建立唯一 Ninja 生成和 archive，随后同批删除旧工程与全部调度引用；仍只用既定三条命令验收。
- **2026-08-21 / CP-231 合同固定**：shared-items 展开为 `TerminalRenderData.cpp`、`TerminalSelection.cpp`、`TerminalApi.cpp`、`Terminal.cpp` 加 PCH，旧 archive 为 13,991,756 字节、5 个对象 member、2,733 行 public-symbol 表示，旧共享 PDB 7,770,112 字节。IDL spike 证明单一 `ICoreSettings.idl` 只需固定 Windows Foundation contract 即可由 MIDL 直接生成 4,608 字节最终命名 WinMD；固定 cppwinrt 再以 SDK 10.0.22621.0 为 reference 生成 4 个 Core projection 文件，四个头与旧通用 MIDL→MdMerge→CppWinRT 链逐字节一致，因此删除无必要的 MdMerge 中间层而非保留兼容链。3 个直接旧 ProjectReference 改为同一 Ninja WinMD 的显式 Reference；递归最终 archive 链接入口固定为 TerminalControl DLL、Control 单测、TerminalCore 单测 3 个。`full` foundation 在任何产品 MSBuild 前生成 WinMD、projection 和唯一 `/Z7 /GL-` archive；旧工程、3 个引用和两个 solution BuildDependency 同批删除。
- **2026-08-21 / CP-231 首次默认构建检查点**：configure exit 0；默认构建在执行编译前由 Ninja 拒绝，首因是初版自定义命令误用了未定义的 `_portable_windows_sdk_root`，Foundation WinMD 被解析到根路径 `/References/...`。直接改为本文件已有且固定为 Windows Kits 10 的 `_portable_sdk_root`，不添加查找、别名或 fallback；重新执行原三条命令。
- **2026-08-21 / CP-231 第二次默认构建检查点**：修正 SDK root 后 configure exit 0，MIDL 已启动但把 CMake 的正斜杠绝对 IDL 路径错误拼成 `D:\D:\...`，默认构建 exit 1。该工具对输入文件的 Windows 路径语法有硬要求；仅用 `file(TO_NATIVE_PATH)` 生成同一 IDL 的原生路径参数，输出和依赖仍是唯一 CMake 路径，不引入第二输入。
- **2026-08-21 / CP-231 第三次默认构建检查点**：IDL 输入改为原生路径后 MIDL 已完成预处理并读取 Foundation contract，但对仍为正斜杠的 `/metadata_dir` 报 MIDL4034/Windows.Winmd 路径语法错误。MIDL 的所有文件系统参数统一转换为 Windows 原生路径；这是单一命令的工具语法修正，不增加兼容分支。
- **2026-08-21 / CP-231 首次 full 检查点**：原生 TerminalCore 的 MIDL、projection、5-member archive 和下游 TerminalControl MIDL 均成功；TerminalControl 最终链接因 `Microsoft.Terminal.Control.iobj/ipdb` 中保留迁移前 `/GL` 跨库状态，报 51 个 `ControlLib` 与已切成 `/GL-` 的 `ConTypes.lib` 重复定义并 exit 1。真实 link tlog 确认新 `TerminalCore.lib` 已被读取，重复符号不来自它。精确删除该目标 72,184,278 字节 iobj 和 33,220,560 字节 ipdb 后重链；不增加 linker 开关、忽略重复符号或兼容路径。
- **2026-08-21 / CP-231 第二次 full 检查点**：清除 LTCG 缓存后同样 51 个重复定义再次出现，证明缓存判断错误；继续沿首个符号定位到 `HwndTerminal.cpp` 直接文本包含 `../../types/viewport.cpp`，而 DLL 同时正式链接 `ConTypes.lib`，重复双方及 51 个 Viewport 符号完全吻合。删除该 `.cpp` include，让 Viewport 只由唯一 `ConTypes.lib` 提供；不使用 `/FORCE:MULTIPLE`、不删正式库、不恢复旧 TerminalCore 工程。
- **2026-08-21 / CP-231 第三次 full 检查点**：删除 `.cpp` include 后重编证明该文件此前还隐式借用了 `types/precomp.h` 带入的声明，报 Viewport 与三个 UI Automation API 未声明。改为显式包含 `types/inc/Viewport.hpp` 和 SDK `UIAutomationCoreApi.h`，不再通过实现文件偷带声明；这同时保留唯一实现与完整声明依赖。
- **2026-08-21 / CP-231 第四次 full 检查点**：显式头加入后 UI Automation 三项错误已清零，剩余 20 个均由旧 `Viewport.cpp` 内部的 `using namespace Microsoft::Console::Types` 曾被文本泄漏到调用文件导致。加入窄化的 `using Microsoft::Console::Types::Viewport`，不恢复整个命名空间污染；继续原命令回归。
- **2026-08-21 / CP-231 完成**：最终 `cmake -S . -B .\build`、`cmake --build .\build`、`cmake --build .\build --target full` 均 exit 0；TerminalControl、Settings、TerminalApp、产品包、workspace extension 和 repack 全部通过。新 archive 为 10,657,380 字节，与旧基线同为 5 个对象 member，public-symbol 表示由 2,733 行降为 958 行；WinMD 为 4,608 字节，Ninja projection 精确 4 文件。3 个最终入口显式链接，13 个现有 WinMD HintPath 统一消费新输出；旧源码树 projection、Release/Debug PDB 与陈旧 Debug 输出删除，旧路径/GUID 残留 0。现存 vcxproj 56、ProjectReference 55，XML/JSON 可解析，`git diff --check` 无 whitespace error。
- **2026-08-21 / CP-232 启动**：下一目标固定为 `microsoft/src/cascadia/TerminalControl/TerminalControlLib.vcxproj`。该工程约 16 个实现源、15 个 IDL，并承担 Control 静态实现、WinMD/projection 与 XAML 相关输入；先保存 archive/PDB/WinMD 和生成文件基线，展开 ProjectReference、显式 WinMD、最终 DLL/单测消费者及资源边界。实现必须让 Ninja 直接拥有生成与 archive，不通过旧 MSBuild 工程代编，也不增加 preset、wrapper、fallback。
- **2026-08-21 / CP-232 基线与 XAML 边界**：当前 archive 为 191,769,538 字节，`lib /list` 19 个 member、public-symbol 表示 27,900 行，共享 PDB 93,048,832 字节；15 个声明 IDL 加 XAML 元数据 IDL 形成 16 个 unmerged WinMD，最终 WinMD 54,272 字节，PRI 67,888 字节。源码树 `Generated Files` 当前有 1,441 个文件，包含 platform/reference projection、cppwinrt `.g.cpp/.g.h`、`module.g.cpp`、两套 XAML `.g.h/.g.hpp/.xbf` 和 XamlTypeInfo；这些都不能作为新 Ninja 输入。真实工程引用只有 TerminalControl DLL 与 Control 单测两个，TerminalConnection/UIHelpers 是其上游 metadata 输入；最终 archive 链接入口同为 DLL 与 Control 单测。
- **2026-08-21 / CP-232 XAML 编译器检查点**：固定 SDK 10.0.22621.0 的实现位于 `Windows Kits/10/bin/10.0.22621.0/XamlCompiler/Microsoft.Windows.UI.Xaml.Build.Tasks.dll`，核心任务类型为 `Microsoft.Windows.UI.Xaml.Build.Tasks.CompileXaml`，Pass1/Pass2 的完整输入属性已从固定 SDK targets 锁定；`XamlSaveStateFile.xml` 进一步确认本目标只含 `SearchBoxControl.xaml`、`TermControl.xaml`，引用集合为新 TerminalCore WinMD、TerminalConnection、UIHelpers、MUX、WebView2 和本地 Control WinMD。该 DLL 的公开任务继承 `Microsoft.Build.Utilities.Task`，不能把直接加载该 task DLL误记为“已去 MSBuild”；下一检查点必须确认能否调用其非 MSBuild 编译核心，或建立不依赖 VS MSBuild 目录的最小任务宿主。未完成前不删除 TerminalControlLib 工程，也不消费 1,441 个旧生成文件。
- **2026-08-21 / CP-232 task API 检查点**：反射已锁定 `CompileXaml` 的全部公开输入/输出以及 Pass1/Pass2 targets 映射；SDK task assembly 本身静态引用 `Microsoft.Build.Utilities.v4.0`、`Microsoft.Build.Framework`、`Microsoft.Build` 4.0，且真正的 compiler 类型均为 internal，没有公开的非 MSBuild Execute 入口。仅自制一个实例化 `CompileXaml` 的 launcher 仍会保留三项 MSBuild assembly 依赖，因此不实施这种表面去掉 `MSBuild.exe` 的兼容包装。下一步沿 `CompileXaml.Execute -> PopulateWrapper` 的内部 wrapper 构造和 SDK `genxbf.dll` 调用边界继续拆解，目标是 CMake 直接调用不依赖 Microsoft.Build 程序集的最小固定宿主。
- **2026-08-21 / CP-232 内部核心边界检查点**：IL 已证明公开任务的 `Execute()` 本身只负责构造 `CompileXamlInternal`、调用 `PopulateWrapper`、加载 `SavedStateManager`，最终进入 `CompileXamlInternal.DoExecute()`；`PopulateWrapper` 把 MSBuild `ITaskItem` 转换为内部 `IFileItem`/`IAssemblyItem`，把 `TaskLoggingHelper` 包装为内部 `ILog`，并构造可接受空 VS host 的 `BuildTaskFileService`。这把调查边界从公开 MSBuild task 精确收缩到同程序集的内部核心。下一步逐项检查 `CompileXamlInternal`、文件/assembly adapter、`ILog` 和 `BuildTaskFileService` 的类型签名与 `DoExecute` 调用图，并用隔离进程验证核心路径是否会解析 Microsoft.Build 程序集；只有确证不依赖后才建立固定宿主，不能用反射包装公开 task 冒充原生链。
- **2026-08-21 / CP-233 启动（与 CP-232 并行收敛）**：XAML 核心调查不再阻塞可直接删除的工程。首批固定为 `buffersize`、`CloseTest`、`ConEchoKey`、`FontList`、`Nihilist`、`Scratch` 六个单源码控制台工具：均无 ProjectReference、MIDL、XAML、RC、自定义生成和 PCH，输出均位于 `microsoft/bin/x64/Release`。六项只保留一个根 CMake/Ninja 入口并直接生成原名 EXE；同批删除旧 vcxproj/filters、solution 工程项及指向 `Nihilist`/`CloseTest` 的 solution-only BuildDependency，不保留 MSBuild 工具工程。检查点为六个目标编译链接、原三条命令、旧路径/GUID 残留 0 和 solution XML 可解析。
- **2026-08-21 / CP-232 evaluated 输入检查点**：在删除旧工程前用只读 evaluated-project 查询固定了真实 Release/x64 XAML 参数：语言 `CppWinRT`、扩展名 `.cpp`、根命名空间 `Microsoft.Terminal.Control`、项目名 `TerminalControlLib`、输出类型 `staticlibrary`、目标最低版本 10.0.18362.0、SDK 10.0.22621.0、`XamlComponentResourceLocation=nested`、禁用 XBF 行信息、启用默认 validation context；两个 Page 的 identity/full path 及 14 个 ClInclude/DependentUpon metadata 也已完整枚举。Pass2 本地 WinMD 是既定 ControlLib 输出；reference 集合必须在上游 ResolveReferences 后固定，不能依赖未执行 target 时为空的 evaluated item。
- **2026-08-21 / CP-233 完成**：根 CMake 已用一个明确的控制台工具函数直接构建六个单源码目标，全部生成到原 `microsoft/bin/x64/Release` 路径：`buffersize.exe` 40,960、`CloseTest.exe` 58,880、`ConEchoKey.exe` 43,008、`FontList.exe` 38,400、`Nihilist.exe` 9,728、`Scratch.exe` 9,728 字节。六个 vcxproj、五个 filters、六个 solution project、两条测试 BuildDependency 和 `conhost.slnf` 的 Nihilist 项已删除；源码不变且没有备用工程。`cmake -S . -B .\build`、`cmake --build .\build` 均 exit 0；沙箱内 full 仅复现既有 FileTracker E_ACCESSDENIED，允许 FileTracker 后同一 `cmake --build .\build --target full` exit 0，产品包、workspace extension、repack 全部完成。旧路径/GUID 残留 0，solution/slnf 可解析，当前非 tmp vcxproj 42、ProjectReference 51，`git diff --check` 无 whitespace error。
- **2026-08-21 / CP-234 启动**：继续批量迁移三个仍无 ProjectReference/MIDL/XAML/RC/自定义生成的工具：单源码 `RenderingTests`、双源码 `U8U16Test`、以及 `main.cpp + pch.cpp + manifest` 的 `ConsoleMonitor`。扩展现有唯一 CMake 工具函数为明确 source-list 输入，前两项直接复用；ConsoleMonitor 额外声明 CMake PCH 和现有 manifest。三项验证通过后删除 vcxproj/filters 与 solution 项，不把特殊无 CRT 的 benchcat 或依赖 conptylib 的 VtPipeTerm 混入此合同。
- **2026-08-21 / CP-234 首次链接检查点**：RenderingTests 与 U8U16Test 已直接成功，ConsoleMonitor 的源码/PCH 均成功但首次被通用 console subsystem 链接为需要 `main`，而实际唯一入口是 `wWinMain`。该工程旧 XML 没显式写 subsystem，真实入口已由源码确定为 Windows GUI；仅对该目标明确 `/SUBSYSTEM:WINDOWS`，保留同一函数和单一路径，不添加另一入口或 shim。
- **2026-08-21 / CP-234 完成**：修正唯一 subsystem 后三项目标全部由 Ninja 构建：`RenderingTests.exe` 17,920、`U8U16Test.exe` 68,608、`ConsoleMonitor.exe` 47,104 字节；后者直接使用 CMake PCH 并嵌入原 manifest。三个 vcxproj、三个 filters 和 solution project 已删除，源码与 manifest 保留。`cmake -S . -B .\build`、`cmake --build .\build`、允许 FileTracker 后的 `cmake --build .\build --target full` 均 exit 0，产品包、workspace extension 和 repack 完成；旧路径/GUID 残留 0，solution XML 可解析。当前非 tmp vcxproj 39、ProjectReference 51，`git diff --check` 无 whitespace error。
- **2026-08-21 / CP-235 启动**：单独迁移 `tools/benchcat/benchcat.vcxproj`。它只有 `main.cpp`，但 Release 合同刻意定义 `NODEFAULTLIB`、关闭异常/GS/SDL/CFG/WPO、指定 `/ENTRY:main` 并忽略全部默认库，源码内 `crt.cpp` 只实现实际需要的 memcpy/memset；输出名是 `bc.exe`。新目标必须逐项保留这个无 CRT 合同并直接显式链接所需 Windows import library，不能复用普通工具函数或悄悄恢复 CRT。成功后删除唯一 vcxproj/solution 项。
- **2026-08-21 / CP-235 首次编译检查点**：初版无 CRT 选项已生效，但遗漏旧公共 props 的 C++ 标准，两个 `[[fallthrough]]` 在默认语言模式触发 C5051 并被 `/WX` 提升为错误。给唯一目标明确 C++20 后通过；不降 `/WX`、不抑制警告、不改源码。
- **2026-08-21 / CP-235 完成**：Ninja 直接生成 8,704 字节 `bc.exe`，保留 `/NODEFAULTLIB`、`/ENTRY:main`、无 GS/SDL/CFG/异常和源码内最小 memcpy/memset 合同。PE imports 精确为 KERNEL32、ADVAPI32、SHLWAPI、SHELL32，UCRT/VCRUNTIME/MSVCP/MSVCR 导入为 0，证明没有暗中恢复 CRT。旧 vcxproj 和 solution 项删除，路径/GUID 残留 0。原三条命令均 exit 0，full 完成产品包、workspace extension 和 repack；当前非 tmp vcxproj 38，solution XML 可解析，`git diff --check` 无 whitespace error。
- **2026-08-21 / CP-236 启动**：迁移单源码 `VtPipeTerm.vcxproj`。它的两个非系统链接输入 `ConTypes.lib` 与 `conptylib.lib` 均已由现有 Ninja target 唯一拥有，当前产物分别为 10,666,980 与 6,460,562 字节；新工具目标直接依赖并链接这两个 CMake target，而不是再次写死一组平行库路径。补充真实的 `src/inc` 头路径后生成原 `VtPipeTerm.exe`，成功即删除旧 vcxproj/solution 项及 Host.EXE 的 solution-only BuildDependency，不保留 MSBuild 工具入口。
- **2026-08-21 / CP-236 首次链接检查点**：源码已编译，首次链接准确暴露旧公共 props 的三项隐式合同：工具本身使用静态 CRT，而两个 Ninja archive 也是 `/MT`；`ConTypes` 的 utils 对固定 vcpkg `fmt.lib` 有真实引用；管道实现调用三个 Nt API，需要 `ntdll.lib`。正式目标改为同一静态 CRT并精确链接单个固定 fmt archive 与 ntdll，不使用 vcpkg `*.lib` 通配、不忽略 RuntimeLibrary mismatch、不增加备用链接路径。
- **2026-08-21 / CP-236 完成**：补齐三项真实链接合同后 Ninja 直接生成 236,544 字节 `VtPipeTerm.exe`，并通过 CMake target dependency 消费唯一 `ConTypes` 与 `conptylib`。旧 vcxproj、filters、metaproj、solution project 及其 Host.EXE BuildDependency 已删除，旧路径/GUID 残留 0。`cmake -S . -B .\build`、`cmake --build .\build`、允许 FileTracker 后的 `cmake --build .\build --target full` 均 exit 0，产品包、workspace extension、repack 完成；当前非 tmp vcxproj 37，solution XML 可解析，`git diff --check` 无 whitespace error。
- **2026-08-21 / CP-237 启动**：下一批固定为 `winconpty/dll/winconptydll.vcxproj` 与 `winconpty/ft_pty/winconpty.FeatureTests.vcxproj`。前者只有 PCH 翻译单元、`.def` 和现有 Ninja `conptylib`，目标是直接生成原 `conpty.dll`；后者是两个源码的 TAEF 测试 DLL，同样直接链接唯一 `conptylib`。先保存/核对现有 DLL、PDB、exports/imports、测试 TAEF include/lib/define 与全部 solution/slnf 调度；两个新目标分别证明后同批删除旧 vcxproj/filters/metaproj 和调度项，不保留 MSBuild 包装工程。
- **2026-08-21 / CP-237 构建检查点**：Ninja 已直接生成 `conpty.dll` 与 479,232 字节 `winconpty.Feature.Tests.dll`。`conpty.dll` 的 14 个 `.def` 导出全部存在且别名地址正确，依赖仅 `KERNEL32.dll`、`ntdll.dll`、`ADVAPI32.dll`；FeatureTests 直接使用 TAEF 的 `Wex.Logger.lib`、`Wex.Common.lib`、`TE.Common.lib` 和唯一 Ninja `conptylib`，单目标构建 exit 0。下一检查点是删除两个旧工程和 `OpenConsole.slnx`、`conhost.slnf` 调度后执行三条固定构建命令。
- **2026-08-21 / CP-237 完成**：已删除 `winconptydll.vcxproj`、`winconpty.FeatureTests.vcxproj`、对应 `OpenConsole.slnx`/`conhost.slnf` 项以及 WPF metaproj 中硬编码的旧 DLL 工程绝对路径；旧路径/GUID 残留 0。三条固定命令 `cmake -S . -B .\build`、`cmake --build .\build`、`cmake --build .\build --target full` 均 exit 0，solution XML 与 slnf JSON 可解析；当前非 tmp vcxproj 35。
- **2026-08-21 / CP-238 启动**：下一批固定为两个无项目依赖的独立控制台程序：`samples/ConPTY/EchoCon/EchoCon/EchoCon.vcxproj` 与 `src/tools/ConsoleBench/ConsoleBench.vcxproj`。前者仅 `EchoCon.cpp`/`stdafx.cpp`，后者仅 5 个源码、PCH 和现有 manifest。直接建立两个 Ninja executable，逐项核对 PCH、Unicode/console 定义、manifest、CFG 与输出名；构建和 PE 校验通过后删除两个 vcxproj/filters 及全部引用，不增加脚本、preset 或兼容路径。
- **2026-08-21 / CP-238 构建检查点**：Ninja 已直接生成 11,776 字节 `EchoCon.exe` 与 192,000 字节 `ConsoleBench.exe`，两者均为 x64 console PE；EchoCon 保留原 PCH 和源码允许的未使用线程句柄诊断，ConsoleBench 保留 PCH、manifest、`ntdll`/version 链接以及禁用 CFG 合同。下一检查点是删除两个 vcxproj/filters、EchoCon 独立 sln 与主 solution 项后执行三条固定构建命令。
- **2026-08-21 / CP-238 完成**：两个 vcxproj、两个 filters、EchoCon 独立 sln 与主 solution 项均已删除，旧路径/GUID 残留 0。三条固定构建命令均 exit 0，产品包与 full repack 完成，solution XML 可解析，当前非 tmp vcxproj 33。
- **2026-08-21 / CP-239 启动**：下一批固定为 parser 下两个独立控制台 fuzz 程序：`ft_fuzzer/VTCommandFuzzer.vcxproj` 与 `ft_fuzzwrapper/FuzzWrapper.vcxproj`。前者是两翻译单元的自包含 directed fuzzer，后者是三翻译单元并直接消费现有 Ninja `ConTypes`、`ConTermParser`。逐项落实 PCH、console/test define、include 和静态运行库，分别生成 `VTCommandFuzzer.exe`、`ConTerm.Parser.FuzzWrapper.exe`；证明后删除旧工程/filters 与 solution/slnf 项，不迁移当前依赖链巨大的 Host fuzzer。
- **2026-08-21 / CP-239 构建检查点**：Ninja 已直接生成 261,120 字节 `VTCommandFuzzer.exe` 与 268,800 字节 `ConTerm.Parser.FuzzWrapper.exe`。两者 PCH、TAEF test define/lib 与静态 CRT 已落地，wrapper 直接建立对唯一 `ConTypes`、`ConTermParser` target 的依赖；双目标构建 exit 0。下一检查点是删除两个工程/filters 和 solution/slnf 调度后运行三条固定构建命令。
- **2026-08-21 / CP-239 完成**：两个 vcxproj、两个 filters、主 solution 与 conhost slnf 调度均已删除，旧路径/GUID 残留 0。三条固定构建命令均 exit 0，产品包、workspace extension 与 full repack 完成；solution XML、slnf JSON 可解析，`git diff --check` 无 whitespace error，当前非 tmp vcxproj 31。
- **2026-08-21 / CP-240 启动**：下一批固定为同一 parser/types 基础链上的 `Parser.UnitTests.vcxproj`（6 个源码）与 `Types.Unit.Tests.vcxproj`（5 个源码）。两者无 ProjectReference，直接复用已证明的 TAEF include/lib 合同，并链接现有唯一 Ninja `ConTermParser`、`ConTypes` 及工程声明的其余静态库；先保存目标名、PCH、源码、链接和调度清单，再构建、检查测试 DLL 导出并删除旧工程/filters/solution/slnf 项。25 源码的 til tests 留到下一批，不与本批混合。
- **2026-08-21 / CP-240 构建检查点**：Ninja 已直接生成 784,896 字节 `ConParser.Unit.Tests.dll` 与 897,024 字节 `Types.Unit.Tests.dll`。Parser 直接消费 `TerminalInput`、`ConTermAdapt`、`ConTypes`、`ConInteractivityBaseLib`、`ConTermParser`，Types 直接消费 `ConTypes`；补齐的 `onecore`、Crypt32、BCrypt、系统 ICU 是静态库实际未解析符号的真实链接合同。TAEF `/list` 对两个 DLL 均 exit 0，完整发现 Parser 参数化测试与 Types 的 17 个测试。下一检查点是删除旧工程/filters、solution/slnf 项并运行三条固定构建命令。
- **2026-08-21 / CP-240 调度复核**：删除 CP-240 slnf 项时发现并清除了 CP-239 遗留的 `ft_fuzzwrapper/FuzzWrapper.vcxproj` slnf 行，同时修正本次列表编辑产生的重复 til 行；当前 `conhost.slnf` 每个剩余工程仅出现一次。后续残留检查必须同时覆盖 `/` 与 `\\` 两种路径分隔符。
- **2026-08-21 / CP-240 完成**：两个旧 vcxproj、Parser filters、主 solution 与 conhost slnf 调度均已删除。三条固定命令全部 exit 0，TAEF 枚举、产品包、workspace extension、full repack 均通过；solution XML 与去重后的 slnf JSON 有效，当前非 tmp vcxproj 29，`git diff --check` 无 whitespace error。
- **2026-08-21 / CP-241 启动**：下一批固定为 `TextBuffer.Unit.Tests.vcxproj`（5 个编译单元）与 `Adapter.UnitTests.vcxproj`（5 个编译单元）。TextBuffer 直接消费 `ConTypes`、`ConRenderBase`、`ConBufferOut`；Adapter 直接消费其工程声明的现有 Ninja console library 闭包。复用已实际通过 TAEF 枚举的测试 DLL 合同，先构建并枚举，再删除旧工程/filters 和 solution/slnf 调度；不把 Interactivity 测试混入本批。
- **2026-08-21 / CP-241 构建检查点**：Ninja 已直接生成 571,392 字节 `TextBuffer.Unit.Tests.dll` 与 1,096,704 字节 `ConAdapter.Unit.Tests.dll`。TAEF `/list` 均 exit 0，分别输出 24 行与 289 行测试清单；Adapter 的完整 console 静态库闭包由真实 CMake target 依赖表达。下一检查点是删除两个 vcxproj、Adapter filters、TextBuffer 硬编码旧绝对路径的 metaproj 及 solution/slnf 项，再运行三条固定构建命令。
- **2026-08-21 / CP-241 完成**：两个 vcxproj、Adapter filters、TextBuffer metaproj 及全部 solution/slnf 调度已删除，旧路径/GUID 残留 0。三条固定命令全部 exit 0，产品包与 full repack 通过，当前非 tmp vcxproj 27。
- **2026-08-21 / CP-242 启动**：立即迁移只有 `UiaTextRangeTests.cpp` 与共享 PCH 两个编译单元的 `Interactivity.Win32.UnitTests.vcxproj`。直接复用 CP-241 已证明的完整 console library target 闭包并加入该工程额外声明的 `ConInt`、winmm、imm32；生成 `Conhost.Interactivity.Win32.Unit.Tests.dll`、TAEF 枚举后删除旧工程与全部调度。
- **2026-08-21 / CP-242 构建检查点**：Ninja 已直接生成 1,617,408 字节 `Conhost.Interactivity.Win32.Unit.Tests.dll`，TAEF `/list` exit 0 并输出 60 行 UIA TextRange 测试清单。链接器验证补齐 `MidiAudio` target、Propsys、D2D/DWrite/DXGI/D3D11/D3DCompiler/WindowsCodecs 的真实闭包；下一检查点是删除旧 vcxproj/filters 与 solution/slnf 调度后运行三条固定命令。
- **2026-08-21 / CP-242 完成**：旧 vcxproj/filters 与 solution/slnf 调度已删除，旧路径/GUID 残留 0；三条固定命令均 exit 0，当前非 tmp vcxproj 26。
- **2026-08-21 / CP-243 启动**：继续迁移无 ProjectReference、无额外静态工程依赖的 `til.unit.tests.vcxproj`。24 个既有编译单元逐项列入一个 `til.Unit.Tests.dll` Ninja target，保留共享 PCH、TAEF、静态 CRT 与 `src/inc/test` include 合同；构建和 TAEF 枚举通过后删除旧 vcxproj/filters 及 solution/slnf 项。
- **2026-08-21 / CP-243 构建检查点**：24 个源码全部由 Ninja 编译并生成 860,672 字节 `til.Unit.Tests.dll`；TAEF `/list` exit 0，输出 306 行测试清单。下一检查点是删除旧 vcxproj/filters、solution/slnf 项并执行三条固定构建命令。
- **2026-08-21 / CP-243 完成**：旧 `til.unit.tests.vcxproj`、filters 及 solution/slnf 调度已删除，旧路径/GUID 残留 0；固定命令 `cmake -S . -B .\build`、`cmake --build .\build`、`cmake --build .\build --target full` 均 exit 0，产品包、workspace extension 与 full repack 完成。当前非 tmp vcxproj 25，`git diff --check` 无 whitespace error。
- **2026-08-21 / CP-244 启动**：为提高收敛速度，同批调查并迁移 `src/host/ut_host/Host.UnitTests.vcxproj` 与 `src/host/ft_host/Host.FeatureTests.vcxproj`。两者均直接进入既有 Ninja console library 图：UnitTests 使用已经原生化的 `ConhostV2Lib_unittest` 及完整 host 闭包，FeatureTests 使用 `ConTypes` 与 `conptylib`。先锁定全部源码、PCH、定义、链接、输出名及 solution/slnf 调度，再生成两个 TAEF DLL并执行 `/list`；证明后立即删除两个旧工程及调度，不增加 MSBuild 兼容入口。
- **2026-08-21 / CP-244 构建检查点**：Ninja 已直接编译 UnitTests 的 19 个编译单元和 FeatureTests 的 21 个 C++ 编译单元加 RC，生成 2,642,432 字节 `Conhost.Unit.Tests.dll` 与 2,683,392 字节 `ConHost.Feature.Tests.dll`。TAEF `/list` 均 exit 0，分别输出 5122 行和 354 行测试清单；UnitTests 直接链接 `ConhostV2Lib_unittest` 及既有原生 host 闭包，FeatureTests 直接链接唯一 `ConTypes`、`conptylib` target。下一检查点是删除两个旧 vcxproj/filters 和 solution/slnf 调度，再执行三条固定构建命令。
- **2026-08-21 / CP-244 完成**：两个旧 vcxproj、filters、metaproj 与 OpenConsole/conhost 调度全部删除，旧路径/GUID 残留 0；solution XML、slnf JSON 有效。三条固定命令均 exit 0，`full` 完成直接产品包、workspace extension 和 repack；当前非 tmp vcxproj 23。
- **2026-08-21 / CP-245 启动**：继续调查剩余非 XAML 的 `src/propsheet/propsheet.vcxproj` 与 `src/host/ft_fuzzer/Host.FuzzWrapper.vcxproj`。目标是确认两者的输出类型、全部源/PCH/RC/DEF、现有原生 target 依赖和 Host.EXE 调度关系；若闭包均已由 Ninja 拥有则同批迁移并删除旧工程，否则只实施已闭合目标并把真实阻塞写入检查点，不添加兼容构建路径。
- **2026-08-21 / CP-245 构建检查点**：Ninja 已生成 1,294,336 字节 `OpenConsoleFuzzer.exe` 与 335,872 字节 `console.dll`。Host fuzzer 的两个编译单元直接链接唯一 `ConhostV2Lib` 和完整原生 host 闭包；propsheet 的 17 个 C++ 编译单元、`console.rc`、`console.def` 与 `strid.mc` 已由固定 Windows SDK `mc.exe` 生成规则直接拥有，三项 DEF 导出均存在。首次 propsheet 链接暴露 `strid.rc` 被重复编译，现已收敛为只由 `console.rc` 包含；随后补齐 `ConProps` 实际需要的 `propsys.lib`，双目标构建 exit 0。下一检查点是删除两个旧 vcxproj/filters/metaproj 和 solution/slnf 调度，并运行三条固定构建命令。
- **2026-08-21 / CP-245 完成**：两个旧 vcxproj、propsheet filters、solution-only BuildDependency 及 OpenConsole/conhost 调度全部删除，旧路径/GUID 残留 0；conhost slnf 现有 2 项且无重复。三条固定命令均 exit 0，`full` 完成产品包、workspace extension 和 repack；当前非 tmp vcxproj 21。
- **2026-08-21 / CP-246 启动**：下一目标固定为剩余 conhost 产品入口 `src/host/exe/Host.EXE.vcxproj`。现有 Host 静态实现、renderer/interactivity/server/propsheet 库已由 Ninja 拥有，先锁定 EXE 自身源、RC/manifest、入口、DEF、链接闭包以及 package map 对 `OpenConsole.exe` 的消费；新目标必须直接生成同路径产品 EXE并进入 `full` 的 package 依赖，证明后删除旧工程及 conhost 调度，不保留 MSBuild 入口。
- **2026-08-21 / CP-246 构建检查点**：Ninja 已直接编译 Host PCH、`CConsoleHandoff.cpp`、`exemain.cpp`、RC 和既有 manifest，生成 1,352,192 字节 `OpenConsole.exe`。首次链接确认源码唯一入口为 `wWinMain`，目标已明确使用 Windows subsystem；保留 `icu.dll` delay-load，并直接链接现有完整 host target 闭包。`full` 中独立调用 `Host.EXE.vcxproj` 的 MSBuild 命令已删除，native-product-foundation 直接构建新目标；下一检查点用原 `full` 验证 package map 实际消费该 EXE，成功后删除旧工程与 solution/slnf 调度。
- **2026-08-21 / CP-246 完成**：切断 Host MSBuild 命令后原 `full` 明确从 `microsoft/bin/x64/Release/OpenConsole.exe` 打包并 exit 0；随后删除旧 vcxproj、filters、metaproj、solution 工程项、两条 solution-only BuildDependency 和 conhost slnf 项。删除后三条固定命令再次全部 exit 0，产品包、workspace extension、repack 完成，旧路径/GUID 残留 0，solution/slnf 可解析；当前非 tmp vcxproj 20。连续 solution 块删除产生的混合换行已机械归一化，`git diff --check` 无 whitespace error。
- **2026-08-21 / CP-247 启动**：下一批先迁移 `cascadia/UnitTests_TerminalCore/UnitTests.vcxproj`。该 TAEF DLL 只有 8 个编译单元，无 XAML/MIDL/RC，直接消费已经原生化的 `TerminalCore`、`ConTypes`、`ConRenderAtlas`、`TerminalInput`、`ConTermAdapt`、`ConRenderBase`、`ConBufferOut`、`ConTermParser`；目标输出固定为 `Terminal.Core.Unit.Tests.dll`，构建和 TAEF 枚举通过后删除旧工程与调度。
- **2026-08-21 / CP-247 构建检查点**：8 个源码已由 Ninja 直接生成 1,054,720 字节 `Terminal.Core.Unit.Tests.dll`；测试目标消费自有 TerminalCore WinMD/projection，不读取旧 `Generated Files`，TAEF `/list` exit 0 并输出 63 行测试清单。下一步删除旧 vcxproj 和 solution 调度；完整回归与后续无 XAML 测试批次合并执行，以减少重复产品打包但不跳过三条固定命令。
- **2026-08-21 / CP-248 启动**：目标固定为 `cascadia/TerminalConnection/TerminalConnection.vcxproj`，把剩余 WinRT 产品图的第一个非 XAML 叶子彻底交给 Ninja。5 个 IDL 分别由固定 Windows SDK `midl.exe` 生成独立 unmerged WinMD，再由 `mdmerge.exe -partial` 合成唯一 `Microsoft.Terminal.TerminalConnection.winmd`，随后固定 `cppwinrt.exe -component` 生成 `*.g.h`、`module.g.cpp` 和投影头；8 个实现编译单元只读取 `build` 下的新生成目录，禁止读取源码树旧 `Generated Files`。`Resources/en-US/Resources.resw` 由固定 `MakePri.exe` 生成同名项目 PRI。验收检查点依次为：五个独立 WinMD、合并 WinMD、组件投影、`TerminalConnection.dll`/import lib/PRI；删除任一生成文件后的精确重建；产品 `full` 实际消费三项输出；清除全部旧 ProjectReference、solution/filter/vcxproj 和旧生成目录依赖。全过程不增加 MSBuild 回退、包装脚本、preset 或第二种编译用法。
- **2026-08-21 / CP-248 构建检查点**：固定 SDK 生成链已由 Ninja 从 5 个 IDL 产出五份 unmerged WinMD，`mdmerge -partial` 合成 7,680 字节 `Microsoft.Terminal.TerminalConnection.winmd`，固定 C++/WinRT 工具用 `-component/-prefix/-optimize` 生成 4 组 `g.h/g.cpp`、`module.g.cpp` 和投影；8 个实现编译单元全部读取 `build/terminal-connection/Release/projection`。资源规则直接索引 16 份 RESW，新 PRI 为 33,448 字节，dump 后与旧基线同为 31 个 NamedResource、496 个 Candidate。最终 Ninja DLL 为 606,208 字节且导出 `DllCanUnloadNow`、`DllGetActivationFactory`；初次链接据真实未解析符号补齐唯一 `fmt.lib`。旧 vcxproj、8 个直接 ProjectReference、solution 工程项和 3 条 BuildDependency 已删除，`full` foundation 现直接要求该 target。下一检查点执行三条固定命令并核验产品包实际包含 DLL/WinMD/PRI，然后做删除生成文件的精确重建。
- **2026-08-21 / CP-247 完成**：与 CP-248 合并执行的固定 `cmake -S . -B .\build`、`cmake --build .\build`、`cmake --build .\build --target full` 已全部 exit 0；Terminal Core 单测目标和已删除工程仍保持旧路径/GUID 命中 0，CP-247 关闭。
- **2026-08-21 / CP-248 完成**：删掉旧 ProjectReference 后第一次 `full` 精确暴露 `TerminalControlLib` 的 MIDL 类型闭包缺口，现已把所有 8 个原消费者改成直接 `Reference` 唯一 Ninja WinMD；不恢复工程级依赖。沙箱外同文固定 `full` 随后 exit 0，实际 MIDL/cppwinrt 日志反复读取 `microsoft/bin/x64/Release/TerminalConnection/Microsoft.Terminal.TerminalConnection.winmd`，最终 MSIX 明确打包新 DLL 和 WinMD，项目 PRI也被下游 MakePri 读取并并入最终 `resources.pri`。精确删除 `build/terminal-connection/Release/projection/module.g.cpp` 后，固定默认构建只重新执行 WinMD merge/component projection、该对象和受影响链接，DLL恢复为 606,208 字节。旧 vcxproj/filters、8 个 ProjectReference、solution 项/GUID命中均为 0；当前非 tmp vcxproj 18，XML有效，`git diff --check` 无 whitespace error。下一目标 CP-249 为无 XAML 的 WinRT 叶子 `UIHelpers`。
- **2026-08-21 / CP-249 启动**：目标固定为 `cascadia/UIHelpers/UIHelpers.vcxproj`。先清点 5 个 IDL、7 个 C++ 编译单元、16 份本地化资源、DEF、输出 WinMD/PRI 和直接消费者；复用 CP-248 已验证的唯一 MIDL→mdmerge→cppwinrt 与 MakePri 合同生成 `Microsoft.Terminal.UI.dll/.winmd/.pri`，但每个项目仍有独立明确的生成清单。证明后一次删除旧工程、filters、ProjectReference 与 solution 调度，不读取源码树旧 `Generated Files`，不增加通用兼容层或第二种入口。
- **2026-08-21 / CP-249 构建与切换检查点**：Ninja 已由 5 个 IDL 生成独立 unmerged WinMD，`mdmerge -partial` 合成 5,120 字节 `Microsoft.Terminal.UI.winmd`；MUX 元数据的真实闭包还需要固定 WebView2 WinMD，因此同一 cppwinrt 规则先生成 WebView2、再生成 MUX、最后生成 UIHelpers component projection，不读取任何旧 MSBuild `Generated Files`。7 个对象成功链接为 578,048 字节 `Microsoft.Terminal.UI.dll`，16 份 RESW 生成 4,240 字节 PRI，现有 SVG 工具规则生成并复制 workspace icons。8 个旧 ProjectReference 已全部改为直接引用唯一 Ninja WinMD，`full` foundation 已加入 UIHelpers target，旧 vcxproj/filters 和 solution 项已删除。下一检查点执行三条固定命令，确认所有下游 MIDL/cppwinrt、最终 MSIX 与图标资源实际消费新输出，再做生成文件删除重建证明和残留计数。
- **2026-08-21 / CP-249 完成**：固定 `cmake -S . -B .\build`、`cmake --build .\build`、沙箱外同文 `cmake --build .\build --target full` 全部 exit 0。TerminalControl、Settings Editor、TerminalApp 等下游 MIDL/cppwinrt 日志明确读取唯一 Ninja `Microsoft.Terminal.UI.winmd`，TerminalApp MakePri 明确 dump 新 `Microsoft.Terminal.UI.pri`，最终 MSIX 明确打包新 DLL/WinMD，产品包、workspace extension、full repack 全部完成。DLL 保留 `DllCanUnloadNow`/`DllGetActivationFactory`，workspace icon 递归文件数 696。工作区边界校验后精确删除 `build/ui-helpers/Release/projection/module.g.cpp`，固定默认构建只重跑合并/component projection、该对象、DLL 和受影响链接并恢复文件。8 个直接 WinMD Reference 存在，旧 ProjectReference/路径/GUID 命中 0，全部 XML 有效，`git diff --check` 无 whitespace error；当前非 tmp vcxproj 17。
- **2026-08-21 / CP-250 启动**：下一目标固定为同一非 XAML WinRT 链的 `TerminalSettingsModel/Microsoft.Terminal.Settings.ModelLib.vcxproj` 与薄 DLL `dll/Microsoft.Terminal.Settings.Model.vcxproj`，同批迁移以删除 `full` 中现存的独立 Settings Model MSBuild 命令。先锁定两个工程的全部 CPP/IDL、DEF、资源、WinMD 合并与 cppwinrt 生成合同、静态库到薄 DLL 的边界、直接 ProjectReference 和最终产品消费者；实现必须直接依赖已迁移的 TerminalCore/TerminalConnection/WinRTUtils 及当前 Control WinMD，不读取旧 obj 或源码树 `Generated Files`，不增加 wrapper、preset、回退或第二套编译用法。
- **2026-08-21 / CP-250 拓扑更正**：完整 XML 和 `full` 日志证明 Settings Model 的 18 个 IDL 必须消费 TerminalControl WinMD，而旧链仅靠 Settings DLL 对 Control DLL 的 ProjectReference 隐式提前构建；因此不能先删除 Settings 工程再偷用现存 Control 输出，否则干净构建不闭合。真正产品顺序修正为 TerminalControl Lib/DLL → Settings Model Lib/DLL。SDK XAML 编译器只有 `Microsoft.Windows.UI.Xaml.Build.Tasks.dll` 的 MSBuild task，没有独立 `XamlCompiler.exe`，后续必须把该 task 的输入输出收敛为 CMake 可直接拥有的正式生成步骤，不能包装调用旧 vcxproj。为避免调查期间停滞，本检查点先同批迁移三个自身无 XAML/MIDL 的叶子 TAEF DLL：`UnitTests_Control`、`UnitTests_SettingsModel`、`ut_app`，复用已证明的 TAEF/C++20/静态 CRT 合同和现有产品 WinMD，证明后直接删除三个旧测试工程与调度，把剩余工程 17→14。
- **2026-08-21 / CP-250 测试批次撤回**：上一检查点最后一句关于先迁移三个测试的判断无效，已在实施前撤回；三个测试都直接包含产品组件生成的 `*.g.h`，不能在产品 XAML/component 生成图闭合前迁移。没有为这三个测试添加 CMake target、没有删除其工程或调度，非 tmp vcxproj 仍为 17。CP-250 唯一当前范围恢复为先建立 XAML 编译正式步骤并按 TerminalControl Lib/DLL → Settings Model Lib/DLL 的真实拓扑推进。
- **2026-08-21 / CP-250 XAML 编译合同证明**：新增的 `XamlCompilerHost` 是 CMake 内部的单用途 SDK task host，不是用户构建入口，也不调用 `msbuild.exe`、不读取 vcxproj；固定入口仍只有既定三条命令。它直接加载 Windows SDK 的 `Microsoft.Windows.UI.Xaml.Build.Tasks.dll`，用 SDK ABI 所需的 .NET Framework 4 `Microsoft.Build.Framework.dll` 承载 `CompileXaml` task。以 TerminalControl 两份 XAML、全部固定 Windows SDK contract WinMD、MUX/WebView2 和本项目 WinMD 执行 `RealBuildPass1`、`RealBuildPass2` 均成功；两份重写 XAML、两份 `*.xaml.g.h`、`XamlLibMetadataProvider.g.cpp`、`XamlTypeInfo.Impl.g.cpp` 与旧输出逐字节相同，XBF 由当前正式输入重新生成。CMake 中已开始建立 TerminalControl 的 16 份 IDL、MIDL→mdmerge→cppwinrt、两阶段 XAML 和静态实现库/薄 DLL 唯一生成图；下一检查点以 `cmake --build .\build --target TerminalControl` 的实际编译链接结果修正闭包，再补齐 PRI、消费者切换和旧工程删除。
- **2026-08-21 / CP-250 XAML 编译合同纠正**：上一条把一次带有既存状态文件/输出的隔离试验误判为 Pass2 已闭合，现予撤销。可复现的正式结果是：Pass1 用 5 个产品 WinMD、精确 33 个 SDK contract 和 Facade `Windows.winmd` 成功，生成的两份 `*.xaml.g.h` 与隔离基线逐字节相同；Pass2 对本地 Control WinMD 解析 `Windows.UI.Xaml.Media.XamlLight` 时失败。用重新合并的旧 Control WinMD 也同样失败，证明问题不在新 MIDL/mdmerge 产物；直接执行旧工程的 `MarkupCompilePass2` 亦复现该错误，说明不能靠恢复 vcxproj 或 MSBuild 调度解决。当前检查点固定为查清 SDK task 在干净 Pass2 中所需的平台元数据合同，使 Pass1/Pass2 都能从空输出成功；在此之前不复制旧 XBF、不读取源码树 `Generated Files`、不删除 TerminalControl 工程，也不宣称 XAML 链完成。
- **2026-08-21 / CP-250 XAML 根因与干净生成检查点**：Pass2 的 `XamlLight` 错误根因已定位到新 Control WinMD 的合并元数据输入，而不是 XAML task：此前 `mdmerge` 误把 UnionMetadata 主 `Windows.winmd` 与精确 SDK contract 同时作为搜索根，导致 `VisualBellLight` 的基类作用域坍缩为 `[Windows]Windows.UI.Xaml.Media.XamlLight`；旧正确 WinMD 使用 `[Windows.Foundation.UniversalApiContract]...XamlLight`。现已删除主 UnionMetadata 输入，`mdmerge` 只接收从 38 个产品/包/SDK引用文件推导出的精确目录，新 WinMD 的类型作用域恢复正确。随后从空状态执行 `RealBuildPass1`、`RealBuildPass2` 均成功并生成两份 XBF。`ClInclude` 的 `DependentUpon` 关系也已按原工程逐项传给 task：8 个 IDL 实现头、`SearchBoxControl.h`、`TermControl.h` 被纳入 `XamlTypeInfo`；独立删除状态文件及两份 `*.xaml.g.hpp` 后，固定 CMake target 重新运行两阶段编译，生成头仅包含 `SearchBoxControl.h`/`TermControl.h`，证明不依赖旧状态、源码树 `Generated Files` 或 MSBuild 输出。下一检查点是完成 TerminalControl 静态实现库和薄 DLL 的实际编译链接，再建立 PRI、切换全部消费者并删除两个旧工程。
- **2026-08-21 / CP-250 TerminalControl 切换检查点**：TerminalControl 的 24 个实现/XAML 生成编译单元已由 Ninja 生成唯一 `Microsoft.Terminal.ControlLib.lib`，薄 DLL 已直接链接成功；链接过程中暴露的唯一外部符号 `TextMenuFlyout` 不是缺库，而是错误读取 UIHelpers 的 component projection，现已建立只包含 TerminalCore、TerminalConnection、UIHelpers、MUX、WebView2 的正式 consumer projection并删除错误输入顺序，DLL 最终成功。MakePri 使用独立 RESW index 与 `EMBEDFILES` index，生成的 `Microsoft.Terminal.Control.pri` 与旧合同同为 67,888 字节、62 个 NamedResource、900 个 Candidate，两份 XBF 均为 EmbeddedData；无资源薄 DLL 的 728 字节空 PRI 已从产品合并清单删除，不创建占位。10 个旧 ProjectReference/solution BuildDependency 已切断，元数据消费者直接引用唯一 Ninja WinMD，Control 单测直接引用唯一 Ninja archive；Lib/DLL 两个 vcxproj及 metaproj、solution 项已删除，`full` foundation 已直接依赖新 target。下一检查点执行固定三条命令，确认 Settings Model MIDL、最终产品 PRI/MSIX 实际消费新 Control 输出并完成删除生成文件重建证明；通过后关闭 TerminalControl，紧接着迁移 Settings Model Lib/DLL。
- **2026-08-21 / CP-250 完整链推进检查点**：配置与默认构建 exit 0；沙箱内 `full` 只触发已知 FileTracker E_ACCESSDENIED，沙箱外同文命令中 Settings Model 的 18 个 MIDL 均明确读取唯一 Ninja Control WinMD，Settings Model Lib/DLL 编译链接成功。产品递归到 Settings Editor 后才失败于其旧 MSBuild `MarkupCompilePass2`：`IconSource` 被声明在 `Windows.winmd` 而不是 SDK contract。元数据表逐项检查确认旧/新 Control WinMD 的 `XamlLight` 都正确指向 `Windows.Foundation.UniversalApiContract`，且 Control WinMD 不含 `IconSource` TypeRef，因此不能回滚 Control 或增加兼容引用；这是真正的下游旧 XAML 干净构建缺陷。执行顺序固定为立即完成 Settings Model Lib/DLL 的 Ninja 切换并删除 `full` 中最后一条独立 Settings MSBuild 命令，随后用已证明的精确 contract/XAML host迁移 Settings Editor，不能用缓存、旧 Generated Files 或恢复 ProjectReference 掩盖错误。
- **2026-08-21 / CP-250 Settings Model 原生构建检查点**：根 CMake 已直接拥有 Settings Model 的 18 个 IDL→独立 WinMD→`mdmerge -partial`→component/consumer projection、42 个实现编译单元、三份压缩 JSON、16 份 RESW/PRI、静态实现库和薄 DLL。首次目标构建在 64/68 完成静态库后暴露生成 RC 对无必要 `winres.h` 的隐式搜索依赖，现已直接删除该 include；随后链接按真实未解析符号补齐固定 vcpkg `fmt.lib`/`jsoncpp.lib`、Setup Configuration Native 以及 `ntdll`/`shlwapi`/`pathcch`/`propsys`，`cmake --build .\build --target TerminalSettingsModel` exit 0，并把唯一 WinMD 复制到既定 DLL 输出目录。没有读取旧 obj/Generated Files，没有调用 vcxproj/MSBuild，也没有增加 preset、wrapper、兼容分支或新用户入口。下一检查点立即把所有 Settings Model ProjectReference 改为直接 WinMD/唯一 archive 消费，删除 Lib/DLL 工程、solution 调度和 `full` 的独立 MSBuild 命令，再执行残留/XML/增量与固定三命令验证。
- **2026-08-21 / CP-250 Settings Model 切换检查点**：7 个现存消费者已全部切断 Settings Model Lib/DLL ProjectReference；运行时/元数据消费者改为直接读取 `Microsoft.Terminal.Settings.Model.winmd`，两个静态测试消费者显式链接唯一 Ninja `Microsoft.Terminal.Settings.Model.Lib.lib` 并直接引用其 WinMD。`full` foundation 已直接要求 `TerminalSettingsModel`，根 CMake 中该 DLL 的独立 MSBuild 命令已删除，adapter 对新目标建立显式生成依赖。Lib/DLL 的两个 vcxproj、filters/metaproj、solution 工程块及 10 条 solution 调度依赖已删除；旧工程路径引用为 0、现存 vcxproj/slnx XML 错误为 0，非 tmp vcxproj 从 15 降至 13。下一步不等待旧 Editor 回归：直接迁移造成当前 `full` 失败的 Settings Editor XAML/WinRT 目标；完成后再执行固定三命令并关闭 CP-250。
- **2026-08-21 / CP-251 Settings Editor 元数据根因检查点**：根 CMake 已直接拥有 Settings Editor 的 35 个 IDL（含 XAML 元数据 provider）、独立 WinMD、`mdmerge -partial`、component/consumer projection、24 个 XAML 页的两阶段 SDK 编译、设置搜索索引生成、24 份 XBF 与 RESW 的 PRI，以及全部实现编译单元和最终 DLL。干净 XAML 首次暴露的 `IconSource in Windows.winmd` 不是 Editor 页面或 Control WinMD 本身的问题，而是更早迁移中三个元数据合同错误：TerminalCore 单 IDL 的 unmerged assembly 被以 `ICoreSettings.winmd` 错名直接当最终 WinMD；TerminalConnection/UIHelpers 的 MIDL/mdmerge 错把 UnionMetadata 主 `Windows.winmd` 作为平台定义。现已让 TerminalCore 先生成真实 `ICoreSettings.winmd` 再合并为内部 assembly 名同为 `Microsoft.Terminal.Core` 的最终 WinMD，并让 TerminalConnection/UIHelpers 只使用 SDK contract metadata。下游全链重新生成后，Editor 的 24 页 Pass1/Pass2、XBF 与 PRI 均从空输出成功，证明不需要主 `Windows.winmd`、旧状态或 MSBuild 输出。首次完整编译 135/139 已通过，唯一剩余链接符号是 `DwmGetWindowAttribute`；目标链接闭包加入其直接系统库 `dwmapi.lib` 后立即复验。当前剩余非 tmp vcxproj 为 13，仍须全部迁移，不在此检查点停下。
- **2026-08-21 / CP-251 Settings Editor 构建与切换检查点**：补齐直接 `dwmapi.lib` 后 `cmake --build .\build --target TerminalSettingsEditor` exit 0，最终 DLL、import lib、WinMD、PRI 和全部 24 份 XBF 均由 Ninja 唯一拥有。5 个现存消费者已切断旧 Editor ProjectReference；已有直接 WinMD 的 TerminalAppLib 删除纯调度引用，其余四项加入同一路径的直接 WinMD Reference。`full` foundation 已显式加入 Editor target，solution 的工程块和三条 BuildDependency、两个残留 metaproj 调度块以及 vcxproj/filters/metaproj 已删除。剩余非 tmp vcxproj 12；下一目标固定为较小的 XAML/WinRT 组件 `UIMarkdown`，复用同一已证明的直接 MIDL→mdmerge→cppwinrt→两阶段 XAML→PRI 合同，不增加另一套构建写法。
- **2026-08-23 / 最终 x64 验收**：修复了 Debug/Release 的原生构建闭包：Debug 显式定义 `DBG`、禁用增量链接并统一采用静态 CRT；Settings Model 单测按配置提供唯一的 public `KeyChord` factory-constructor 支持，避免 Release 漏符号与 Debug 同名定义冲突；TerminalCore PRI 及 Microsoft.UI.Xaml 运行时 DLL 均由 CMake/Ninja 从声明的 SDK/NuGet 输入生成或解包，不依赖已有 Release 输出。最终 `cmake --build .\build --config Debug --target full -- -j 1` 和同一 Release 命令均 exit 0，均完成 native-product-foundation、terminal-settings-adapter、native-product-shims、workspace-extension-full 与 full-repack；两份最终日志的编译/链接/包输入错误扫描均为 0。`SettingsModel.Unit.Tests.dll`、`TerminalCore/Microsoft.Terminal.Core.pri` 和 `WindowsTerminal/Microsoft.UI.Xaml.dll` 均在 Debug/Release 输出中存在；迁移入口/管线静态扫描未发现 MSBuild、VSBuild、MSBuild.exe、dotnet build/publish/msbuild 或 `.appxrecipe` 调用。
