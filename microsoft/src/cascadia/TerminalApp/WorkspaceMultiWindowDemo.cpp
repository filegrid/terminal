// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "TerminalPage.h"
#include "../../../../src/core/chat/WorkspaceDiagnosticLog.h"

#include <winrt/Microsoft.Web.WebView2.Core.h>
#include "../../../packages/Microsoft.Web.WebView2.1.0.1661.34/build/native/include/WebView2.h"
#include <wrl.h>
#include <winrt/Windows.UI.Composition.h>
#include <winrt/Windows.UI.Xaml.Hosting.h>

#include <filesystem>

using namespace winrt;
using namespace winrt::Windows::UI;
using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Windows::UI::Xaml::Controls;
using namespace winrt::Windows::UI::Xaml::Controls::Primitives;
using namespace winrt::Windows::UI::Xaml::Media;

namespace winrt::TerminalApp::implementation
{
    namespace
    {
        using ::Microsoft::WRL::Callback;
        using ::Microsoft::WRL::ComPtr;

        std::wstring _WorkspaceWebViewDemoDataDirectory()
        {
            std::wstring value(32768, L'\0');
            const auto length = GetEnvironmentVariableW(L"WT_PORTABLE_ROOT", value.data(), gsl::narrow_cast<DWORD>(value.size()));
            if (length > 0 && length < value.size())
            {
                value.resize(length);
                return value + L"\\webview\\native-host-demo";
            }
            return std::filesystem::temp_directory_path().wstring() + L"\\WindowsTerminalNativeWebViewDemo";
        }

        struct NativeWebViewDemoHost final : std::enable_shared_from_this<NativeWebViewDemoHost>
        {
            NativeWebViewDemoHost(const Grid& surface, const HWND parentWindow, hstring url) :
                _surface(surface),
                _parentWindow(parentWindow),
                _url(std::move(url)),
                _userDataDirectory(_WorkspaceWebViewDemoDataDirectory())
            {
            }

            ~NativeWebViewDemoHost()
            {
                Close();
            }

            void Initialize()
            {
                Json::Value payload{ Json::objectValue };
                terminal::workspacechat::AddDiagnosticTextFields(payload, "url", _url.c_str());
                terminal::workspacechat::AddDiagnosticTextFields(payload, "userDataDirectory", _userDataDirectory);
                payload["parentWindow"] = Json::UInt64{ gsl::narrow_cast<uint64_t>(reinterpret_cast<uintptr_t>(_parentWindow)) };
                std::ignore = terminal::workspacechat::AppendWorkspaceDiagnosticLog(L"workspace_native_webview_demo_create_requested", payload);

                _visual = Windows::UI::Xaml::Hosting::ElementCompositionPreview::GetElementVisual(_surface).Compositor().CreateContainerVisual();
                Windows::UI::Xaml::Hosting::ElementCompositionPreview::SetElementChildVisual(_surface, _visual);
                _surface.SizeChanged([weak = weak_from_this()](auto&&, auto&&) {
                    if (const auto self = weak.lock())
                    {
                        self->_Resize();
                    }
                });
                _surface.PointerPressed([weak = weak_from_this()](auto&&, const auto& args) {
                    if (const auto self = weak.lock())
                    {
                        self->_SendMouse(COREWEBVIEW2_MOUSE_EVENT_KIND_LEFT_BUTTON_DOWN, args);
                    }
                });
                _surface.PointerReleased([weak = weak_from_this()](auto&&, const auto& args) {
                    if (const auto self = weak.lock())
                    {
                        self->_SendMouse(COREWEBVIEW2_MOUSE_EVENT_KIND_LEFT_BUTTON_UP, args);
                    }
                });
                _surface.PointerMoved([weak = weak_from_this()](auto&&, const auto& args) {
                    if (const auto self = weak.lock())
                    {
                        self->_SendMouse(COREWEBVIEW2_MOUSE_EVENT_KIND_MOVE, args);
                    }
                });

                std::error_code error;
                std::filesystem::create_directories(_userDataDirectory, error);
                const auto callback = Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
                    [self = shared_from_this()](const HRESULT result, ICoreWebView2Environment* environment) -> HRESULT {
                        if (FAILED(result) || !environment)
                        {
                            self->_LogFailure(L"workspace_native_webview_demo_environment_failed", result);
                            return S_OK;
                        }
                        self->_environment = environment;
                        ComPtr<ICoreWebView2Environment3> environment3;
                        if (const auto queryResult = environment->QueryInterface(IID_PPV_ARGS(&environment3)); FAILED(queryResult))
                        {
                            self->_LogFailure(L"workspace_native_webview_demo_composition_unavailable", queryResult);
                            return S_OK;
                        }
                        return environment3->CreateCoreWebView2CompositionController(
                            self->_parentWindow,
                            Callback<ICoreWebView2CreateCoreWebView2CompositionControllerCompletedHandler>(
                                [self](const HRESULT controllerResult, ICoreWebView2CompositionController* controller) -> HRESULT {
                                    if (FAILED(controllerResult) || !controller)
                                    {
                                        self->_LogFailure(L"workspace_native_webview_demo_controller_failed", controllerResult);
                                        return S_OK;
                                    }
                                    self->_compositionController = controller;
                                    if (FAILED(controller->QueryInterface(IID_PPV_ARGS(&self->_controller))))
                                    {
                                        self->_LogFailure(L"workspace_native_webview_demo_controller_interface_failed", E_NOINTERFACE);
                                        return S_OK;
                                    }
                                    if (FAILED(self->_controller->get_CoreWebView2(&self->_core)))
                                    {
                                        self->_LogFailure(L"workspace_native_webview_demo_core_failed", E_FAIL);
                                        return S_OK;
                                    }
                                    EventRegistrationToken navigationToken{};
                                    std::ignore = self->_core->add_NavigationCompleted(
                                        Callback<ICoreWebView2NavigationCompletedEventHandler>(
                                            [self](ICoreWebView2*, ICoreWebView2NavigationCompletedEventArgs* args) -> HRESULT {
                                                BOOL success{};
                                                COREWEBVIEW2_WEB_ERROR_STATUS status{};
                                                if (args)
                                                {
                                                    std::ignore = args->get_IsSuccess(&success);
                                                    std::ignore = args->get_WebErrorStatus(&status);
                                                }
                                                Json::Value payload{ Json::objectValue };
                                                payload["success"] = success == TRUE;
                                                payload["webErrorStatus"] = static_cast<int>(status);
                                                terminal::workspacechat::AddDiagnosticTextFields(payload, "url", self->_url.c_str());
                                                std::ignore = terminal::workspacechat::AppendWorkspaceDiagnosticLog(L"workspace_native_webview_demo_navigation_completed", payload);
                                                return S_OK;
                                            }).Get(),
                                        &navigationToken);
                                    EventRegistrationToken processFailedToken{};
                                    std::ignore = self->_core->add_ProcessFailed(
                                        Callback<ICoreWebView2ProcessFailedEventHandler>(
                                            [self](ICoreWebView2*, ICoreWebView2ProcessFailedEventArgs* args) -> HRESULT {
                                                COREWEBVIEW2_PROCESS_FAILED_KIND kind{};
                                                if (args)
                                                {
                                                    std::ignore = args->get_ProcessFailedKind(&kind);
                                                }
                                                Json::Value payload{ Json::objectValue };
                                                payload["processFailedKind"] = static_cast<int>(kind);
                                                terminal::workspacechat::AddDiagnosticTextFields(payload, "url", self->_url.c_str());
                                                std::ignore = terminal::workspacechat::AppendWorkspaceDiagnosticLog(L"workspace_native_webview_demo_process_failed", payload);
                                                return S_OK;
                                            }).Get(),
                                        &processFailedToken);
                                    const auto visualUnknown = reinterpret_cast<IUnknown*>(winrt::get_abi(self->_visual));
                                    if (const auto targetResult = controller->put_RootVisualTarget(visualUnknown); FAILED(targetResult))
                                    {
                                        self->_LogFailure(L"workspace_native_webview_demo_visual_target_failed", targetResult);
                                        return S_OK;
                                    }
                                    self->_controller->put_IsVisible(TRUE);
                                    self->_Resize();
                                    self->_core->Navigate(self->_url.c_str());
                                    Json::Value payload{ Json::objectValue };
                                    terminal::workspacechat::AddDiagnosticTextFields(payload, "url", self->_url.c_str());
                                    std::ignore = terminal::workspacechat::AppendWorkspaceDiagnosticLog(L"workspace_native_webview_demo_ready", payload);
                                    return S_OK;
                                }).Get());
                    });
                const auto result = CreateCoreWebView2EnvironmentWithOptions(nullptr, _userDataDirectory.c_str(), nullptr, callback.Get());
                if (FAILED(result))
                {
                    _LogFailure(L"workspace_native_webview_demo_environment_request_failed", result);
                }
            }

            void Close() noexcept
            {
                if (_controller)
                {
                    _controller->Close();
                }
                _core.Reset();
                _controller.Reset();
                _compositionController.Reset();
                _environment.Reset();
                if (_surface && _visual)
                {
                    Windows::UI::Xaml::Hosting::ElementCompositionPreview::SetElementChildVisual(_surface, nullptr);
                }
                _visual = nullptr;
            }

        private:
            void _Resize()
            {
                if (!_controller || !_visual)
                {
                    return;
                }
                const auto scale = Windows::Graphics::Display::DisplayInformation::GetForCurrentView().RawPixelsPerViewPixel();
                const auto width = std::max(0L, gsl::narrow_cast<LONG>(std::lround(_surface.ActualWidth() * scale)));
                const auto height = std::max(0L, gsl::narrow_cast<LONG>(std::lround(_surface.ActualHeight() * scale)));
                _visual.Size({ static_cast<float>(width), static_cast<float>(height) });
                std::ignore = _controller->put_Bounds(RECT{ 0, 0, width, height });
            }

            void _SendMouse(const COREWEBVIEW2_MOUSE_EVENT_KIND kind, const Windows::UI::Xaml::Input::PointerRoutedEventArgs& args)
            {
                if (!_compositionController)
                {
                    return;
                }
                const auto scale = Windows::Graphics::Display::DisplayInformation::GetForCurrentView().RawPixelsPerViewPixel();
                const auto position = args.GetCurrentPoint(_surface).Position();
                const POINT point{
                    gsl::narrow_cast<LONG>(std::lround(position.X * scale)),
                    gsl::narrow_cast<LONG>(std::lround(position.Y * scale))
                };
                std::ignore = _compositionController->SendMouseInput(kind, COREWEBVIEW2_MOUSE_EVENT_VIRTUAL_KEYS_NONE, 0, point);
                if (kind == COREWEBVIEW2_MOUSE_EVENT_KIND_LEFT_BUTTON_DOWN && _controller)
                {
                    std::ignore = _controller->MoveFocus(COREWEBVIEW2_MOVE_FOCUS_REASON_PROGRAMMATIC);
                }
            }

            void _LogFailure(const std::wstring_view eventName, const HRESULT result) const
            {
                Json::Value payload{ Json::objectValue };
                payload["hresult"] = static_cast<int>(result);
                terminal::workspacechat::AddDiagnosticTextFields(payload, "url", _url.c_str());
                terminal::workspacechat::AddDiagnosticTextFields(payload, "userDataDirectory", _userDataDirectory);
                std::ignore = terminal::workspacechat::AppendWorkspaceDiagnosticLog(eventName, payload);
            }

            Grid _surface{ nullptr };
            HWND _parentWindow{};
            hstring _url;
            std::wstring _userDataDirectory;
            Windows::UI::Composition::ContainerVisual _visual{ nullptr };
            ComPtr<ICoreWebView2Environment> _environment;
            ComPtr<ICoreWebView2CompositionController> _compositionController;
            ComPtr<ICoreWebView2Controller> _controller;
            ComPtr<ICoreWebView2> _core;
        };
    }

    UIElement TerminalPage::_BuildWorkspaceMultiWindowDemo(const int webViewHostMode)
    {
        // Deliberately local-only: this is the Step 1 Host visual prototype,
        // not a workspace model, terminal launcher, or persistence feature.
        struct DemoCommand
        {
            hstring Icon;
            hstring Name;
            hstring Command;
            hstring Title;
        };
        struct DemoState
        {
            std::vector<DemoCommand> Commands{
                { L"workspace-icon://color/development/0", L"Codex", L"codex", L"Codex — workspace" },
                { L"workspace-icon://color/development/1", L"Claude Code", L"claude", L"Claude Code — review" },
            };
            std::vector<double> Weights{ 0.55, 0.45 };
            bool Split{ true };
            int TabPlacement{ 0 }; // 0: left-top, 1: right-top, 2: right-bottom
            size_t Active{ 0 };
            // This is deliberately local to the demo. It does not alter the
            // workspace command host or any persisted workspace setting.
            bool WebViewHostComparison{};
            int WebViewHostMode{}; // 0: XAML WebView2; 1: native composition controller
            std::vector<std::shared_ptr<NativeWebViewDemoHost>> NativeWebViews;

            explicit DemoState(const int mode) :
                WebViewHostComparison(mode >= 0),
                WebViewHostMode(mode >= 0 ? mode : 0)
            {
            }
        };

        const auto state = std::make_shared<DemoState>(webViewHostMode);
        const auto host = Grid{};
        host.Margin(ThicknessHelper::FromLengths(16, 16, 16, 16));
        // Keep the prototype visually and behaviorally tied to the existing
        // workspace manager instead of introducing an unrelated control set.
        auto workspaceResources = ResourceDictionary{};
        workspaceResources.Source(Windows::Foundation::Uri{ L"ms-appx:///TerminalApp/WorkspaceSettingsResources.xaml" });
        const auto applyWorkspaceStyle = [workspaceResources](const auto& control, const wchar_t* key) {
            const auto resourceKey = box_value(key);
            if (workspaceResources.HasKey(resourceKey))
            {
                if (const auto style = workspaceResources.Lookup(resourceKey).try_as<winrt::Windows::UI::Xaml::Style>())
                {
                    control.Style(style);
                }
            }
        };
        const auto rebuild = std::make_shared<std::function<void()>>();

        *rebuild = [this, host, state, rebuild, applyWorkspaceStyle]() {
            for (const auto& nativeWebView : state->NativeWebViews)
            {
                nativeWebView->Close();
            }
            state->NativeWebViews.clear();
            host.Children().Clear();
            const auto snapSplitWeight = [](const double value, const double minimum, const double combined) {
                constexpr double step = 0.05;
                const auto snapped = std::round(value / step) * step;
                return std::clamp(snapped, minimum, combined - minimum);
            };
            const auto positionRatioBubble = [](const Border& bubble, const Grid& preview, const double pointerX, const double pointerY) {
                const auto x = std::clamp(pointerX + 14.0, 8.0, std::max(8.0, preview.ActualWidth() - 128.0));
                const auto y = std::clamp(pointerY + 14.0, 8.0, std::max(8.0, preview.ActualHeight() - 36.0));
                bubble.Margin(ThicknessHelper::FromLengths(x, y, 0, 0));
            };
            auto page = Grid{};
            auto settingsColumn = ColumnDefinition{};
            // Match the existing workspace editor's 760px settings column.
            // The preview is secondary; the configuration remains the primary
            // surface instead of a bespoke narrow demo sidebar.
            settingsColumn.Width(GridLengthHelper::FromPixels(760));
            page.ColumnDefinitions().Append(settingsColumn);
            page.ColumnDefinitions().Append(ColumnDefinition{});

            auto settingsScroller = ScrollViewer{};
            settingsScroller.VerticalScrollBarVisibility(ScrollBarVisibility::Auto);
            auto settings = StackPanel{};
            settings.Spacing(12);
            settings.Margin(ThicknessHelper::FromLengths(0, 0, 20, 0));
            applyWorkspaceStyle(settings, L"WorkspaceSettingsStackStyle");
            settingsScroller.Content(settings);
            page.Children().Append(settingsScroller);
            const auto makeWorkspaceSetting = [&applyWorkspaceStyle](const hstring& label, const UIElement& content) {
                auto setting = ContentControl{};
                setting.Tag(box_value(label));
                const auto element = content.as<FrameworkElement>();
                element.HorizontalAlignment(HorizontalAlignment::Right);
                setting.Content(content);
                applyWorkspaceStyle(setting, L"WorkspaceSettingContainerStyle");
                return setting;
            };
            const auto makeSectionTitle = [&applyWorkspaceStyle](const hstring& text) {
                auto section = TextBlock{};
                section.Text(text);
                applyWorkspaceStyle(section, L"WorkspaceSectionHeaderStyle");
                return section;
            };

            auto commandHeader = Grid{};
            commandHeader.ColumnDefinitions().Append(ColumnDefinition{});
            auto commandButtonsColumn = ColumnDefinition{};
            commandButtonsColumn.Width(GridLengthHelper::Auto());
            commandHeader.ColumnDefinitions().Append(commandButtonsColumn);
            auto commandTitle = makeSectionTitle(L"命令窗口");
            commandHeader.Children().Append(commandTitle);
            auto commandButtons = StackPanel{};
            commandButtons.Orientation(Orientation::Horizontal);
            commandButtons.Spacing(4);
            commandButtons.VerticalAlignment(VerticalAlignment::Center);
            Grid::SetColumn(commandButtons, 1);
            auto addCommand = Button{};
            auto addCommandIcon = SymbolIcon{};
            addCommandIcon.Symbol(Symbol::Add);
            addCommand.Content(addCommandIcon);
            ToolTipService::SetToolTip(addCommand, box_value(L"添加命令窗口"));
            addCommand.IsEnabled(state->Commands.size() < 5);
            addCommand.Click([state, rebuild](auto&&, auto&&) {
                state->Commands.push_back({ L"+", L"未命名命令", L"", L"未命名终端" });
                state->Weights.assign(state->Commands.size(), 1.0 / static_cast<double>(state->Commands.size()));
                (*rebuild)();
            });
            commandButtons.Children().Append(addCommand);
            commandHeader.Children().Append(commandButtons);
            settings.Children().Append(commandHeader);
            auto commandList = ListView{};
            commandList.CanDragItems(true);
            commandList.CanReorderItems(true);
            commandList.AllowDrop(true);
            commandList.SelectionMode(ListViewSelectionMode::None);
            for (size_t index = 0; index < state->Commands.size(); ++index)
            {
                auto commandRow = Grid{};
                commandRow.Padding(ThicknessHelper::FromLengths(12, 8, 12, 8));
                commandRow.HorizontalAlignment(HorizontalAlignment::Stretch);
                auto iconColumn = ColumnDefinition{};
                iconColumn.Width(GridLengthHelper::FromPixels(52));
                commandRow.ColumnDefinitions().Append(iconColumn);
                auto nameColumn = ColumnDefinition{};
                nameColumn.Width(GridLengthHelper::FromPixels(200));
                commandRow.ColumnDefinitions().Append(nameColumn);
                auto fieldGap = ColumnDefinition{};
                fieldGap.Width(GridLengthHelper::FromPixels(12));
                commandRow.ColumnDefinitions().Append(fieldGap);
                commandRow.ColumnDefinitions().Append(ColumnDefinition{});
                auto deleteGap = ColumnDefinition{};
                deleteGap.Width(GridLengthHelper::FromPixels(8));
                commandRow.ColumnDefinitions().Append(deleteGap);
                auto deleteColumn = ColumnDefinition{};
                deleteColumn.Width(GridLengthHelper::Auto());
                commandRow.ColumnDefinitions().Append(deleteColumn);
                auto iconButton = Button{};
                // Match the icon affordance in the existing node editor.
                iconButton.Width(44);
                iconButton.Height(44);
                iconButton.MinWidth(44);
                iconButton.MinHeight(44);
                iconButton.Padding(ThicknessHelper::FromLengths(0, 0, 0, 0));
                iconButton.HorizontalContentAlignment(HorizontalAlignment::Center);
                iconButton.VerticalContentAlignment(VerticalAlignment::Center);
                if (auto icon = _CreateNewTabFlyoutIcon(state->Commands[index].Icon))
                {
                    if (const auto element = icon.try_as<FrameworkElement>())
                    {
                        element.Width(32);
                        element.Height(32);
                    }
                    iconButton.Content(icon);
                }
                else
                {
                    auto fallback = SymbolIcon{};
                    fallback.Symbol(Symbol::Page);
                    iconButton.Content(fallback);
                }
                ToolTipService::SetToolTip(iconButton, box_value(L"选择图标"));
                iconButton.Click([weakThis{ get_weak() }, state, rebuild, index](auto&&, auto&&) {
                    [](winrt::weak_ref<TerminalPage> weakThis, std::shared_ptr<DemoState> state, std::shared_ptr<std::function<void()>> rebuild, size_t index) -> safe_void_coroutine {
                        // Exactly the same production picker used by the node
                        // editor. Keep the Host alive across its async dialog
                        // and only retain the demo's local selection value.
                        if (auto self = weakThis.get(); self && self->_workspaceExtension)
                        {
                            const auto selected = co_await self->_workspaceExtension->PickWorkspaceManagerIcon(std::wstring{ state->Commands[index].Icon.c_str() }, std::nullopt);
                            if (!selected.empty())
                            {
                                state->Commands[index].Icon = selected;
                                (*rebuild)();
                            }
                        }
                    }(weakThis, state, rebuild, index);
                });
                commandRow.Children().Append(iconButton);
                auto nameBox = TextBox{};
                nameBox.PlaceholderText(L"名字，例如 Codex");
                nameBox.Text(state->Commands[index].Name);
                nameBox.MinWidth(0);
                nameBox.LostFocus([state, rebuild, index](auto&& sender, auto&&) {
                    state->Commands[index].Name = sender.as<TextBox>().Text();
                    (*rebuild)();
                });
                Grid::SetColumn(nameBox, 1);
                commandRow.Children().Append(nameBox);
                auto commandBox = TextBox{};
                commandBox.PlaceholderText(L"启动命令，例如 codex --resume（可为空）");
                commandBox.Text(state->Commands[index].Command);
                commandBox.MinWidth(320);
                commandBox.HorizontalAlignment(HorizontalAlignment::Stretch);
                commandBox.LostFocus([state, rebuild, index](auto&& sender, auto&&) {
                    state->Commands[index].Command = sender.as<TextBox>().Text();
                    (*rebuild)();
                });
                Grid::SetColumn(commandBox, 3);
                commandRow.Children().Append(commandBox);
                auto remove = Button{};
                auto removeIcon = SymbolIcon{};
                removeIcon.Symbol(Symbol::Delete);
                remove.Content(removeIcon);
                remove.IsEnabled(state->Commands.size() > 1);
                ToolTipService::SetToolTip(remove, box_value(L"删除命令窗口"));
                remove.Click([state, rebuild, index](auto&&, auto&&) {
                    state->Commands.erase(state->Commands.begin() + index);
                    state->Weights.assign(state->Commands.size(), 1.0 / static_cast<double>(state->Commands.size()));
                    state->Active = 0;
                    (*rebuild)();
                });
                Grid::SetColumn(remove, 5);
                commandRow.Children().Append(remove);
                auto item = ListViewItem{};
                applyWorkspaceStyle(item, L"WorkspaceNodeOrderItemStyle");
                item.HorizontalContentAlignment(HorizontalAlignment::Stretch);
                item.Tag(box_value(static_cast<uint32_t>(index)));
                item.Content(commandRow);
                commandList.Items().Append(item);
            }
            commandList.DragItemsCompleted([state, rebuild, commandList](auto&&, auto&&) {
                std::vector<DemoCommand> commands;
                std::vector<double> weights;
                commands.reserve(commandList.Items().Size());
                weights.reserve(commandList.Items().Size());
                for (uint32_t itemIndex = 0; itemIndex < commandList.Items().Size(); ++itemIndex)
                {
                    const auto item = commandList.Items().GetAt(itemIndex).as<ListViewItem>();
                    const auto originalIndex = winrt::unbox_value<uint32_t>(item.Tag());
                    commands.emplace_back(state->Commands.at(originalIndex));
                    weights.emplace_back(state->Weights.at(originalIndex));
                }
                state->Commands = std::move(commands);
                state->Weights = std::move(weights);
                state->Active = 0;
                (*rebuild)();
            });
            settings.Children().Append(commandList);
            {
                auto comparison = CheckBox{};
                comparison.Content(box_value(L"WebView2 宿主对照（打开 3 个现有地址）"));
                comparison.IsChecked(state->WebViewHostComparison);
                comparison.Checked([state, rebuild](auto&&, auto&&) {
                    state->WebViewHostComparison = true;
                    (*rebuild)();
                });
                comparison.Unchecked([state, rebuild](auto&&, auto&&) {
                    state->WebViewHostComparison = false;
                    (*rebuild)();
                });
                settings.Children().Append(makeWorkspaceSetting(L"诊断 Demo", comparison));
                if (state->WebViewHostComparison)
                {
                    auto hostMode = ComboBox{};
                    hostMode.Items().Append(box_value(L"XAML WebView2（当前实现）"));
                    hostMode.Items().Append(box_value(L"原生 CompositionController（Tauri/Wry 路线）"));
                    hostMode.SelectedIndex(state->WebViewHostMode);
                    hostMode.SelectionChanged([state, rebuild](auto&& sender, auto&&) {
                        state->WebViewHostMode = sender.as<ComboBox>().SelectedIndex();
                        (*rebuild)();
                    });
                    settings.Children().Append(makeWorkspaceSetting(L"对照宿主", hostMode));
                }
            }
            if (state->Commands.size() > 1)
            {
            settings.Children().Append(makeSectionTitle(L"多窗口展示"));
            auto modePanel = StackPanel{};
            modePanel.Orientation(Orientation::Horizontal);
            modePanel.Spacing(10);
            for (const auto split : { true, false })
            {
                auto mode = RadioButton{};
                mode.GroupName(L"demo-display-mode");
                mode.Content(box_value(split ? L"左右分隔" : L"Tab"));
                mode.IsChecked(state->Split == split);
                mode.Checked([state, rebuild, split](auto&&, auto&&) { state->Split = split; (*rebuild)(); });
                modePanel.Children().Append(mode);
            }
            settings.Children().Append(makeWorkspaceSetting(L"展示方式", modePanel));
            if (state->Split && state->Commands.size() > 1)
            {
                // A single, continuous allocation control. There is no
                // collection of pairwise sliders: with three windows it has
                // three segments and exactly two movable dividers.
                auto allocation = Grid{};
                allocation.Width(500);
                allocation.Height(54);
                allocation.Margin(ThicknessHelper::FromLengths(0, 0, 0, 8));
                auto labelsRow = RowDefinition{};
                labelsRow.Height(GridLengthHelper::FromPixels(26));
                allocation.RowDefinitions().Append(labelsRow);
                auto railRow = RowDefinition{};
                railRow.Height(GridLengthHelper::FromPixels(12));
                allocation.RowDefinitions().Append(railRow);
                allocation.RowDefinitions().Append(RowDefinition{});
                // The divider handlers are created while this loop is still
                // appending later windows. Keep these collections shared so
                // every handler observes the completed set, rather than a
                // stale by-value prefix that would be indexed out of range.
                const auto allocationColumns = std::make_shared<std::vector<ColumnDefinition>>();
                const auto allocationLabels = std::make_shared<std::vector<TextBlock>>();
                allocationColumns->reserve(state->Commands.size());
                allocationLabels->reserve(state->Commands.size());
                auto rail = Border{};
                rail.Height(4);
                rail.Background(SolidColorBrush{ Color{ 255, 102, 102, 102 } });
                rail.VerticalAlignment(VerticalAlignment::Center);
                Grid::SetRow(rail, 1);
                Grid::SetColumnSpan(rail, static_cast<int>(state->Commands.size() * 2 - 1));
                allocation.Children().Append(rail);
                for (size_t index = 0; index < state->Commands.size(); ++index)
                {
                    auto regionColumn = ColumnDefinition{};
                    regionColumn.Width(GridLengthHelper::FromValueAndType(state->Weights[index], GridUnitType::Star));
                    allocation.ColumnDefinitions().Append(regionColumn);
                    allocationColumns->emplace_back(regionColumn);
                    auto region = Border{};
                    region.Background(SolidColorBrush{ Color{ 42, 128, 128, 128 } });
                    region.CornerRadius(CornerRadiusHelper::FromUniformRadius(2));
                    auto label = TextBlock{};
                    const auto name = state->Commands[index].Name.empty() ?
                                          to_hstring(static_cast<int>(index + 1)) + L" 号窗口" :
                                          state->Commands[index].Name;
                    label.Text(name + L"  " + to_hstring(static_cast<int>(state->Weights[index] * 100.0 + 0.5)) + L"%");
                    label.TextTrimming(TextTrimming::CharacterEllipsis);
                    label.HorizontalAlignment(HorizontalAlignment::Center);
                    label.VerticalAlignment(VerticalAlignment::Center);
                    Grid::SetColumn(region, static_cast<int>(index * 2));
                    Grid::SetRow(region, 1);
                    allocation.Children().Append(region);
                    Grid::SetColumn(label, static_cast<int>(index * 2));
                    Grid::SetRow(label, 0);
                    allocation.Children().Append(label);
                    allocationLabels->emplace_back(label);
                    if (index + 1 < state->Commands.size())
                    {
                        auto dividerColumn = ColumnDefinition{};
                        dividerColumn.Width(GridLengthHelper::FromPixels(18));
                        allocation.ColumnDefinitions().Append(dividerColumn);
                        struct AllocationDragState
                        {
                            bool Active{};
                            double StartX{};
                            double StartLeft{};
                            double Combined{};
                        };
                        const auto drag = std::make_shared<AllocationDragState>();
                        // A single shared allocation rail with an ordinary
                        // splitter grip for each boundary.
                        auto divider = Grid{};
                        divider.Width(18);
                        divider.Height(30);
                        divider.Background(SolidColorBrush{ Colors::Transparent() });
                        divider.HorizontalAlignment(HorizontalAlignment::Center);
                        divider.VerticalAlignment(VerticalAlignment::Center);
                        ToolTipService::SetToolTip(divider, box_value(L"拖动分隔条；两侧百分比将实时更新"));
                        auto grip = StackPanel{};
                        grip.Orientation(Orientation::Horizontal);
                        grip.HorizontalAlignment(HorizontalAlignment::Center);
                        grip.VerticalAlignment(VerticalAlignment::Center);
                        grip.Spacing(2);
                        for (int dotIndex = 0; dotIndex < 3; ++dotIndex)
                        {
                            auto dot = Border{};
                            dot.Width(2);
                            dot.Height(14);
                            dot.CornerRadius(CornerRadiusHelper::FromUniformRadius(1));
                            dot.Background(SolidColorBrush{ Color{ 255, 138, 138, 138 } });
                            grip.Children().Append(dot);
                        }
                        divider.Children().Append(grip);
                        Grid::SetColumn(divider, static_cast<int>(index * 2 + 1));
                        Grid::SetRow(divider, 0);
                        Grid::SetRowSpan(divider, 2);
                        const auto weakAllocation = make_weak(allocation);
                        divider.PointerPressed([weakAllocation, divider, state, drag, index](auto&&, const auto& args) {
                            args.Handled(true);
                            if (const auto allocation = weakAllocation.get())
                            {
                                drag->Active = true;
                                drag->StartX = args.GetCurrentPoint(allocation).Position().X;
                                drag->StartLeft = state->Weights[index];
                                drag->Combined = state->Weights[index] + state->Weights[index + 1];
                                divider.CapturePointer(args.Pointer());
                            }
                        });
                        divider.PointerMoved([weakAllocation, allocationColumns, allocationLabels, state, drag, index, snapSplitWeight](auto&&, const auto& args) {
                            args.Handled(true);
                            const auto allocation = weakAllocation.get();
                            if (!drag->Active || !allocation)
                            {
                                return;
                            }
                            {
                                const auto dividerWidth = 18.0 * static_cast<double>(state->Commands.size() - 1);
                                const auto availableWidth = std::max(1.0, allocation.ActualWidth() - dividerWidth);
                                const auto minimum = std::min(0.15, drag->Combined / 2.0);
                                const auto left = snapSplitWeight(drag->StartLeft + (args.GetCurrentPoint(allocation).Position().X - drag->StartX) / availableWidth,
                                                                  minimum,
                                                                  drag->Combined);
                                state->Weights[index] = left;
                                state->Weights[index + 1] = drag->Combined - left;
                                for (size_t weightIndex = 0; weightIndex < state->Weights.size(); ++weightIndex)
                                {
                                    allocationColumns->at(weightIndex).Width(GridLengthHelper::FromValueAndType(state->Weights[weightIndex], GridUnitType::Star));
                                    const auto name = state->Commands[weightIndex].Name.empty() ?
                                                          to_hstring(static_cast<int>(weightIndex + 1)) + L" 号窗口" :
                                                          state->Commands[weightIndex].Name;
                                    allocationLabels->at(weightIndex).Text(name + L"  " + to_hstring(static_cast<int>(state->Weights[weightIndex] * 100.0 + 0.5)) + L"%");
                                }
                            }
                        });
                        divider.PointerReleased([divider, drag](auto&&, const auto& args) {
                            args.Handled(true);
                            drag->Active = false;
                            divider.ReleasePointerCaptures();
                        });
                        divider.PointerCaptureLost([drag](auto&&, auto&&) {
                            drag->Active = false;
                        });
                        allocation.Children().Append(divider);
                    }
                }
                settings.Children().Append(makeWorkspaceSetting(L"大小分配", allocation));
            }
            if (!state->Split)
            {
                auto placement = ComboBox{};
                applyWorkspaceStyle(placement, L"WorkspaceComboBoxSettingStyle");
                for (const auto text : { L"左上（图标 + 文字）", L"右上（图标）", L"右下（图标）" })
                {
                    auto item = ComboBoxItem{};
                    item.Content(box_value(text));
                    placement.Items().Append(item);
                }
                placement.SelectedIndex(state->TabPlacement);
                placement.SelectionChanged([state, rebuild](auto&& sender, auto&&) {
                    state->TabPlacement = sender.as<ComboBox>().SelectedIndex();
                    (*rebuild)();
                });
                settings.Children().Append(makeWorkspaceSetting(L"Tab 位置", placement));
            }
            }

            auto preview = Grid{};
            preview.Background(SolidColorBrush{ Color{ 255, 18, 18, 18 } });
            preview.BorderThickness(ThicknessHelper::FromLengths(1, 1, 1, 1));
            preview.BorderBrush(SolidColorBrush{ Colors::DimGray() });
            Grid::SetColumn(preview, 1);
            page.Children().Append(preview);
            if (state->WebViewHostComparison)
            {
                const std::array<hstring, 3> urls{
                    L"https://www.qq.com",
                    L"http://127.0.0.1:8080/?folder=/home/coder/project",
                    L"http://localhost:18080/"
                };
                auto layout = Grid{};
                layout.Padding(ThicknessHelper::FromLengths(8, 8, 8, 8));
                for (size_t index = 0; index < urls.size(); ++index)
                {
                    auto row = RowDefinition{};
                    row.Height(GridLengthHelper::FromValueAndType(1, GridUnitType::Star));
                    layout.RowDefinitions().Append(row);
                }
                auto heading = TextBlock{};
                heading.Text(state->WebViewHostMode == 0 ? L"XAML WebView2：三个现有地址" : L"原生 CompositionController：三个现有地址");
                heading.Margin(ThicknessHelper::FromLengths(10, 6, 10, 4));
                heading.FontSize(14);
                heading.HorizontalAlignment(HorizontalAlignment::Left);
                heading.VerticalAlignment(VerticalAlignment::Top);
                layout.Children().Append(heading);

                for (size_t index = 0; index < urls.size(); ++index)
                {
                    auto tile = Grid{};
                    tile.Margin(ThicknessHelper::FromLengths(0, 22, 0, 3));
                    tile.BorderBrush(SolidColorBrush{ Color{ 255, 80, 80, 80 } });
                    tile.BorderThickness(ThicknessHelper::FromLengths(1, 1, 1, 1));
                    Grid::SetRow(tile, static_cast<int>(index));
                    auto label = TextBlock{};
                    label.Text(to_hstring(static_cast<int>(index + 1)) + L". " + urls[index]);
                    label.Margin(ThicknessHelper::FromLengths(8, 3, 8, 3));
                    label.FontSize(12);
                    label.VerticalAlignment(VerticalAlignment::Top);
                    tile.Children().Append(label);
                    auto surface = Grid{};
                    surface.Margin(ThicknessHelper::FromLengths(0, 25, 0, 0));
                    tile.Children().Append(surface);
                    if (state->WebViewHostMode == 0)
                    {
                        auto webView = Microsoft::UI::Xaml::Controls::WebView2{};
                        webView.HorizontalAlignment(HorizontalAlignment::Stretch);
                        webView.VerticalAlignment(VerticalAlignment::Stretch);
                        webView.CoreWebView2Initialized([webView, url = urls[index]](auto&&, const auto& args) {
                            Json::Value payload{ Json::objectValue };
                            payload["initializationHresult"] = Json::Int{ args.Exception() };
                            terminal::workspacechat::AddDiagnosticTextFields(payload, "url", url.c_str());
                            std::ignore = terminal::workspacechat::AppendWorkspaceDiagnosticLog(L"workspace_webview_demo_xaml_initialized", payload);
                            if (SUCCEEDED(args.Exception()))
                            {
                                webView.CoreWebView2().Navigate(url);
                            }
                        });
                        webView.Loaded([webView, url = urls[index]](auto&&, auto&&) -> winrt::fire_and_forget {
                            try
                            {
                                co_await webView.EnsureCoreWebView2Async();
                            }
                            catch (const winrt::hresult_error& ex)
                            {
                                Json::Value payload{ Json::objectValue };
                                terminal::workspacechat::AddDiagnosticTextFields(payload, "url", url.c_str());
                                terminal::workspacechat::AppendExceptionDiagnostic(payload, ex);
                                std::ignore = terminal::workspacechat::AppendWorkspaceDiagnosticLog(L"workspace_webview_demo_xaml_ensure_exception", payload);
                            }
                        });
                        webView.NavigationCompleted([url = urls[index]](auto&&, const auto& args) {
                            Json::Value payload{ Json::objectValue };
                            payload["success"] = args.IsSuccess();
                            payload["webErrorStatus"] = static_cast<int>(args.WebErrorStatus());
                            terminal::workspacechat::AddDiagnosticTextFields(payload, "url", url.c_str());
                            std::ignore = terminal::workspacechat::AppendWorkspaceDiagnosticLog(L"workspace_webview_demo_xaml_navigation_completed", payload);
                        });
                        surface.Children().Append(webView);
                    }
                    else
                    {
                        const auto nativeWebView = std::make_shared<NativeWebViewDemoHost>(surface, _hostingHwnd.value_or(nullptr), urls[index]);
                        state->NativeWebViews.emplace_back(nativeWebView);
                        nativeWebView->Initialize();
                    }
                    layout.Children().Append(tile);
                }
                preview.Children().Append(layout);
                host.Children().Append(page);
                return;
            }
            const auto makeTerminal = [state](const size_t index) {
                auto terminal = Border{};
                terminal.Margin(ThicknessHelper::FromLengths(6, state->Commands.size() == 1 ? 6 : 34, 6, 6));
                terminal.Padding(ThicknessHelper::FromLengths(14, 12, 14, 12));
                terminal.Background(SolidColorBrush{ Color{ 255, 27, 31, 35 } });
                auto body = StackPanel{};
                auto label = TextBlock{};
                label.Text(state->Commands[index].Title);
                label.FontSize(16);
                body.Children().Append(label);
                auto command = TextBlock{};
                command.Text(state->Commands[index].Command.empty() ? L"（空 command：仅 Demo 展示）" : L"> " + state->Commands[index].Command);
                command.Margin(ThicknessHelper::FromLengths(0, 12, 0, 0));
                command.Opacity(0.72);
                body.Children().Append(command);
                terminal.Child(body);
                return terminal;
            };
            if (state->Split)
            {
                std::vector<ColumnDefinition> terminalColumns;
                terminalColumns.reserve(state->Weights.size());
                for (size_t index = 0; index < state->Weights.size(); ++index)
                {
                    auto column = ColumnDefinition{};
                    column.Width(GridLengthHelper::FromValueAndType(state->Weights[index], GridUnitType::Star));
                    preview.ColumnDefinitions().Append(column);
                    terminalColumns.emplace_back(column);
                    if (index + 1 < state->Weights.size())
                    {
                        auto dividerColumn = ColumnDefinition{};
                        dividerColumn.Width(GridLengthHelper::FromPixels(12));
                        preview.ColumnDefinitions().Append(dividerColumn);
                    }
                }
                for (size_t index = 0; index < state->Commands.size(); ++index)
                {
                    const auto terminal = makeTerminal(index);
                    Grid::SetColumn(terminal, static_cast<int>(index * 2));
                    preview.Children().Append(terminal);
                }
                // There is one actual draggable divider between every adjacent
                // pair. A three-window demo therefore always has two dividers.
                for (size_t index = 0; index + 1 < state->Commands.size(); ++index)
                {
                    struct DragState
                    {
                        bool Active{};
                        double StartX{};
                        double StartLeft{};
                        double Combined{};
                        double LastX{};
                        double LastY{};
                    };
                    const auto drag = std::make_shared<DragState>();
                    // The stock Pane has no mouse splitter. This Host demo
                    // therefore owns a small, explicit pointer drag surface.
                    auto splitter = Grid{};
                    splitter.Background(SolidColorBrush{ Colors::Transparent() });
                    auto divider = Border{};
                    divider.Background(SolidColorBrush{ Color{ 255, 92, 92, 92 } });
                    divider.Margin(ThicknessHelper::FromLengths(4, 6, 4, 6));
                    splitter.Children().Append(divider);
                    ToolTipService::SetToolTip(splitter, box_value(L"拖动以调整相邻窗口比例"));
                    Grid::SetColumn(splitter, static_cast<int>(index * 2 + 1));

                    auto ratioBubble = Border{};
                    ratioBubble.Background(SolidColorBrush{ Color{ 235, 45, 45, 45 } });
                    ratioBubble.CornerRadius(CornerRadiusHelper::FromUniformRadius(4));
                    ratioBubble.Padding(ThicknessHelper::FromLengths(8, 4, 8, 4));
                    ratioBubble.HorizontalAlignment(HorizontalAlignment::Left);
                    ratioBubble.VerticalAlignment(VerticalAlignment::Top);
                    ratioBubble.Margin(ThicknessHelper::FromLengths(8, 8, 0, 0));
                    ratioBubble.Visibility(Visibility::Collapsed);
                    auto ratioIndicator = StackPanel{};
                    ratioIndicator.Orientation(Orientation::Horizontal);
                    ratioIndicator.Spacing(5);
                    ratioIndicator.VerticalAlignment(VerticalAlignment::Center);
                    auto dragIcon = SymbolIcon{};
                    dragIcon.Symbol(Symbol::Switch);
                    dragIcon.Width(14);
                    dragIcon.Height(14);
                    ratioIndicator.Children().Append(dragIcon);
                    auto ratioText = TextBlock{};
                    ratioText.FontSize(12);
                    ratioText.Text(to_hstring(static_cast<int>(state->Weights[index] * 100.0 + 0.5)) + L"% | " + to_hstring(static_cast<int>(state->Weights[index + 1] * 100.0 + 0.5)) + L"%");
                    ratioIndicator.Children().Append(ratioText);
                    ratioBubble.Child(ratioIndicator);
                    Grid::SetColumn(ratioBubble, 0);
                    Grid::SetColumnSpan(ratioBubble, static_cast<int>(state->Commands.size() * 2 - 1));

                    const auto holdTimer = DispatcherTimer{};
                    holdTimer.Interval(Windows::Foundation::TimeSpan{ 3'500'000 });
                    holdTimer.Tick([holdTimer, preview, ratioBubble, drag, positionRatioBubble](auto&&, auto&&) {
                        holdTimer.Stop();
                        if (drag->Active)
                        {
                            positionRatioBubble(ratioBubble, preview, drag->LastX, drag->LastY);
                            ratioBubble.Visibility(Visibility::Visible);
                        }
                    });
                    splitter.PointerPressed([preview, splitter, ratioBubble, holdTimer, state, drag, index](auto&&, const auto& args) {
                        drag->Active = true;
                        drag->StartX = args.GetCurrentPoint(preview).Position().X;
                        drag->LastX = args.GetCurrentPoint(preview).Position().X;
                        drag->LastY = args.GetCurrentPoint(preview).Position().Y;
                        drag->StartLeft = state->Weights[index];
                        drag->Combined = state->Weights[index] + state->Weights[index + 1];
                        splitter.CapturePointer(args.Pointer());
                        holdTimer.Start();
                    });
                    splitter.PointerMoved([preview, ratioBubble, ratioText, state, terminalColumns, drag, index, snapSplitWeight, positionRatioBubble](auto&&, const auto& args) {
                        if (!drag->Active)
                        {
                            return;
                        }
                        const auto point = args.GetCurrentPoint(preview).Position();
                        drag->LastX = point.X;
                        drag->LastY = point.Y;
                        if (ratioBubble.Visibility() == Visibility::Visible)
                        {
                            positionRatioBubble(ratioBubble, preview, drag->LastX, drag->LastY);
                        }
                        const auto dividerWidth = 12.0 * static_cast<double>(state->Commands.size() - 1);
                        const auto availableWidth = std::max(1.0, preview.ActualWidth() - dividerWidth);
                        const auto minimum = std::min(0.15, drag->Combined / 2.0);
                        const auto left = snapSplitWeight(drag->StartLeft + (point.X - drag->StartX) / availableWidth,
                                                          minimum,
                                                          drag->Combined);
                        state->Weights[index] = left;
                        state->Weights[index + 1] = drag->Combined - left;
                        terminalColumns[index].Width(GridLengthHelper::FromValueAndType(state->Weights[index], GridUnitType::Star));
                        terminalColumns[index + 1].Width(GridLengthHelper::FromValueAndType(state->Weights[index + 1], GridUnitType::Star));
                        ratioText.Text(to_hstring(static_cast<int>(state->Weights[index] * 100.0 + 0.5)) + L"% | " + to_hstring(static_cast<int>(state->Weights[index + 1] * 100.0 + 0.5)) + L"%");
                    });
                    splitter.PointerReleased([splitter, ratioBubble, holdTimer, drag](auto&&, const auto&) {
                        drag->Active = false;
                        splitter.ReleasePointerCaptures();
                        holdTimer.Stop();
                        ratioBubble.Visibility(Visibility::Collapsed);
                    });
                    splitter.PointerCaptureLost([ratioBubble, holdTimer, drag](auto&&, auto&&) {
                        drag->Active = false;
                        holdTimer.Stop();
                        ratioBubble.Visibility(Visibility::Collapsed);
                    });
                    preview.Children().Append(splitter);
                    preview.Children().Append(ratioBubble);
                }
            }
            else
            {
                preview.Children().Append(makeTerminal(state->Active));
                auto tabs = StackPanel{};
                tabs.Orientation(state->TabPlacement == 0 ? Orientation::Horizontal : Orientation::Vertical);
                tabs.Spacing(4);
                tabs.HorizontalAlignment(state->TabPlacement == 0 ? HorizontalAlignment::Left : HorizontalAlignment::Right);
                tabs.VerticalAlignment(state->TabPlacement == 2 ? VerticalAlignment::Bottom : VerticalAlignment::Top);
                tabs.Margin(ThicknessHelper::FromLengths(8, 6, 8, 6));
                for (size_t index = 0; index < state->Commands.size(); ++index)
                {
                    auto tab = Button{};
                    auto tabContent = StackPanel{};
                    tabContent.Orientation(state->TabPlacement == 0 ? Orientation::Horizontal : Orientation::Vertical);
                    tabContent.Spacing(state->TabPlacement == 0 ? 6 : 0);
                    tabContent.HorizontalAlignment(HorizontalAlignment::Center);
                    tabContent.VerticalAlignment(VerticalAlignment::Center);
                    if (auto icon = _CreateNewTabFlyoutIcon(state->Commands[index].Icon))
                    {
                        if (const auto element = icon.try_as<FrameworkElement>())
                        {
                            element.Width(20);
                            element.Height(20);
                        }
                        tabContent.Children().Append(icon);
                    }
                    else
                    {
                        auto fallback = SymbolIcon{};
                        fallback.Symbol(Symbol::Page);
                        tabContent.Children().Append(fallback);
                    }
                    if (state->TabPlacement == 0)
                    {
                        auto text = TextBlock{};
                        text.Text(state->Commands[index].Name.empty() ? state->Commands[index].Title : state->Commands[index].Name);
                        text.TextTrimming(TextTrimming::CharacterEllipsis);
                        tabContent.Children().Append(text);
                    }
                    tab.Content(tabContent);
                    tab.Opacity(index == state->Active ? 1.0 : 0.5);
                    ToolTipService::SetToolTip(tab, box_value(state->Commands[index].Name.empty() ? state->Commands[index].Title : state->Commands[index].Name));
                    tab.Click([state, rebuild, index](auto&&, auto&&) { state->Active = index; (*rebuild)(); });
                    tabs.Children().Append(tab);
                }
                preview.Children().Append(tabs);
            }
            host.Children().Append(page);
        };
        (*rebuild)();
        return host;
    }

}
