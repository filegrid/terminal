// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "TerminalPage.h"

using namespace winrt;
using namespace winrt::Windows::UI;
using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Windows::UI::Xaml::Controls;
using namespace winrt::Windows::UI::Xaml::Controls::Primitives;
using namespace winrt::Windows::UI::Xaml::Media;

namespace winrt::TerminalApp::implementation
{
    UIElement TerminalPage::_BuildWorkspaceMultiWindowDemo()
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
        };

        const auto state = std::make_shared<DemoState>();
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
            addCommand.IsEnabled(state->Commands.size() < 3);
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
