#include "../../../../microsoft/src/cascadia/WinRTUtils/inc/Utils.h"
#include "../chat/WorkspaceDiagnosticLog.h"

    UIElement TerminalPage::_BuildWorkspaceManagerContent()
    {
        {
            Json::Value payload{ Json::objectValue };
            payload["workspaceCount"] = gsl::narrow<Json::ArrayIndex>(_workspaceEditorManager.Workspaces().size());
            payload["navSelection"] = _workspaceManagerNavSelection;
            payload["selectedWorkspaceIndex"] = _workspaceEditorSelectedIndex;
            payload["editMode"] = _workspaceEditorEditMode;
            payload["definitionsDirty"] = _workspaceDefinitionsDirty;
            std::ignore = terminal::workspacechat::AppendWorkspaceDiagnosticLog(L"workspace_manager_build_begin", payload);
        }
        const auto marginBottom = [](const double bottom) {
            return WUX::ThicknessHelper::FromLengths(0, 0, 0, bottom);
        };
        // Use the workspace-owned copy of the settings-editor presentation.
        // It deliberately lives outside the original Settings page so the
        // workspace manager does not modify or depend on that page's tree.
        auto workspaceResources = ResourceDictionary{};
        workspaceResources.Source(winrt::Windows::Foundation::Uri{ L"ms-appx:///TerminalApp/WorkspaceSettingsResources.xaml" });
        const auto applyWorkspaceStyle = [&workspaceResources](const auto& control, const wchar_t* key) {
            const auto resourceKey = box_value(key);
            if (workspaceResources.HasKey(resourceKey))
            {
                if (const auto style = workspaceResources.Lookup(resourceKey).try_as<winrt::Windows::UI::Xaml::Style>())
                {
                    control.Style(style);
                }
            }
        };
        // The workspace manager is a standalone tab. Settings-editor styles
        // can depend on resources that are absent here, so do not apply them
        // dynamically during page construction.
        const auto applyOptionalStyle = [](const auto&, const wchar_t*) {};
        const auto makeSectionTitle = [&](const winrt::hstring& text) {
            auto title = TextBlock{};
            title.Text(text);
            applyWorkspaceStyle(title, L"WorkspaceSectionHeaderStyle");
            return title;
        };
        struct WorkspaceIconFamilyOption
        {
            const wchar_t* key;
            const wchar_t* label;
        };
        struct WorkspaceIconSectionOption
        {
            const wchar_t* key;
            const wchar_t* label;
            uint32_t count;
        };
        static constexpr std::array<WorkspaceIconFamilyOption, 4> workspaceIconFamilies{ {
            { L"color", L"彩色" },
            { L"outline", L"描边" },
            { L"duotone", L"双色" },
            { L"sharp", L"硬朗" },
        } };
        static constexpr std::array<WorkspaceIconSectionOption, 5> workspaceIconSections{ {
            { L"numbers", L"数字 0-9", 10 },
            { L"letters", L"字母 A-Z", 26 },
            { L"daily", L"日常", 20 },
            { L"development", L"研发", 20 },
            { L"office", L"办公", 20 },
        } };
        static constexpr WorkspaceIconSectionOption workspaceWindowsSection{ L"windows", L"Windows / OS", 20 };
        const auto makeWorkspaceIconDescriptor = [](const std::wstring& family, const std::wstring& section, const uint32_t index) {
            return std::wstring{ L"workspace-icon://" } + family + L"/" + section + L"/" + std::to_wstring(index);
        };
        const auto tryParseWorkspaceIconDescriptor = [](const std::wstring& iconValue, std::wstring& family, std::wstring& section, uint32_t& index) -> bool {
            static constexpr std::wstring_view prefix{ L"workspace-icon://" };
            const std::wstring_view path{ iconValue };
            if (!path.starts_with(prefix))
            {
                return false;
            }

            const auto payload = path.substr(prefix.size());
            const auto slash1 = payload.find(L'/');
            const auto slash2 = payload.find(L'/', slash1 == std::wstring_view::npos ? slash1 : slash1 + 1);
            if (slash1 == std::wstring_view::npos || slash2 == std::wstring_view::npos)
            {
                return false;
            }

            family.assign(payload.substr(0, slash1));
            section.assign(payload.substr(slash1 + 1, slash2 - slash1 - 1));
            const auto parsedIndex = til::parse_unsigned<uint32_t>(payload.substr(slash2 + 1));
            if (!parsedIndex.has_value())
            {
                return false;
            }
            index = *parsedIndex;
            return true;
        };
        const auto makeWorkspaceSetting = [&](const winrt::hstring& labelText, const UIElement& content) {
            auto setting = ContentControl{};
            setting.Tag(box_value(labelText));
            if (const auto toggle = content.try_as<WUX::Controls::ToggleSwitch>())
            {
                toggle.HorizontalAlignment(HorizontalAlignment::Right);
                toggle.Loaded([this, toggle, labelText](auto&&, auto&&) {
                    const auto position = toggle.TransformToVisual(this->Root()).TransformPoint({ 0, 0 });
                    const auto togglePosition = toggle.TransformToVisual(this->Root()).TransformPoint({ 0, 0 });
                    const auto message = fmt::format(FMT_COMPILE(L"[WorkspaceLayout] {} toggleX={:.1f} toggleWidth={:.1f} toggleRight={:.1f}\n"),
                                                     labelText.c_str(),
                                                     position.X,
                                                     toggle.ActualWidth(),
                                                     position.X + toggle.ActualWidth());
                    OutputDebugStringW(message.c_str());
                    Json::Value payload{ Json::objectValue };
                    terminal::workspacechat::AddDiagnosticTextFields(payload, "label", labelText.c_str());
                    payload["toggleRootX"] = position.X;
                    payload["toggleWidth"] = toggle.ActualWidth();
                    payload["toggleRootRight"] = position.X + toggle.ActualWidth();
                    std::ignore = terminal::workspacechat::AppendWorkspaceDiagnosticLog(L"workspace_layout", payload);
                });
                setting.Content(toggle);
            }
            else
            {
                const auto contentElement = content.as<FrameworkElement>();
                contentElement.HorizontalAlignment(HorizontalAlignment::Right);
                const auto weakContent = make_weak(contentElement);
                contentElement.Loaded([this, weakContent, labelText](auto&&, auto&&) {
                    if (const auto loadedContent = weakContent.get())
                    {
                        const auto contentPosition = loadedContent.TransformToVisual(this->Root()).TransformPoint({ 0, 0 });
                        Json::Value payload{ Json::objectValue };
                        terminal::workspacechat::AddDiagnosticTextFields(payload, "label", labelText.c_str());
                        payload["controlRootX"] = contentPosition.X;
                        payload["controlRootRight"] = contentPosition.X + loadedContent.ActualWidth();
                        payload["controlWidth"] = loadedContent.ActualWidth();
                        std::ignore = terminal::workspacechat::AppendWorkspaceDiagnosticLog(L"workspace_control_layout", payload);
                    }
                });
                setting.Content(content);
            }
            applyWorkspaceStyle(setting, L"WorkspaceSettingContainerStyle");
            return setting;
        };
        auto nav = MUX::Controls::NavigationView{};
        nav.Background(SolidColorBrush{ Colors::Transparent() });
        nav.IsBackButtonVisible(MUX::Controls::NavigationViewBackButtonVisible::Collapsed);
        nav.IsPaneToggleButtonVisible(false);
        nav.IsSettingsVisible(false);
        nav.PaneDisplayMode(MUX::Controls::NavigationViewPaneDisplayMode::Left);
        nav.OpenPaneLength(320);
        nav.AlwaysShowHeader(false);

        const auto& workspaces = _workspaceEditorManager.Workspaces();
        if (_workspaceManagerNavSelection == 0 && !workspaces.empty())
        {
            _workspaceManagerNavSelection = Microsoft::Terminal::Settings::Model::implementation::WorkspaceManagerNavSelectionForWorkspace(0);
            Json::Value payload{ Json::objectValue };
            payload["workspaceCount"] = gsl::narrow<Json::ArrayIndex>(workspaces.size());
            payload["navSelection"] = _workspaceManagerNavSelection;
            std::ignore = terminal::workspacechat::AppendWorkspaceDiagnosticLog(L"workspace_manager_nav_defaulted", payload);
        }
        if (_workspaceManagerNavSelection >= 1000)
        {
            const auto workspaceIndex = Microsoft::Terminal::Settings::Model::implementation::ResolveWorkspaceIndexFromManagerNavSelection(_workspaceManagerNavSelection);
            if (workspaces.empty() || !workspaceIndex.has_value() || *workspaceIndex >= workspaces.size())
            {
                _workspaceManagerNavSelection = Microsoft::Terminal::Settings::Model::implementation::ResolveWorkspaceManagerNavSelectionForEditor(workspaces.size(), _workspaceEditorSelectedIndex);
                Json::Value payload{ Json::objectValue };
                payload["workspaceCount"] = gsl::narrow<Json::ArrayIndex>(workspaces.size());
                payload["selectedWorkspaceIndex"] = _workspaceEditorSelectedIndex;
                payload["navSelection"] = _workspaceManagerNavSelection;
                std::ignore = terminal::workspacechat::AppendWorkspaceDiagnosticLog(L"workspace_manager_nav_recovered", payload);
            }
        }

        nav.MenuItems().Append(MUX::Controls::NavigationViewItemHeader{});
        if (const auto headerItem = nav.MenuItems().GetAt(nav.MenuItems().Size() - 1).try_as<MUX::Controls::NavigationViewItemHeader>())
        {
            auto label = TextBlock{};
            label.Text(RS_(L"WorkspaceEditor_WorkspaceLabel"));
            label.VerticalAlignment(VerticalAlignment::Center);
            headerItem.Content(label);
        }

        std::vector<MUX::Controls::NavigationViewItem> workspaceGeneralItems;
        workspaceGeneralItems.reserve(workspaces.size());
        std::vector<std::vector<MUX::Controls::NavigationViewItem>> workspaceNodeItems;
        workspaceNodeItems.reserve(workspaces.size());
        for (uint32_t index = 0; index < workspaces.size(); ++index)
        {
            const auto& workspace = workspaces[index];
            auto item = MUX::Controls::NavigationViewItem{};
            item.Content(box_value(_WorkspaceDisplayName(workspace)));
            item.SelectsOnInvoked(false);
            item.IsExpanded(_workspaceManagerNavSelection >= 1000 &&
                             Microsoft::Terminal::Settings::Model::implementation::ResolveWorkspaceIndexFromManagerNavSelection(_workspaceManagerNavSelection) == index);
            item.Tapped([item](auto&&, auto&&) {
                item.IsExpanded(!item.IsExpanded());
            });

            WUX::Controls::IconElement workspaceNavIcon{ nullptr };
            if (!workspace.Icon.empty())
            {
                workspaceNavIcon = _CreateNewTabFlyoutIcon(winrt::hstring{ workspace.Icon });
            }
            if (workspaceNavIcon)
            {
                if (const auto frameworkElement = workspaceNavIcon.try_as<FrameworkElement>())
                {
                    frameworkElement.Width(16);
                    frameworkElement.Height(16);
                    frameworkElement.HorizontalAlignment(HorizontalAlignment::Center);
                    frameworkElement.VerticalAlignment(VerticalAlignment::Center);
                }
                item.Icon(workspaceNavIcon);
            }
            else
            {
                WUX::Controls::SymbolIcon icon{};
                icon.Symbol(WUX::Controls::Symbol::OpenFile);
                item.Icon(icon);
            }

            auto generalItem = MUX::Controls::NavigationViewItem{};
            auto generalContent = Grid{};
            generalContent.ColumnDefinitions().Append(ColumnDefinition{});
            auto nodeAddColumn = ColumnDefinition{};
            nodeAddColumn.Width(GridLengthHelper::Auto());
            generalContent.ColumnDefinitions().Append(nodeAddColumn);
            auto generalLabel = TextBlock{};
            generalLabel.Text(RS_(L"WorkspaceEditor_GeneralNav"));
            generalLabel.VerticalAlignment(VerticalAlignment::Center);
            generalContent.Children().Append(generalLabel);
            auto addNodeButton = Button{};
            addNodeButton.Content(box_value(L"+"));
            addNodeButton.MinWidth(28);
            addNodeButton.Padding(WUX::ThicknessHelper::FromLengths(4, 0, 4, 0));
            Controls::Grid::SetColumn(addNodeButton, 1);
            auto addNodeFlyout = MenuFlyout{};
            auto addBlankNodeItem = MenuFlyoutItem{};
            addBlankNodeItem.Text(L"空白节点");
            addBlankNodeItem.Click([weakThis{ get_weak() }, index](auto&&, auto&&) {
                if (auto self{ weakThis.get() })
                {
                    self->_SetSelectedWorkspaceIndex(index);
                    self->_AddWorkspaceNode();
                    const auto& nodes = self->_workspaceExtension->WorkspaceEditorManager().Workspaces().at(index).Nodes;
                    self->_workspaceExtension->WorkspaceManagerNavSelection() = Microsoft::Terminal::Settings::Model::implementation::WorkspaceManagerNavSelectionForWorkspaceNode(index, nodes.size() - 1);
                    self->_RebuildWorkspaceManagerTab();
                }
            });
            addNodeFlyout.Items().Append(addBlankNodeItem);
            if (!workspace.Nodes.empty())
            {
                addNodeFlyout.Items().Append(MenuFlyoutSeparator{});
                for (uint32_t templateIndex = 0; templateIndex < workspace.Nodes.size(); ++templateIndex)
                {
                    const auto& templateNode = workspace.Nodes.at(templateIndex);
                    auto templateItem = MenuFlyoutItem{};
                    templateItem.Text(winrt::hstring{ L"来自节点模板：" + (templateNode.Name.empty() ? templateNode.Id : templateNode.Name) });
                    templateItem.Click([weakThis{ get_weak() }, index, templateIndex](auto&&, auto&&) {
                        if (auto self{ weakThis.get() })
                        {
                            self->_SetSelectedWorkspaceIndex(index);
                            self->_AddWorkspaceNode();
                            auto& nodes = self->_workspaceExtension->WorkspaceEditorManager().Workspaces().at(index).Nodes;
                            if (nodes.size() < 2 || templateIndex >= nodes.size() - 1)
                            {
                                return;
                            }
                            const auto newId = nodes.back().Id;
                            const auto generatedName = nodes.back().Name;
                            nodes.back() = nodes.at(templateIndex);
                            nodes.back().Id = newId;
                            nodes.back().Name = generatedName;
                            EnsureWorkspaceNodeTabColors(self->_workspaceExtension->WorkspaceEditorManager().Workspaces().at(index), self->_settings);
                            self->_workspaceExtension->WorkspaceDefinitionsDirty() = true;
                            self->_workspaceExtension->WorkspaceManagerNavSelection() = Microsoft::Terminal::Settings::Model::implementation::WorkspaceManagerNavSelectionForWorkspaceNode(index, nodes.size() - 1);
                            self->_RebuildWorkspaceManagerTab();
                        }
                    });
                    addNodeFlyout.Items().Append(templateItem);
                }
            }
            addNodeButton.Click([addNodeFlyout, addNodeButton](auto&&, auto&&) {
                addNodeFlyout.ShowAt(addNodeButton);
            });
            generalContent.Children().Append(addNodeButton);
            generalItem.Content(generalContent);
            generalItem.Tag(box_value(Microsoft::Terminal::Settings::Model::implementation::WorkspaceManagerNavSelectionForWorkspace(index)));
            {
                WUX::Controls::SymbolIcon childIcon{};
                childIcon.Symbol(WUX::Controls::Symbol::Bullets);
                generalItem.Icon(childIcon);
            }
            item.MenuItems().Append(generalItem);
            workspaceGeneralItems.emplace_back(generalItem);

            std::vector<MUX::Controls::NavigationViewItem> nodeItems;
            nodeItems.reserve(workspace.Nodes.size());
            for (uint32_t nodeIndex = 0; nodeIndex < workspace.Nodes.size(); ++nodeIndex)
            {
                const auto& node = workspace.Nodes[nodeIndex];
                auto nodeItem = MUX::Controls::NavigationViewItem{};
                auto nodeContent = Grid{};
                nodeContent.ColumnDefinitions().Append(ColumnDefinition{});
                auto moveColumn = ColumnDefinition{};
                moveColumn.Width(GridLengthHelper::Auto());
                nodeContent.ColumnDefinitions().Append(moveColumn);
                auto nodeLabel = TextBlock{};
                nodeLabel.Text(winrt::hstring{ node.Name.empty() ? node.Id : node.Name });
                nodeLabel.VerticalAlignment(VerticalAlignment::Center);
                nodeContent.Children().Append(nodeLabel);
                if (_workspaceEditorEditMode && node.ShowTab)
                {
                    auto moveButton = Button{};
                    moveButton.Content(box_value(L"↕"));
                    moveButton.MinWidth(28);
                    moveButton.Padding(WUX::ThicknessHelper::FromLengths(2, 0, 2, 0));
                    Controls::Grid::SetColumn(moveButton, 1);
                    auto moveFlyout = MenuFlyout{};
                    for (const auto [offset, text] : { std::pair<int, const wchar_t*>{ -1, L"上移" }, { 1, L"下移" } })
                    {
                        auto moveItem = MenuFlyoutItem{};
                        moveItem.Text(text);
                        moveItem.IsEnabled((offset < 0 && nodeIndex > 0) || (offset > 0 && nodeIndex + 1 < workspace.Nodes.size()));
                        moveItem.Click([weakThis{ get_weak() }, index, nodeIndex, offset](auto&&, auto&&) {
                            if (auto self{ weakThis.get() })
                            {
                                auto& nodes = self->_workspaceExtension->WorkspaceEditorManager().Workspaces().at(index).Nodes;
                                const auto target = static_cast<size_t>(static_cast<int>(nodeIndex) + offset);
                                if (target >= nodes.size() || !nodes.at(target).ShowTab)
                                {
                                    return;
                                }
                                std::swap(nodes.at(nodeIndex), nodes.at(target));
                                auto& workspace = self->_workspaceExtension->WorkspaceEditorManager().Workspaces().at(index);
                                workspace.TabOrder.clear();
                                for (const auto& candidate : workspace.Nodes)
                                {
                                    if (candidate.ShowTab)
                                    {
                                        workspace.TabOrder.emplace_back(candidate.Id);
                                    }
                                }
                                self->_workspaceExtension->WorkspaceDefinitionsDirty() = true;
                                self->_workspaceExtension->WorkspaceManagerNavSelection() = Microsoft::Terminal::Settings::Model::implementation::WorkspaceManagerNavSelectionForWorkspaceNode(index, target);
                                self->_RebuildWorkspaceManagerTab();
                            }
                        });
                        moveFlyout.Items().Append(moveItem);
                    }
                    moveButton.Click([moveFlyout, moveButton](auto&&, auto&&) {
                        moveFlyout.ShowAt(moveButton);
                    });
                    nodeContent.Children().Append(moveButton);
                }
                nodeItem.Content(nodeContent);
                nodeItem.Tag(box_value(Microsoft::Terminal::Settings::Model::implementation::WorkspaceManagerNavSelectionForWorkspaceNode(index, nodeIndex)));
                nodeItem.DoubleTapped([item](auto&&, auto&&) {
                    item.IsExpanded(!item.IsExpanded());
                });
                {
                // A node represents its source. Prefer that profile's icon;
                // retain a neutral fallback only for an unresolved source.
                WUX::Controls::IconElement nodeIcon{ nullptr };
                if (!node.Icon.empty())
                {
                    nodeIcon = _CreateNewTabFlyoutIcon(winrt::hstring{ node.Icon });
                }
                if (!nodeIcon)
                {
                    if (const auto guid = _tryParseGuid(node.ProfileGuid); guid.has_value())
                    {
                        if (const auto profile = _settings.FindProfile(*guid))
                        {
                            nodeIcon = _CreateNewTabFlyoutIcon(profile.Icon().Resolved());
                        }
                    }
                }
                if (nodeIcon)
                {
                    if (const auto frameworkElement = nodeIcon.try_as<FrameworkElement>())
                    {
                        frameworkElement.Width(16);
                        frameworkElement.Height(16);
                        frameworkElement.HorizontalAlignment(HorizontalAlignment::Center);
                        frameworkElement.VerticalAlignment(VerticalAlignment::Center);
                    }
                    nodeItem.Icon(nodeIcon);
                }
                else
                {
                    WUX::Controls::SymbolIcon childIcon{};
                    childIcon.Symbol(WUX::Controls::Symbol::Page);
                    nodeItem.Icon(childIcon);
                }
                }
                item.MenuItems().Append(nodeItem);
                nodeItems.emplace_back(nodeItem);
            }
            workspaceNodeItems.emplace_back(std::move(nodeItems));

            nav.MenuItems().Append(item);
        }

        // Keep these two management actions adjacent at the bottom of the nav.
        auto newWorkspaceItem = MUX::Controls::NavigationViewItem{};
        newWorkspaceItem.Content(box_value(L"新建工作区"));
        newWorkspaceItem.Tag(box_value(-2));
        newWorkspaceItem.SelectsOnInvoked(false);
        {
            WUX::Controls::SymbolIcon icon{};
            icon.Symbol(WUX::Controls::Symbol::Add);
            newWorkspaceItem.Icon(icon);
        }
        nav.FooterMenuItems().Append(newWorkspaceItem);

        auto openYamlItem = MUX::Controls::NavigationViewItem{};
        openYamlItem.Content(box_value(L"打开配置目录"));
        openYamlItem.Tag(box_value(-1));
        openYamlItem.SelectsOnInvoked(false);
        {
            WUX::Controls::SymbolIcon icon{};
            icon.Symbol(WUX::Controls::Symbol::Document);
            openYamlItem.Icon(icon);
        }
        nav.FooterMenuItems().Append(openYamlItem);

        if (_workspaceManagerNavSelection >= 1000)
        {
            const auto workspaceIndex = Microsoft::Terminal::Settings::Model::implementation::ResolveWorkspaceIndexFromManagerNavSelection(_workspaceManagerNavSelection).value_or(0);
            const auto selectedNodeIndex = Microsoft::Terminal::Settings::Model::implementation::ResolveWorkspaceNodeIndexFromManagerNavSelection(_workspaceManagerNavSelection);
            if (workspaceIndex < workspaceGeneralItems.size())
            {
                if (!selectedNodeIndex.has_value())
                {
                    nav.SelectedItem(workspaceGeneralItems[workspaceIndex]);
                }
                else
                {
                    const auto nodeIndex = *selectedNodeIndex;
                    const auto stableNodeIndex = std::make_shared<size_t>(nodeIndex);
                    if (workspaceIndex < workspaceNodeItems.size() && nodeIndex < workspaceNodeItems[workspaceIndex].size())
                    {
                        nav.SelectedItem(workspaceNodeItems[workspaceIndex][nodeIndex]);
                    }
                    else
                    {
                        nav.SelectedItem(workspaceGeneralItems[workspaceIndex]);
                    }
                }
            }
        }

        nav.ItemInvoked([weakThis{ get_weak() }](auto&&, const MUX::Controls::NavigationViewItemInvokedEventArgs& args) {
            if (auto self{ weakThis.get() })
            {
                if (const auto item = args.InvokedItemContainer().try_as<MUX::Controls::NavigationViewItem>())
                {
                    if (const auto tag = item.Tag())
                    {
                        const auto value = winrt::unbox_value<int32_t>(tag);
                        if (value == -2)
                        {
                            auto addWorkspaceFlyout = MenuFlyout{};
                            auto addBlankWorkspaceItem = MenuFlyoutItem{};
                            addBlankWorkspaceItem.Text(L"空白工作区");
                            addBlankWorkspaceItem.Click([weakThis](auto&&, auto&&) {
                                if (auto self{ weakThis.get() })
                                {
                                    self->_AddWorkspaceDefinition();
                                    const auto& workspaces = self->_workspaceExtension->WorkspaceEditorManager().Workspaces();
                                    self->_workspaceExtension->WorkspaceManagerNavSelection() = Microsoft::Terminal::Settings::Model::implementation::WorkspaceManagerNavSelectionForWorkspace(workspaces.size() - 1);
                                    self->_RebuildWorkspaceManagerTab();
                                }
                            });
                            addWorkspaceFlyout.Items().Append(addBlankWorkspaceItem);
                            const auto& workspaces = self->_workspaceExtension->WorkspaceEditorManager().Workspaces();
                            if (!workspaces.empty())
                            {
                                addWorkspaceFlyout.Items().Append(MenuFlyoutSeparator{});
                                for (uint32_t templateIndex = 0; templateIndex < workspaces.size(); ++templateIndex)
                                {
                                    const auto& templateWorkspace = workspaces.at(templateIndex);
                                    auto templateItem = MenuFlyoutItem{};
                                    templateItem.Text(winrt::hstring{ L"来自工作区模板：" + self->_WorkspaceDisplayName(templateWorkspace) });
                                    templateItem.Click([weakThis, templateIndex](auto&&, auto&&) {
                                        if (auto self{ weakThis.get() })
                                        {
                                            self->_AddWorkspaceDefinition(templateIndex);
                                            const auto& workspaces = self->_workspaceExtension->WorkspaceEditorManager().Workspaces();
                                            self->_workspaceExtension->WorkspaceManagerNavSelection() = Microsoft::Terminal::Settings::Model::implementation::WorkspaceManagerNavSelectionForWorkspace(workspaces.size() - 1);
                                            self->_RebuildWorkspaceManagerTab();
                                        }
                                    });
                                    addWorkspaceFlyout.Items().Append(templateItem);
                                }
                            }
                            addWorkspaceFlyout.ShowAt(item);
                        }
                        else if (value == -1)
                        {
                            const auto filePath = Microsoft::Terminal::Settings::Model::implementation::WorkspaceManager::DefaultPath().wstring();
                            const auto result = reinterpret_cast<uintptr_t>(ShellExecuteW(nullptr, L"open", filePath.c_str(), nullptr, nullptr, SW_SHOW));
                            if (result <= 32)
                            {
                                ShellExecuteW(nullptr, L"open", L"explorer.exe", filePath.c_str(), nullptr, SW_SHOW);
                            }
                        }
                    }
                    else if (item.MenuItems().Size() > 0)
                    {
                        item.IsExpanded(!item.IsExpanded());
                    }
                }
            }
        });

        nav.SelectionChanged([weakThis{ get_weak() }](auto&&, auto&& args) {
            if (auto self{ weakThis.get() })
            {
                if (args.IsSettingsSelected())
                {
                    return;
                }

                if (const auto item = args.SelectedItemContainer().try_as<MUX::Controls::NavigationViewItem>())
                {
                    const auto value = winrt::unbox_value<int32_t>(item.Tag());
                    self->_workspaceExtension->WorkspaceManagerNavSelection() = value;
                    if (const auto workspaceIndex = Microsoft::Terminal::Settings::Model::implementation::ResolveWorkspaceIndexFromManagerNavSelection(value))
                    {
                        self->_SetSelectedWorkspaceIndex(*workspaceIndex);
                    }
                    self->_RebuildWorkspaceManagerTab();
                }
            }
        });

        auto contentGrid = Grid{};
        contentGrid.RowDefinitions().Append(RowDefinition{});
        auto footerRow = RowDefinition{};
        footerRow.Height(GridLengthHelper::Auto());
        contentGrid.RowDefinitions().Append(footerRow);

        auto scrollViewer = ScrollViewer{};
        scrollViewer.VerticalScrollBarVisibility(ScrollBarVisibility::Auto);
        scrollViewer.HorizontalScrollBarVisibility(ScrollBarVisibility::Disabled);
        scrollViewer.HorizontalScrollMode(ScrollMode::Disabled);
        scrollViewer.HorizontalAlignment(HorizontalAlignment::Stretch);
        scrollViewer.VerticalAlignment(VerticalAlignment::Stretch);

        auto root = StackPanel{};
        root.HorizontalAlignment(HorizontalAlignment::Center);
        root.Margin(WUX::ThicknessHelper::FromLengths(16, 0, 16, 16));
        root.Resources().MergedDictionaries().Append(workspaceResources);
        applyWorkspaceStyle(root, L"WorkspaceSettingsStackStyle");
        scrollViewer.Content(root);
        contentGrid.Children().Append(scrollViewer);

        if (_workspaceManagerNavSelection >= 1000)
        {
            auto* workspace = _SelectedWorkspaceForEditing();
            const auto selectedNodeIndex = Microsoft::Terminal::Settings::Model::implementation::ResolveWorkspaceNodeIndexFromManagerNavSelection(_workspaceManagerNavSelection);
            if (workspace == nullptr)
            {
                auto empty = TextBlock{};
                empty.Text(RS_(L"WorkspaceEditor_NoneSaved"));
                empty.TextWrapping(TextWrapping::Wrap);
                root.Children().Append(empty);
            }
            else
            {
                const auto addLabeledTextBox = [&](StackPanel& panel, const wchar_t* labelText, const std::wstring& initialValue, const auto& onChanged, const bool readOnly, const bool multiline = false) {
                    auto textBox = TextBox{};
                    // Reuse the settings editor's control styling instead of a
                    // workspace-specific textbox treatment.
                    applyWorkspaceStyle(textBox, L"WorkspaceTextBoxSettingStyle");
                    textBox.Text(initialValue);
                    textBox.IsReadOnly(readOnly);
                    textBox.AcceptsReturn(multiline);
                    textBox.TextWrapping(multiline ? TextWrapping::Wrap : TextWrapping::NoWrap);
                    if (!readOnly)
                    {
                        textBox.TextChanged(onChanged);
                    }
                    panel.Children().Append(makeWorkspaceSetting(labelText, textBox));
                };

                if (!selectedNodeIndex.has_value())
                {
                    auto generalPanel = StackPanel{};

                    generalPanel.Children().Append(makeSectionTitle(RS_(L"WorkspaceEditor_GeneralSection")));
                    auto workspaceNamePanel = StackPanel{};
                    workspaceNamePanel.Orientation(Orientation::Horizontal);
                    workspaceNamePanel.Spacing(8);
                    auto workspaceNameBox = TextBox{};
                    applyWorkspaceStyle(workspaceNameBox, L"WorkspaceTextBoxSettingStyle");
                    workspaceNameBox.Text(workspace->Name);
                    workspaceNameBox.IsReadOnly(!_workspaceEditorEditMode);
                    if (_workspaceEditorEditMode)
                    {
                        workspaceNameBox.TextChanged([weakThis{ get_weak() }](auto&& sender, auto&&) {
                            if (auto self{ weakThis.get() })
                            {
                                if (auto* current = self->_SelectedWorkspaceForEditing())
                                {
                                    current->Name = sender.as<TextBox>().Text().c_str();
                                    self->_workspaceExtension->WorkspaceDefinitionsDirty() = true;
                                }
                            }
                        });
                    }
                    workspaceNamePanel.Children().Append(workspaceNameBox);
                    if (_workspaceEditorEditMode)
                    {
                        auto deleteWorkspaceButton = Button{};
                        auto deleteWorkspaceIcon = SymbolIcon{};
                        deleteWorkspaceIcon.Symbol(Symbol::Delete);
                        deleteWorkspaceButton.Content(deleteWorkspaceIcon);
                        deleteWorkspaceButton.VerticalAlignment(VerticalAlignment::Center);
                        ToolTipService::SetToolTip(deleteWorkspaceButton, box_value(RS_(L"WorkspaceEditor_DeleteWorkspaceButton")));
                        Automation::AutomationProperties::SetName(deleteWorkspaceButton, RS_(L"WorkspaceEditor_DeleteWorkspaceButton"));
                        deleteWorkspaceButton.Click([weakThis{ get_weak() }](auto&&, auto&&) {
                            [weakThis]() -> safe_void_coroutine {
                                if (auto self{ weakThis.get() })
                                {
                                    auto dialog = ContentDialog{};
                                    dialog.Title(box_value(L"删除工作区"));
                                    dialog.Content(box_value(L"确定要删除这个工作区吗？"));
                                    dialog.PrimaryButtonText(L"删除");
                                    dialog.CloseButtonText(L"取消");
                                    if (auto presenter{ self->_dialogPresenter.get() })
                                    {
                                        const auto result = co_await presenter.ShowDialog(dialog);
                                        if (auto strong{ weakThis.get() }; strong && result == ContentDialogResult::Primary)
                                        {
                                            strong->_DeleteSelectedWorkspaceDefinition();
                                            strong->_RebuildWorkspaceManagerTab();
                                        }
                                    }
                                }
                            }();
                        });
                        workspaceNamePanel.Children().Append(deleteWorkspaceButton);
                    }
                    generalPanel.Children().Append(makeWorkspaceSetting(RS_(L"WorkspaceEditor_WorkspaceName"), workspaceNamePanel));
                    addLabeledTextBox(generalPanel, RS_(L"WorkspaceEditor_Description").c_str(), workspace->Description, [weakThis{ get_weak() }](auto&& sender, auto&&) {
                        if (auto self{ weakThis.get() })
                        {
                            if (auto* current = self->_SelectedWorkspaceForEditing())
                            {
                                current->Description = sender.as<TextBox>().Text().c_str();
                                self->_workspaceExtension->WorkspaceDefinitionsDirty() = true;
                            }
                        }
                    }, !_workspaceEditorEditMode, true);

                    generalPanel.Children().Append(makeSectionTitle(L"节点顺序"));
                    auto reorderList = ListView{};
                    reorderList.CanDragItems(_workspaceEditorEditMode);
                    reorderList.CanReorderItems(_workspaceEditorEditMode);
                    reorderList.AllowDrop(_workspaceEditorEditMode);
                    reorderList.Margin(marginBottom(16));
                    uint32_t displayOrder = 0;
                    for (const auto& candidate : workspace->Nodes)
                    {
                        if (!candidate.ShowTab)
                        {
                            continue;
                        }
                        auto nodeRow = Grid{};
                        nodeRow.Tag(box_value(winrt::hstring{ candidate.Id }));
                        nodeRow.Padding(WUX::ThicknessHelper::FromLengths(16, 12, 16, 12));
                        auto numberColumn = ColumnDefinition{};
                        numberColumn.Width(GridLengthHelper::FromPixels(32));
                        auto iconColumn = ColumnDefinition{};
                        iconColumn.Width(GridLengthHelper::FromPixels(56));
                        nodeRow.ColumnDefinitions().Append(numberColumn);
                        nodeRow.ColumnDefinitions().Append(iconColumn);
                        nodeRow.ColumnDefinitions().Append(ColumnDefinition{});
                        auto number = TextBlock{};
                        number.Text(to_hstring(++displayOrder));
                        number.VerticalAlignment(VerticalAlignment::Center);
                        nodeRow.Children().Append(number);
                        IconElement icon{ nullptr };
                        if (!candidate.Icon.empty())
                        {
                            icon = _CreateNewTabFlyoutIcon(winrt::hstring{ candidate.Icon });
                        }
                        if (!icon)
                        {
                            if (const auto guid = _tryParseGuid(candidate.ProfileGuid); guid.has_value())
                            {
                                if (const auto profile = _settings.FindProfile(*guid))
                                {
                                    icon = _CreateNewTabFlyoutIcon(profile.Icon().Resolved());
                                }
                            }
                        }
                        if (!icon)
                        {
                            auto fallbackIcon = SymbolIcon{};
                            fallbackIcon.Symbol(Symbol::Page);
                            icon = fallbackIcon;
                        }
                        if (const auto frameworkElement = icon.try_as<FrameworkElement>())
                        {
                            frameworkElement.Width(24);
                            frameworkElement.Height(24);
                            frameworkElement.HorizontalAlignment(HorizontalAlignment::Center);
                            frameworkElement.VerticalAlignment(VerticalAlignment::Center);
                        }
                        icon.VerticalAlignment(VerticalAlignment::Center);
                        Grid::SetColumn(icon, 1);
                        nodeRow.Children().Append(icon);
                        auto name = TextBlock{};
                        name.Text(winrt::hstring{ candidate.Name.empty() ? candidate.Id : candidate.Name });
                        name.VerticalAlignment(VerticalAlignment::Center);
                        name.Margin(WUX::ThicknessHelper::FromLengths(4, 0, 0, 0));
                        Grid::SetColumn(name, 2);
                        nodeRow.Children().Append(name);
                        auto nodeItem = ListViewItem{};
                        applyWorkspaceStyle(nodeItem, L"WorkspaceNodeOrderItemStyle");
                        nodeItem.Content(nodeRow);
                        nodeItem.Tag(nodeRow.Tag());
                        reorderList.Items().Append(nodeItem);
                    }
                    if (_workspaceEditorEditMode)
                    {
                        reorderList.DragItemsCompleted([weakThis{ get_weak() }, reorderList](auto&&, auto&&) {
                            if (auto self{ weakThis.get() })
                            {
                                auto* current = self->_SelectedWorkspaceForEditing();
                                if (!current)
                                {
                                    return;
                                }
                                std::vector<std::wstring> order;
                                order.reserve(reorderList.Items().Size());
                                for (uint32_t itemIndex = 0; itemIndex < reorderList.Items().Size(); ++itemIndex)
                                {
                                    if (const auto element = reorderList.Items().GetAt(itemIndex).try_as<FrameworkElement>())
                                    {
                                        order.emplace_back(winrt::unbox_value<winrt::hstring>(element.Tag()).c_str());
                                    }
                                }
                                if (order.empty())
                                {
                                    return;
                                }
                                current->TabOrder = order;
                                std::vector<Microsoft::Terminal::Settings::Model::implementation::WorkspaceNode> orderedNodes;
                                orderedNodes.reserve(current->Nodes.size());
                                for (const auto& id : order)
                                {
                                    const auto it = std::find_if(current->Nodes.begin(), current->Nodes.end(), [&](const auto& node) { return node.Id == id; });
                                    if (it != current->Nodes.end())
                                    {
                                        orderedNodes.emplace_back(*it);
                                    }
                                }
                                for (const auto& node : current->Nodes)
                                {
                                    if (!node.ShowTab)
                                    {
                                        orderedNodes.emplace_back(node);
                                    }
                                }
                                current->Nodes = std::move(orderedNodes);
                                self->_workspaceExtension->WorkspaceDefinitionsDirty() = true;
                            }
                        });
                    }
                    generalPanel.Children().Append(reorderList);

                    generalPanel.Children().Append(makeSectionTitle(L"节点默认值"));
                    auto defaultProfilePicker = ComboBox{};
                    applyWorkspaceStyle(defaultProfilePicker, L"WorkspaceComboBoxSettingStyle");
                    defaultProfilePicker.IsEnabled(_workspaceEditorEditMode);
                    int32_t selectedDefaultProfileIndex = -1;
                    const auto defaultProfiles = _settings.ActiveProfiles();
                    for (uint32_t profileIndex = 0; profileIndex < defaultProfiles.Size(); ++profileIndex)
                    {
                        const auto profile = defaultProfiles.GetAt(profileIndex);
                        auto profileItem = ComboBoxItem{};
                        const auto guidText = Utils::GuidToString(profile.Guid());
                        const auto displayName = profile.Name().empty() ? profile.Source() : profile.Name();
                        profileItem.Content(box_value(displayName.empty() ? winrt::hstring{ guidText } : displayName));
                        profileItem.Tag(box_value(guidText));
                        defaultProfilePicker.Items().Append(profileItem);
                        if (!workspace->NewNodeDefaults.ProfileGuid.empty() && _wcsicmp(guidText.c_str(), workspace->NewNodeDefaults.ProfileGuid.c_str()) == 0)
                        {
                            selectedDefaultProfileIndex = gsl::narrow_cast<int32_t>(profileIndex);
                        }
                    }
                    defaultProfilePicker.SelectedIndex(selectedDefaultProfileIndex);
                    if (_workspaceEditorEditMode)
                    {
                        defaultProfilePicker.SelectionChanged([weakThis{ get_weak() }](auto&& sender, auto&&) {
                            if (auto self{ weakThis.get() })
                            {
                                if (auto* current = self->_SelectedWorkspaceForEditing())
                                {
                                    if (const auto item = sender.as<ComboBox>().SelectedItem().try_as<ComboBoxItem>())
                                    {
                                        current->NewNodeDefaults.ProfileGuid = winrt::unbox_value<winrt::hstring>(item.Tag()).c_str();
                                        current->NewNodeDefaults.ProfileName = winrt::unbox_value_or<winrt::hstring>(item.Content(), {}).c_str();
                                        self->_workspaceExtension->WorkspaceDefinitionsDirty() = true;
                                    }
                                }
                            }
                        });
                    }
                    generalPanel.Children().Append(makeWorkspaceSetting(L"来源", defaultProfilePicker));
                    addLabeledTextBox(generalPanel, L"启动目录", workspace->NewNodeDefaults.StartupDirectory, [weakThis{ get_weak() }](auto&& sender, auto&&) {
                        if (auto self{ weakThis.get() })
                        {
                            if (auto* current = self->_SelectedWorkspaceForEditing())
                            {
                                current->NewNodeDefaults.StartupDirectory = sender.as<TextBox>().Text().c_str();
                                self->_workspaceExtension->WorkspaceDefinitionsDirty() = true;
                            }
                        }
                    }, !_workspaceEditorEditMode);
                    auto addDefaultToggle = [&](const wchar_t* label, const bool isOn, const auto& onToggled) {
                        auto toggle = WUX::Controls::ToggleSwitch{};
                        applyWorkspaceStyle(toggle, L"WorkspaceToggleSwitchStyle");
                        toggle.Header(nullptr);
                        toggle.IsOn(isOn);
                        toggle.IsEnabled(_workspaceEditorEditMode);
                        toggle.HorizontalAlignment(HorizontalAlignment::Right);
                        if (_workspaceEditorEditMode)
                        {
                            toggle.Toggled(onToggled);
                        }
                        generalPanel.Children().Append(makeWorkspaceSetting(label, toggle));
                    };
                    addDefaultToggle(L"显示输入框", workspace->NewNodeDefaults.ShowInputPanel, [weakThis{ get_weak() }](auto&& sender, auto&&) {
                        if (auto self{ weakThis.get() })
                        {
                            if (auto* current = self->_SelectedWorkspaceForEditing())
                            {
                                current->NewNodeDefaults.ShowInputPanel = sender.as<WUX::Controls::ToggleSwitch>().IsOn();
                                self->_workspaceExtension->WorkspaceDefinitionsDirty() = true;
                            }
                        }
                    });
                    addDefaultToggle(L"固定标题", workspace->NewNodeDefaults.UseNodeNameAsTabTitle, [weakThis{ get_weak() }](auto&& sender, auto&&) {
                        if (auto self{ weakThis.get() })
                        {
                            if (auto* current = self->_SelectedWorkspaceForEditing())
                            {
                                current->NewNodeDefaults.UseNodeNameAsTabTitle = sender.as<WUX::Controls::ToggleSwitch>().IsOn();
                                self->_workspaceExtension->WorkspaceDefinitionsDirty() = true;
                            }
                        }
                    });
                    auto defaultShowTab = WUX::Controls::ToggleSwitch{};
                    applyWorkspaceStyle(defaultShowTab, L"WorkspaceToggleSwitchStyle");
                    defaultShowTab.Header(nullptr);
                    defaultShowTab.IsOn(workspace->NewNodeDefaults.ShowTab);
                    defaultShowTab.IsEnabled(_workspaceEditorEditMode);
                    if (_workspaceEditorEditMode)
                    {
                        defaultShowTab.Toggled([weakThis{ get_weak() }](auto&& sender, auto&&) {
                            if (auto self{ weakThis.get() })
                            {
                                if (auto* current = self->_SelectedWorkspaceForEditing())
                                {
                                    current->NewNodeDefaults.ShowTab = sender.as<WUX::Controls::ToggleSwitch>().IsOn();
                                    self->_workspaceExtension->WorkspaceDefinitionsDirty() = true;
                                }
                            }
                        });
                    }
                    defaultShowTab.HorizontalAlignment(HorizontalAlignment::Right);
                    generalPanel.Children().Append(makeWorkspaceSetting(L"显示此标签页", defaultShowTab));

                    auto colorPanel = StackPanel{};
                    colorPanel.Orientation(Orientation::Horizontal);
                    colorPanel.Spacing(8);

                    auto colorPreviewRow = StackPanel{};
                    colorPreviewRow.Orientation(Orientation::Horizontal);
                    colorPreviewRow.Spacing(8);

                    auto colorPreview = Border{};
                    colorPreview.Width(32);
                    colorPreview.Height(24);
                    colorPreview.CornerRadius(CornerRadiusHelper::FromUniformRadius(4));
                    colorPreview.BorderBrush(SolidColorBrush{ Colors::DarkGray() });
                    colorPreview.BorderThickness(WUX::ThicknessHelper::FromLengths(1, 1, 1, 1));

                    auto colorValue = TextBlock{};
                    colorValue.VerticalAlignment(VerticalAlignment::Center);

                    const auto applyWorkspaceColorPreview = [colorPreview, colorValue](const std::wstring& colorValueText) {
                        if (const auto parsedColor = _parseWorkspaceColor(colorValueText))
                        {
                            colorPreview.Background(SolidColorBrush{ *parsedColor });
                            colorValue.Text(winrt::hstring{ _normalizeWorkspaceColor(colorValueText) });
                        }
                        else
                        {
                            colorPreview.Background(SolidColorBrush{ Colors::Transparent() });
                            colorValue.Text(winrt::hstring{});
                        }
                    };

                    applyWorkspaceColorPreview(workspace->BackgroundColor);
                    colorPreviewRow.Children().Append(colorPreview);
                    colorPreviewRow.Children().Append(colorValue);
                    colorPanel.Children().Append(colorPreviewRow);

                    if (_workspaceEditorEditMode)
                    {
                        auto chooseColorButton = Button{};
                        auto chooseColorIcon = SymbolIcon{};
                        chooseColorIcon.Symbol(Symbol::Refresh);
                        chooseColorButton.Content(chooseColorIcon);
                        ToolTipService::SetToolTip(chooseColorButton, box_value(L"换一个"));
                        Automation::AutomationProperties::SetName(chooseColorButton, L"换一个");

                        auto backgroundColorFlyout = winrt::make<ColorPickupFlyout>();
                        if (const auto parsedColor = _parseWorkspaceColor(workspace->BackgroundColor))
                        {
                            backgroundColorFlyout.Color(*parsedColor);
                        }
                        backgroundColorFlyout.ColorSelected([weakThis{ get_weak() }, applyWorkspaceColorPreview](const winrt::Windows::UI::Color& color) {
                            if (auto self{ weakThis.get() })
                            {
                                if (auto* current = self->_SelectedWorkspaceForEditing())
                                {
                                    current->BackgroundColor = _workspaceColorToString(color);
                                    self->_workspaceExtension->WorkspaceDefinitionsDirty() = true;
                                    applyWorkspaceColorPreview(current->BackgroundColor);
                                }
                            }
                        });

                        chooseColorButton.Click([weakThis{ get_weak() }, applyWorkspaceColorPreview](auto&&, auto&&) {
                            if (auto self{ weakThis.get() })
                            {
                                if (auto* current = self->_SelectedWorkspaceForEditing())
                                {
                                    static constexpr std::array<std::wstring_view, 12> palette{
                                        L"#C50F1F", L"#0063B1", L"#0F7B0F", L"#CA5010",
                                        L"#8E562E", L"#744DA9", L"#038387", L"#881798",
                                        L"#498205", L"#515C6B", L"#567C73", L"#7A7574",
                                    };
                                    std::unordered_set<std::wstring> usedColors;
                                    for (const auto& other : self->_workspaceExtension->WorkspaceEditorManager().Workspaces())
                                    {
                                        if (&other != current && !other.BackgroundColor.empty())
                                        {
                                            usedColors.emplace(other.BackgroundColor);
                                        }
                                    }
                                    const auto replacement = std::find_if(palette.begin(), palette.end(), [&](const auto candidate) {
                                        return candidate != std::wstring_view{ current->BackgroundColor } && !usedColors.contains(std::wstring{ candidate });
                                    });
                                    current->BackgroundColor = replacement != palette.end() ? std::wstring{ *replacement } : std::wstring{};
                                    self->_workspaceExtension->WorkspaceDefinitionsDirty() = true;
                                    applyWorkspaceColorPreview(current->BackgroundColor);
                                }
                            }
                        });
                        colorPreviewRow.Tapped([backgroundColorFlyout, colorPreviewRow](auto&&, auto&&) {
                            backgroundColorFlyout.ShowAt(colorPreviewRow);
                        });

                        colorPanel.Children().Append(chooseColorButton);
                    }

                    auto workspaceIconPanel = StackPanel{};
                    workspaceIconPanel.Orientation(Orientation::Horizontal);
                    workspaceIconPanel.Spacing(8);

                    auto workspaceIconPreview = ContentControl{};
                    workspaceIconPreview.Width(36);
                    workspaceIconPreview.Height(36);
                    workspaceIconPreview.HorizontalAlignment(HorizontalAlignment::Center);
                    workspaceIconPreview.VerticalAlignment(VerticalAlignment::Center);
                    workspaceIconPreview.Margin(WUX::ThicknessHelper::FromLengths(3, 1, 0, 0));

                    const auto refreshWorkspaceIconPreview = [weakThis{ get_weak() }, workspaceIconPreview]() {
                        if (auto self{ weakThis.get() })
                        {
                            if (const auto* current = self->_SelectedWorkspaceForEditing())
                            {
                                WUX::Controls::IconElement previewIcon{ nullptr };
                                if (!current->Icon.empty())
                                {
                                    previewIcon = self->_CreateNewTabFlyoutIcon(winrt::hstring{ current->Icon });
                                }
                                if (!previewIcon)
                                {
                                    auto fallback = SymbolIcon{};
                                    fallback.Symbol(Symbol::OpenFile);
                                    previewIcon = fallback;
                                }
                                if (previewIcon)
                                {
                                    if (const auto frameworkElement = previewIcon.try_as<FrameworkElement>())
                                    {
                                        frameworkElement.Width(32);
                                        frameworkElement.Height(32);
                                        frameworkElement.HorizontalAlignment(HorizontalAlignment::Center);
                                        frameworkElement.VerticalAlignment(VerticalAlignment::Center);
                                    }
                                }
                                workspaceIconPreview.Content(previewIcon);
                            }
                        }
                    };
                    refreshWorkspaceIconPreview();

                    auto workspaceIconButton = Button{};
                    workspaceIconButton.Width(44);
                    workspaceIconButton.Height(44);
                    workspaceIconButton.MinWidth(44);
                    workspaceIconButton.MinHeight(44);
                    workspaceIconButton.Padding(WUX::ThicknessHelper::FromLengths(0, 0, 0, 0));
                    workspaceIconButton.HorizontalContentAlignment(HorizontalAlignment::Center);
                    workspaceIconButton.VerticalContentAlignment(VerticalAlignment::Center);
                    workspaceIconButton.Content(workspaceIconPreview);
                    ToolTipService::SetToolTip(workspaceIconButton, box_value(L"选择图标"));
                    Automation::AutomationProperties::SetName(workspaceIconButton, L"选择图标");
                    workspaceIconPanel.Children().Append(workspaceIconButton);

                    if (_workspaceEditorEditMode)
                    {
                        workspaceIconButton.Click([weakThis{ get_weak() }, refreshWorkspaceIconPreview](auto&&, auto&&) {
                            [weakThis, refreshWorkspaceIconPreview]() -> safe_void_coroutine {
                                if (auto self{ weakThis.get() })
                                {
                                    try
                                    {
                                        auto dialog = ContentDialog{};
                                        dialog.PrimaryButtonText(L"");
                                        dialog.CloseButtonText(L"");
                                        dialog.FullSizeDesired(false);

                                        auto selectedIcon = std::make_shared<std::wstring>();
                                        auto currentFamily = std::make_shared<std::wstring>(L"color");
                                        if (const auto* current = self->_SelectedWorkspaceForEditing())
                                        {
                                            *selectedIcon = current->Icon;
                                            static constexpr std::wstring_view prefix{ L"workspace-icon://" };
                                            const std::wstring_view path{ current->Icon };
                                            if (path.starts_with(prefix))
                                            {
                                                const auto payload = path.substr(prefix.size());
                                                const auto slash = payload.find(L'/');
                                                if (slash != std::wstring_view::npos)
                                                {
                                                    *currentFamily = std::wstring{ payload.substr(0, slash) };
                                                }
                                            }
                                        }

                                        auto rootPanel = StackPanel{};
                                        rootPanel.Spacing(4);
                                        rootPanel.Width(412);
                                        rootPanel.MinWidth(412);
                                        rootPanel.Height(544);
                                        rootPanel.MinHeight(544);
                                        rootPanel.MaxWidth(412);
                                        rootPanel.MaxHeight(544);

                                        auto titlePanel = Grid{};
                                        auto titleTextColumn = ColumnDefinition{};
                                        titleTextColumn.Width(GridLength{ 1.0, GridUnitType::Star });
                                        auto titleCloseColumn = ColumnDefinition{};
                                        titleCloseColumn.Width(GridLengthHelper::Auto());
                                        titlePanel.ColumnDefinitions().Append(titleTextColumn);
                                        titlePanel.ColumnDefinitions().Append(titleCloseColumn);
                                        titlePanel.MinWidth(412);

                                        auto titleBlock = TextBlock{};
                                        titleBlock.Text(L"选择图标");
                                        titleBlock.VerticalAlignment(VerticalAlignment::Center);
                                        if (const auto titleStyle = Application::Current().Resources().Lookup(box_value(L"SubtitleTextBlockStyle")).try_as<winrt::Windows::UI::Xaml::Style>())
                                        {
                                            titleBlock.Style(titleStyle);
                                        }
                                        Controls::Grid::SetColumn(titleBlock, 0);
                                        titlePanel.Children().Append(titleBlock);

                                        auto titleCloseButton = Button{};
                                        titleCloseButton.Width(32);
                                        titleCloseButton.Height(32);
                                        titleCloseButton.Padding(WUX::ThicknessHelper::FromLengths(0, 0, 0, 0));
                                        titleCloseButton.HorizontalAlignment(HorizontalAlignment::Right);
                                        titleCloseButton.VerticalAlignment(VerticalAlignment::Center);
                                        auto titleCloseGlyph = TextBlock{};
                                        titleCloseGlyph.FontFamily(Media::FontFamily{ L"Segoe Fluent Icons, Segoe MDL2 Assets" });
                                        titleCloseGlyph.FontSize(12);
                                        titleCloseGlyph.HorizontalAlignment(HorizontalAlignment::Center);
                                        titleCloseGlyph.VerticalAlignment(VerticalAlignment::Center);
                                        titleCloseGlyph.Text(L"\xE8BB");
                                        titleCloseButton.Content(titleCloseGlyph);
                                        titleCloseButton.Click([dialog](auto&&, auto&&) {
                                            dialog.Hide();
                                        });
                                        Controls::Grid::SetColumn(titleCloseButton, 1);
                                        titlePanel.Children().Append(titleCloseButton);
                                        dialog.Title(titlePanel);

                                        auto headerPanel = Grid{};
                                        auto familyColumn = ColumnDefinition{};
                                        familyColumn.Width(GridLengthHelper::Auto());
                                        auto spacerColumn = ColumnDefinition{};
                                        spacerColumn.Width(GridLength{ 1.0, GridUnitType::Star });
                                        auto chooseFileColumn = ColumnDefinition{};
                                        chooseFileColumn.Width(GridLengthHelper::Auto());
                                        headerPanel.ColumnDefinitions().Append(familyColumn);
                                        headerPanel.ColumnDefinitions().Append(spacerColumn);
                                        headerPanel.ColumnDefinitions().Append(chooseFileColumn);

                                        auto familyPanel = StackPanel{};
                                        familyPanel.Orientation(Orientation::Horizontal);
                                        familyPanel.Spacing(4);
                                        auto familyButtons = std::make_shared<std::vector<Button>>();
                                        const auto updateFamilyButtonsState = [familyButtons, currentFamily]() {
                                            for (const auto& button : *familyButtons)
                                            {
                                                const auto buttonFamily = winrt::unbox_value_or<winrt::hstring>(button.Tag(), L"");
                                                const auto isSelected = *currentFamily == buttonFamily.c_str();
                                                button.BorderThickness(WUX::ThicknessHelper::FromLengths(isSelected ? 2 : 1, isSelected ? 2 : 1, isSelected ? 2 : 1, isSelected ? 2 : 1));
                                                button.BorderBrush(SolidColorBrush{ isSelected ? Colors::DodgerBlue() : Colors::DimGray() });
                                            }
                                        };
                                        for (const auto& family : workspaceIconFamilies)
                                        {
                                            auto familyButton = Button{};
                                            familyButton.Content(box_value(family.label));
                                            familyButton.Tag(box_value(family.key));
                                            familyButton.Padding(WUX::ThicknessHelper::FromLengths(6, 3, 6, 3));
                                            familyButton.MinWidth(64);
                                            familyButton.MinHeight(28);
                                            familyButton.Click([currentFamily, updateFamilyButtonsState, familyKey = std::wstring{ family.key }](auto&&, auto&&) {
                                                *currentFamily = familyKey;
                                                updateFamilyButtonsState();
                                            });
                                            familyButtons->emplace_back(familyButton);
                                            familyPanel.Children().Append(familyButton);
                                        }
                                        updateFamilyButtonsState();
                                        Controls::Grid::SetColumn(familyPanel, 0);
                                        headerPanel.Children().Append(familyPanel);

                                        auto chooseFileButton = HyperlinkButton{};
                                        chooseFileButton.Content(box_value(L"选择本地文件"));
                                        chooseFileButton.Padding(WUX::ThicknessHelper::FromLengths(4, 2, 4, 2));
                                        chooseFileButton.Margin(WUX::ThicknessHelper::FromLengths(8, 0, 0, 0));
                                        Controls::Grid::SetColumn(chooseFileButton, 2);
                                        headerPanel.Children().Append(chooseFileButton);

                                        rootPanel.Children().Append(headerPanel);

                                        auto iconSectionsPanel = StackPanel{};
                                        iconSectionsPanel.Spacing(1);

                                        const auto rebuildIconSections = [weakThis, selectedIcon, currentFamily, iconSectionsPanel, dialog]() {
                                            if (auto self{ weakThis.get() })
                                            {
                                                iconSectionsPanel.Children().Clear();
                                                const auto appendSection = [self, dialog, &selectedIcon, &currentFamily, &iconSectionsPanel](const WorkspaceIconSectionOption& section) {
                                                    auto sectionPanel = StackPanel{};
                                                    sectionPanel.Spacing(1);
                                                    auto sectionRows = StackPanel{};
                                                    sectionRows.Spacing(1);
                                                    StackPanel currentRow{};
                                                    uint32_t iconsInCurrentRow = 0;
                                                    for (uint32_t iconIndex = 0; iconIndex < section.count; ++iconIndex)
                                                    {
                                                        if (!currentRow || iconsInCurrentRow == 0)
                                                        {
                                                            currentRow = StackPanel{};
                                                            currentRow.Orientation(Orientation::Horizontal);
                                                            currentRow.Spacing(1);
                                                            sectionRows.Children().Append(currentRow);
                                                        }
                                                        const auto descriptor = std::wstring{ L"workspace-icon://" } + *currentFamily + L"/" + section.key + L"/" + std::to_wstring(iconIndex);
                                                        auto button = Button{};
                                                        button.Width(40);
                                                        button.Height(40);
                                                        button.MinWidth(40);
                                                        button.MinHeight(40);
                                                        button.Padding(WUX::ThicknessHelper::FromLengths(0, 0, 0, 0));
                                                        button.HorizontalContentAlignment(HorizontalAlignment::Center);
                                                        button.VerticalContentAlignment(VerticalAlignment::Center);
                                                        const auto isSelected = *selectedIcon == descriptor;
                                                        button.BorderThickness(WUX::ThicknessHelper::FromLengths(isSelected ? 2 : 1, isSelected ? 2 : 1, isSelected ? 2 : 1, isSelected ? 2 : 1));
                                                        button.BorderBrush(SolidColorBrush{ isSelected ? Colors::DodgerBlue() : Colors::DimGray() });
                                                        auto preview = self->_CreateNewTabFlyoutIcon(winrt::hstring{ descriptor });
                                                        if (preview)
                                                        {
                                                            if (const auto frameworkElement = preview.try_as<FrameworkElement>())
                                                            {
                                                                frameworkElement.Width(32);
                                                                frameworkElement.Height(32);
                                                                frameworkElement.HorizontalAlignment(HorizontalAlignment::Center);
                                                                frameworkElement.VerticalAlignment(VerticalAlignment::Center);
                                                            }
                                                            button.Content(preview);
                                                        }
                                                        button.Click([dialog, selectedIcon, descriptor](auto&&, auto&&) {
                                                            *selectedIcon = descriptor;
                                                            dialog.Hide();
                                                        });
                                                        currentRow.Children().Append(button);
                                                        ++iconsInCurrentRow;
                                                        if (iconsInCurrentRow >= 10)
                                                        {
                                                            currentRow = nullptr;
                                                            iconsInCurrentRow = 0;
                                                        }
                                                    }
                                                    sectionPanel.Children().Append(sectionRows);
                                                    iconSectionsPanel.Children().Append(sectionPanel);
                                                };
                                                for (const auto& section : workspaceIconSections)
                                                {
                                                    appendSection(section);
                                                }
                                                appendSection(workspaceWindowsSection);
                                            }
                                        };

                                        for (const auto& familyButton : *familyButtons)
                                        {
                                            familyButton.Click([rebuildIconSections](auto&&, auto&&) {
                                                rebuildIconSections();
                                            });
                                        }
                                        chooseFileButton.Click([weakThis, dialog, selectedIcon](auto&&, auto&&) {
                                            [weakThis, dialog, selectedIcon]() -> safe_void_coroutine {
                                                if (auto self{ weakThis.get() })
                                                {
                                                    const auto selectedPath = co_await OpenImagePicker(self->_hostingHwnd.value_or(nullptr));
                                                    if (!selectedPath.empty())
                                                    {
                                                        *selectedIcon = selectedPath.c_str();
                                                        dialog.Hide();
                                                    }
                                                }
                                            }();
                                        });
                                        rebuildIconSections();
                                        rootPanel.Children().Append(iconSectionsPanel);
                                        dialog.Content(rootPanel);

                                        if (const auto presenter = self->_dialogPresenter.get())
                                        {
                                            std::ignore = co_await presenter.ShowDialog(dialog);
                                        }

                                        if (!selectedIcon->empty())
                                        {
                                            if (auto* current = self->_SelectedWorkspaceForEditing(); current && current->Icon != *selectedIcon)
                                            {
                                                current->Icon = *selectedIcon;
                                                self->_workspaceExtension->WorkspaceDefinitionsDirty() = true;
                                                self->_workspaceExtension->WorkspaceManagerNavSelection() = Microsoft::Terminal::Settings::Model::implementation::WorkspaceManagerNavSelectionForWorkspace(self->_workspaceExtension->WorkspaceEditorSelectedIndex());
                                                self->_RebuildWorkspaceManagerTab();
                                            }
                                        }
                                    }
                                    catch (...)
                                    {
                                        throw;
                                    }
                                }
                                co_return;
                            }();
                        });
                    }

                    // Keep workspace-level appearance with the other common fields.
                    // The following sections are deliberately ordered as: 常规、节点顺序、节点默认值。
                    generalPanel.Children().InsertAt(3, makeWorkspaceSetting(RS_(L"WorkspaceEditor_BackgroundColor"), colorPanel));
                    generalPanel.Children().InsertAt(4, makeWorkspaceSetting(L"图标", workspaceIconPanel));
                    root.Children().Append(generalPanel);
                }

                if (selectedNodeIndex.has_value())
                {
                    const auto nodeIndex = *selectedNodeIndex;
                    const auto stableNodeIndex = std::make_shared<size_t>(nodeIndex);
                    if (nodeIndex >= workspace->Nodes.size())
                    {
                        auto emptyNodes = TextBlock{};
                        emptyNodes.Text(RS_(L"WorkspaceEditor_NoNodes"));
                        emptyNodes.Margin(marginBottom(12));
                        root.Children().Append(emptyNodes);
                    }
                    else
                    {
                        const auto& node = workspace->Nodes.at(nodeIndex);
                        const auto profiles = _settings.ActiveProfiles();

                        auto nodeRoot = StackPanel{};

                        const auto addNodeTextBox = [&](const wchar_t* labelText, const std::wstring& initialValue, const auto& onChanged, const bool multiline = false) {
                            auto textBox = TextBox{};
                            applyWorkspaceStyle(textBox, L"WorkspaceTextBoxSettingStyle");
                            textBox.Text(initialValue);
                            textBox.IsReadOnly(!_workspaceEditorEditMode);
                            textBox.AcceptsReturn(multiline);
                            textBox.TextWrapping(multiline ? TextWrapping::Wrap : TextWrapping::NoWrap);
                            if (_workspaceEditorEditMode)
                            {
                                textBox.TextChanged(onChanged);
                            }
                            nodeRoot.Children().Append(makeWorkspaceSetting(labelText, textBox));
                        };

                        auto nodeNamePanel = StackPanel{};
                        nodeNamePanel.Orientation(Orientation::Horizontal);
                        nodeNamePanel.Spacing(8);
                        auto nodeNameBox = TextBox{};
                        applyWorkspaceStyle(nodeNameBox, L"WorkspaceTextBoxSettingStyle");
                        nodeNameBox.Text(node.Name);
                        nodeNameBox.IsReadOnly(!_workspaceEditorEditMode);
                        if (_workspaceEditorEditMode)
                        {
                            nodeNameBox.TextChanged([weakThis{ get_weak() }, nodeIndex](auto&& sender, auto&&) {
                                if (auto self{ weakThis.get() })
                                {
                                    if (auto* current = self->_SelectedWorkspaceForEditing(); current && nodeIndex < current->Nodes.size())
                                    {
                                        current->Nodes.at(nodeIndex).Name = sender.as<TextBox>().Text().c_str();
                                        self->_workspaceExtension->WorkspaceDefinitionsDirty() = true;
                                    }
                                }
                            });
                        }
                        nodeNamePanel.Children().Append(nodeNameBox);
                        if (_workspaceEditorEditMode)
                        {
                            auto deleteNodeButton = Button{};
                            auto deleteNodeIcon = SymbolIcon{};
                            deleteNodeIcon.Symbol(Symbol::Delete);
                            deleteNodeButton.Content(deleteNodeIcon);
                            deleteNodeButton.VerticalAlignment(VerticalAlignment::Center);
                            ToolTipService::SetToolTip(deleteNodeButton, box_value(RS_(L"WorkspaceEditor_DeleteNodeButton")));
                            Automation::AutomationProperties::SetName(deleteNodeButton, RS_(L"WorkspaceEditor_DeleteNodeButton"));
                            deleteNodeButton.Click([weakThis{ get_weak() }, nodeIndex](auto&&, auto&&) {
                                [weakThis, nodeIndex]() -> safe_void_coroutine {
                                    if (auto self{ weakThis.get() })
                                    {
                                        auto dialog = ContentDialog{};
                                        dialog.Title(box_value(L"删除节点"));
                                        dialog.Content(box_value(L"确定要删除这个节点吗？"));
                                        dialog.PrimaryButtonText(L"删除");
                                        dialog.CloseButtonText(L"取消");
                                        if (auto presenter{ self->_dialogPresenter.get() })
                                        {
                                            const auto result = co_await presenter.ShowDialog(dialog);
                                            if (auto strong{ weakThis.get() }; strong && result == ContentDialogResult::Primary)
                                            {
                                                strong->_DeleteWorkspaceNode(nodeIndex);
                                                strong->_RebuildWorkspaceManagerTab();
                                            }
                                        }
                                    }
                                }();
                            });
                            nodeNamePanel.Children().Append(deleteNodeButton);
                        }
                        nodeRoot.Children().Append(makeWorkspaceSetting(RS_(L"WorkspaceEditor_NodeName"), nodeNamePanel));

                        auto showTabToggle = WUX::Controls::ToggleSwitch{};
                        applyWorkspaceStyle(showTabToggle, L"WorkspaceToggleSwitchStyle");
                        showTabToggle.Header(nullptr);
                        showTabToggle.IsOn(node.ShowTab);
                        showTabToggle.IsEnabled(_workspaceEditorEditMode);
                        if (_workspaceEditorEditMode)
                        {
                            showTabToggle.Toggled([weakThis{ get_weak() }, nodeIndex](auto&& sender, auto&&) {
                                if (auto self{ weakThis.get() })
                                {
                                    if (auto* current = self->_SelectedWorkspaceForEditing(); current && nodeIndex < current->Nodes.size())
                                    {
                                        if (const auto toggle = sender.try_as<WUX::Controls::ToggleSwitch>())
                                        {
                                            current->Nodes.at(nodeIndex).ShowTab = toggle.IsOn();
                                            self->_workspaceExtension->WorkspaceDefinitionsDirty() = true;
                                        }
                                    }
                                }
                            });
                        }
                        // Append this after the color row so the primary fields stay
                        // in the same order as Terminal's settings pages.

                        auto profilePicker = ComboBox{};
                        applyWorkspaceStyle(profilePicker, L"WorkspaceComboBoxSettingStyle");
                        profilePicker.IsEnabled(_workspaceEditorEditMode);

                        int32_t selectedProfileIndex = -1;
                        for (uint32_t profileIndex = 0; profileIndex < profiles.Size(); ++profileIndex)
                        {
                            const auto profile = profiles.GetAt(profileIndex);
                            auto item = ComboBoxItem{};
                            const auto guidText = Utils::GuidToString(profile.Guid());
                            const auto displayName = profile.Name().empty() ? profile.Source() : profile.Name();
                            item.Content(box_value(displayName.empty() ? winrt::hstring{ guidText } : displayName));
                            item.Tag(box_value(guidText));
                            profilePicker.Items().Append(item);
                            if (!node.ProfileGuid.empty() && _wcsicmp(guidText.c_str(), node.ProfileGuid.c_str()) == 0)
                            {
                                selectedProfileIndex = gsl::narrow_cast<int32_t>(profileIndex);
                            }
                        }

                        if (selectedProfileIndex < 0 && !node.ProfileGuid.empty())
                        {
                            auto item = ComboBoxItem{};
                            if (const auto guid = _tryParseGuid(node.ProfileGuid); guid.has_value())
                            {
                                if (const auto profile = _settings.FindProfile(*guid))
                                {
                                    const auto displayName = profile.Name().empty() ? profile.Source() : profile.Name();
                                    item.Content(box_value(displayName.empty() ? winrt::hstring{ node.ProfileGuid } : displayName));
                                }
                                else
                                {
                                    item.Content(box_value(node.ProfileName.empty() ? winrt::hstring{ node.ProfileGuid } : winrt::hstring{ node.ProfileName }));
                                }
                            }
                            else
                            {
                                item.Content(box_value(node.ProfileName.empty() ? winrt::hstring{ node.ProfileGuid } : winrt::hstring{ node.ProfileName }));
                            }
                            item.Tag(box_value(node.ProfileGuid));
                            profilePicker.Items().Append(item);
                            selectedProfileIndex = gsl::narrow_cast<int32_t>(profilePicker.Items().Size() - 1);
                        }

                        profilePicker.SelectedIndex(selectedProfileIndex);
                        if (_workspaceEditorEditMode)
                        {
                            profilePicker.SelectionChanged([weakThis{ get_weak() }, stableNodeIndex](auto&& sender, auto&&) {
                                if (auto self{ weakThis.get() })
                                {
                                    const auto nodeIndex = *stableNodeIndex;
                                    if (auto* current = self->_SelectedWorkspaceForEditing(); current && nodeIndex < current->Nodes.size())
                                    {
                                        if (const auto picker = sender.try_as<ComboBox>())
                                        {
                                            if (const auto item = picker.SelectedItem().try_as<ComboBoxItem>())
                                            {
                                                current->Nodes.at(nodeIndex).ProfileGuid = winrt::unbox_value<winrt::hstring>(item.Tag()).c_str();
                                                current->Nodes.at(nodeIndex).ProfileName = winrt::unbox_value_or<winrt::hstring>(item.Content(), {}).c_str();
                                                self->_workspaceExtension->WorkspaceDefinitionsDirty() = true;
                                            }
                                        }
                                    }
                                }
                            });
                        }
                        applyWorkspaceStyle(profilePicker, L"WorkspaceComboBoxSettingStyle");
                        nodeRoot.Children().Append(makeWorkspaceSetting(RS_(L"WorkspaceEditor_Source"), profilePicker));

                        auto iconPanel = StackPanel{};
                        iconPanel.Orientation(Orientation::Horizontal);
                        iconPanel.Spacing(8);

                        auto iconPreview = ContentControl{};
                        iconPreview.Width(36);
                        iconPreview.Height(36);
                        iconPreview.HorizontalAlignment(HorizontalAlignment::Center);
                        iconPreview.VerticalAlignment(VerticalAlignment::Center);
                        iconPreview.Margin(WUX::ThicknessHelper::FromLengths(3, 1, 0, 0));
                        const auto refreshNodeIconPreview = [weakThis{ get_weak() }, stableNodeIndex, iconPreview]() {
                            if (auto self{ weakThis.get() })
                            {
                                const auto nodeIndex = *stableNodeIndex;
                                Json::Value refreshPayload{ Json::objectValue };
                                refreshPayload["nodeIndex"] = nodeIndex;
                                std::ignore = terminal::workspacechat::AppendWorkspaceDiagnosticLog(L"workspace_icon_preview_refresh_begin", refreshPayload);
                                if (const auto* current = self->_SelectedWorkspaceForEditing(); current && nodeIndex < current->Nodes.size())
                                {
                                    const auto& currentNode = current->Nodes.at(nodeIndex);
                                    WUX::Controls::IconElement previewIcon{ nullptr };
                                    if (!currentNode.Icon.empty())
                                    {
                                        terminal::workspacechat::AddDiagnosticTextFields(refreshPayload, "iconPath", currentNode.Icon);
                                        std::ignore = terminal::workspacechat::AppendWorkspaceDiagnosticLog(L"workspace_icon_preview_refresh_try_node_icon", refreshPayload);
                                        previewIcon = self->_CreateNewTabFlyoutIcon(winrt::hstring{ currentNode.Icon });
                                    }
                                    if (!previewIcon)
                                    {
                                        if (const auto guid = _tryParseGuid(currentNode.ProfileGuid); guid.has_value())
                                        {
                                            if (const auto profile = self->_settings.FindProfile(*guid))
                                            {
                                                const auto resolvedIcon = profile.Icon().Resolved();
                                                terminal::workspacechat::AddDiagnosticTextFields(refreshPayload, "profileIconPath", resolvedIcon);
                                                std::ignore = terminal::workspacechat::AppendWorkspaceDiagnosticLog(L"workspace_icon_preview_refresh_try_profile_icon", refreshPayload);
                                                previewIcon = self->_CreateNewTabFlyoutIcon(profile.Icon().Resolved());
                                            }
                                        }
                                    }
                                    if (!previewIcon)
                                    {
                                        auto fallback = SymbolIcon{};
                                        fallback.Symbol(Symbol::Page);
                                        previewIcon = fallback;
                                        std::ignore = terminal::workspacechat::AppendWorkspaceDiagnosticLog(L"workspace_icon_preview_refresh_using_fallback", refreshPayload);
                                    }
                                    if (previewIcon)
                                    {
                                        if (const auto frameworkElement = previewIcon.try_as<FrameworkElement>())
                                        {
                                            frameworkElement.Width(32);
                                            frameworkElement.Height(32);
                                            frameworkElement.HorizontalAlignment(HorizontalAlignment::Center);
                                            frameworkElement.VerticalAlignment(VerticalAlignment::Center);
                                        }
                                    }
                                    std::ignore = terminal::workspacechat::AppendWorkspaceDiagnosticLog(L"workspace_icon_preview_refresh_before_content", refreshPayload);
                                    iconPreview.Content(previewIcon);
                                    std::ignore = terminal::workspacechat::AppendWorkspaceDiagnosticLog(L"workspace_icon_preview_refresh_end", refreshPayload);
                                }
                            }
                        };
                        refreshNodeIconPreview();
                        auto iconButton = Button{};
                        iconButton.Width(44);
                        iconButton.Height(44);
                        iconButton.MinWidth(44);
                        iconButton.MinHeight(44);
                        iconButton.Padding(WUX::ThicknessHelper::FromLengths(0, 0, 0, 0));
                        iconButton.HorizontalContentAlignment(HorizontalAlignment::Center);
                        iconButton.VerticalContentAlignment(VerticalAlignment::Center);
                        iconButton.HorizontalAlignment(HorizontalAlignment::Left);
                        iconButton.VerticalAlignment(VerticalAlignment::Center);
                        iconButton.Content(iconPreview);
                        ToolTipService::SetToolTip(iconButton, box_value(L"选择图标"));
                        Automation::AutomationProperties::SetName(iconButton, L"选择图标");
                        iconPanel.Children().Append(iconButton);
                        if (_workspaceEditorEditMode)
                        {
                        iconButton.Click([weakThis{ get_weak() }, stableNodeIndex, refreshNodeIconPreview](auto&&, auto&&) {
                                [weakThis, stableNodeIndex, refreshNodeIconPreview]() -> safe_void_coroutine {
                                    if (auto self{ weakThis.get() })
                                    {
                                          const auto nodeIndex = *stableNodeIndex;
                                          Json::Value startPayload{ Json::objectValue };
                                          startPayload["nodeIndex"] = nodeIndex;
                                          std::ignore = terminal::workspacechat::AppendWorkspaceDiagnosticLog(L"workspace_icon_picker_click", startPayload);

                                          try
                                          {
                                          auto dialog = ContentDialog{};
                                          dialog.PrimaryButtonText(L"");
                                          dialog.CloseButtonText(L"");
                                          dialog.FullSizeDesired(false);
                                          auto selectedIcon = std::make_shared<std::wstring>();
                                          auto currentFamily = std::make_shared<std::wstring>(L"color");
                                          if (const auto* current = self->_SelectedWorkspaceForEditing(); current && nodeIndex < current->Nodes.size())
                                          {
                                              std::wstring iconValue = current->Nodes.at(nodeIndex).Icon;
                                              *selectedIcon = iconValue;
                                              static constexpr std::wstring_view prefix{ L"workspace-icon://" };
                                              const std::wstring_view path{ iconValue };
                                              if (path.starts_with(prefix))
                                              {
                                                  const auto payload = path.substr(prefix.size());
                                                  const auto slash = payload.find(L'/');
                                                  if (slash != std::wstring_view::npos)
                                                  {
                                                      *currentFamily = std::wstring{ payload.substr(0, slash) };
                                                  }
                                              }
                                          }

                                            auto rootPanel = StackPanel{};
                                          rootPanel.Spacing(4);
                                          rootPanel.Width(412);
                                          rootPanel.MinWidth(412);
                                          rootPanel.Height(544);
                                          rootPanel.MinHeight(544);
                                          rootPanel.MaxWidth(412);
                                          rootPanel.MaxHeight(544);
                                            auto titlePanel = Grid{};
                                          auto titleTextColumn = ColumnDefinition{};
                                          titleTextColumn.Width(GridLength{ 1.0, GridUnitType::Star });
                                          auto titleCloseColumn = ColumnDefinition{};
                                          titleCloseColumn.Width(GridLengthHelper::Auto());
                                          titlePanel.ColumnDefinitions().Append(titleTextColumn);
                                          titlePanel.ColumnDefinitions().Append(titleCloseColumn);
                                          titlePanel.MinWidth(412);

                                          auto titleBlock = TextBlock{};
                                          titleBlock.Text(L"选择图标");
                                          titleBlock.VerticalAlignment(VerticalAlignment::Center);
                                          if (const auto titleStyle = Application::Current().Resources().Lookup(box_value(L"SubtitleTextBlockStyle")).try_as<winrt::Windows::UI::Xaml::Style>())
                                          {
                                              titleBlock.Style(titleStyle);
                                          }
                                          Controls::Grid::SetColumn(titleBlock, 0);
                                          titlePanel.Children().Append(titleBlock);

                                          auto titleCloseButton = Button{};
                                          titleCloseButton.Width(32);
                                          titleCloseButton.Height(32);
                                          titleCloseButton.Padding(WUX::ThicknessHelper::FromLengths(0, 0, 0, 0));
                                          titleCloseButton.HorizontalAlignment(HorizontalAlignment::Right);
                                          titleCloseButton.VerticalAlignment(VerticalAlignment::Center);
                                          auto titleCloseGlyph = TextBlock{};
                                          titleCloseGlyph.FontFamily(Media::FontFamily{ L"Segoe Fluent Icons, Segoe MDL2 Assets" });
                                          titleCloseGlyph.FontSize(12);
                                          titleCloseGlyph.HorizontalAlignment(HorizontalAlignment::Center);
                                          titleCloseGlyph.VerticalAlignment(VerticalAlignment::Center);
                                          titleCloseGlyph.Text(L"\xE8BB");
                                          titleCloseButton.Content(titleCloseGlyph);
                                          titleCloseButton.Click([dialog](auto&&, auto&&) {
                                              dialog.Hide();
                                          });
                                          Controls::Grid::SetColumn(titleCloseButton, 1);
                                          titlePanel.Children().Append(titleCloseButton);
                                          dialog.Title(titlePanel);

                                            auto headerPanel = Grid{};
                                          auto familyColumn = ColumnDefinition{};
                                          familyColumn.Width(GridLengthHelper::Auto());
                                          auto spacerColumn = ColumnDefinition{};
                                          spacerColumn.Width(GridLength{ 1.0, GridUnitType::Star });
                                          auto chooseFileColumn = ColumnDefinition{};
                                          chooseFileColumn.Width(GridLengthHelper::Auto());
                                          headerPanel.ColumnDefinitions().Append(familyColumn);
                                          headerPanel.ColumnDefinitions().Append(spacerColumn);
                                          headerPanel.ColumnDefinitions().Append(chooseFileColumn);

                                          auto familyPanel = StackPanel{};
                                          familyPanel.Orientation(Orientation::Horizontal);
                                          familyPanel.Spacing(4);
                                          auto familyButtons = std::make_shared<std::vector<Button>>();
                                          const auto updateFamilyButtonsState = [familyButtons, currentFamily]() {
                                              for (const auto& button : *familyButtons)
                                              {
                                                  const auto buttonFamily = winrt::unbox_value_or<winrt::hstring>(button.Tag(), L"");
                                                  const auto isSelected = *currentFamily == buttonFamily.c_str();
                                                  button.BorderThickness(WUX::ThicknessHelper::FromLengths(isSelected ? 2 : 1, isSelected ? 2 : 1, isSelected ? 2 : 1, isSelected ? 2 : 1));
                                                  button.BorderBrush(SolidColorBrush{ isSelected ? Colors::DodgerBlue() : Colors::DimGray() });
                                              }
                                          };
                                          for (const auto& family : workspaceIconFamilies)
                                          {
                                              auto familyButton = Button{};
                                              familyButton.Content(box_value(family.label));
                                              familyButton.Tag(box_value(family.key));
                                              familyButton.Padding(WUX::ThicknessHelper::FromLengths(6, 3, 6, 3));
                                              familyButton.MinWidth(64);
                                              familyButton.MinHeight(28);
                                              familyButton.Click([currentFamily, updateFamilyButtonsState, familyKey = std::wstring{ family.key }](auto&&, auto&&) {
                                                  *currentFamily = familyKey;
                                                  updateFamilyButtonsState();
                                              });
                                              familyButtons->emplace_back(familyButton);
                                              familyPanel.Children().Append(familyButton);
                                          }
                                          updateFamilyButtonsState();
                                          Controls::Grid::SetColumn(familyPanel, 0);
                                          headerPanel.Children().Append(familyPanel);

                                          auto chooseFileButton = HyperlinkButton{};
                                          chooseFileButton.Content(box_value(L"选择本地文件"));
                                          chooseFileButton.Padding(WUX::ThicknessHelper::FromLengths(4, 2, 4, 2));
                                          chooseFileButton.Margin(WUX::ThicknessHelper::FromLengths(8, 0, 0, 0));
                                          Controls::Grid::SetColumn(chooseFileButton, 2);
                                          headerPanel.Children().Append(chooseFileButton);

                                          rootPanel.Children().Append(headerPanel);

                                            auto iconSectionsPanel = StackPanel{};
                                            iconSectionsPanel.Spacing(1);

                                          auto rebuildIconSections = [weakThis, stableNodeIndex, selectedIcon, currentFamily, iconSectionsPanel, dialog]() {
                                              auto self = weakThis.get();
                                              if (!self)
                                              {
                                                  return;
                                              }
                                              const auto nodeIndex = *stableNodeIndex;
                                              Json::Value rebuildPayload{ Json::objectValue };
                                              rebuildPayload["nodeIndex"] = nodeIndex;
                                              iconSectionsPanel.Children().Clear();
                                              terminal::workspacechat::AddDiagnosticTextFields(rebuildPayload, "family", *currentFamily);
                                              std::ignore = terminal::workspacechat::AppendWorkspaceDiagnosticLog(L"workspace_icon_picker_rebuild_begin", rebuildPayload);

                                              const auto appendSection = [self, dialog, &selectedIcon, &currentFamily, &iconSectionsPanel, nodeIndex](const WorkspaceIconSectionOption& section) {
                                                  auto sectionPanel = StackPanel{};
                                                  sectionPanel.Spacing(1);
                                                  auto sectionRows = StackPanel{};
                                                  sectionRows.Spacing(1);
                                                  StackPanel currentRow{};
                                                  uint32_t iconsInCurrentRow = 0;
                                                  uint32_t rowCount = 0;
                                                  for (uint32_t iconIndex = 0; iconIndex < section.count; ++iconIndex)
                                                  {
                                                      if (!currentRow || iconsInCurrentRow == 0)
                                                      {
                                                          currentRow = StackPanel{};
                                                          currentRow.Orientation(Orientation::Horizontal);
                                                          currentRow.Spacing(1);
                                                          sectionRows.Children().Append(currentRow);
                                                          ++rowCount;
                                                      }

                                                      const auto descriptor = std::wstring{ L"workspace-icon://" } + *currentFamily + L"/" + section.key + L"/" + std::to_wstring(iconIndex);
                                                      auto button = Button{};
                                                      button.Width(40);
                                                      button.Height(40);
                                                      button.MinWidth(40);
                                                      button.MinHeight(40);
                                                      button.Padding(WUX::ThicknessHelper::FromLengths(0, 0, 0, 0));
                                                      button.HorizontalContentAlignment(HorizontalAlignment::Center);
                                                      button.VerticalContentAlignment(VerticalAlignment::Center);
                                                      const auto isSelected = *selectedIcon == descriptor;
                                                      button.BorderThickness(WUX::ThicknessHelper::FromLengths(isSelected ? 2 : 1, isSelected ? 2 : 1, isSelected ? 2 : 1, isSelected ? 2 : 1));
                                                      button.BorderBrush(SolidColorBrush{ isSelected ? Colors::DodgerBlue() : Colors::DimGray() });
                                                      auto preview = self->_CreateNewTabFlyoutIcon(winrt::hstring{ descriptor });
                                                      if (preview)
                                                      {
                                                          if (const auto frameworkElement = preview.try_as<FrameworkElement>())
                                                          {
                                                              frameworkElement.Width(32);
                                                              frameworkElement.Height(32);
                                                              frameworkElement.HorizontalAlignment(HorizontalAlignment::Center);
                                                              frameworkElement.VerticalAlignment(VerticalAlignment::Center);
                                                          }
                                                          button.Content(preview);
                                                      }
                                                      button.Click([dialog, selectedIcon, descriptor](auto&&, auto&&) {
                                                          *selectedIcon = descriptor;
                                                          dialog.Hide();
                                                      });
                                                      currentRow.Children().Append(button);
                                                      ++iconsInCurrentRow;
                                                      if (iconsInCurrentRow >= 10)
                                                      {
                                                          currentRow = nullptr;
                                                          iconsInCurrentRow = 0;
                                                      }
                                                  }
                                                  Json::Value sectionPayload{ Json::objectValue };
                                                  sectionPayload["nodeIndex"] = nodeIndex;
                                                  terminal::workspacechat::AddDiagnosticTextFields(sectionPayload, "family", *currentFamily);
                                                  terminal::workspacechat::AddDiagnosticTextFields(sectionPayload, "section", section.key);
                                                  sectionPayload["iconCount"] = section.count;
                                                  sectionPayload["rowCount"] = rowCount;
                                                  std::ignore = terminal::workspacechat::AppendWorkspaceDiagnosticLog(L"workspace_icon_picker_section_layout", sectionPayload);
                                                  sectionPanel.Children().Append(sectionRows);
                                                  iconSectionsPanel.Children().Append(sectionPanel);
                                              };

                                              for (const auto& section : workspaceIconSections)
                                              {
                                                  appendSection(section);
                                              }
                                              appendSection(workspaceWindowsSection);
                                              std::ignore = terminal::workspacechat::AppendWorkspaceDiagnosticLog(L"workspace_icon_picker_rebuild_end", rebuildPayload);
                                          };

                                          for (const auto& familyButton : *familyButtons)
                                          {
                                              familyButton.Click([rebuildIconSections](auto&&, auto&&) {
                                                  rebuildIconSections();
                                              });
                                          }
                                          chooseFileButton.Click([weakThis, dialog, selectedIcon](auto&&, auto&&) {
                                              [weakThis, dialog, selectedIcon]() -> safe_void_coroutine {
                                                  if (auto self{ weakThis.get() })
                                                  {
                                                      const auto selectedPath = co_await OpenImagePicker(self->_hostingHwnd.value_or(nullptr));
                                                      if (!selectedPath.empty())
                                                      {
                                                          *selectedIcon = selectedPath.c_str();
                                                          dialog.Hide();
                                                      }
                                                  }
                                              }();
                                          });
                                            rebuildIconSections();

                                            rootPanel.Children().Append(iconSectionsPanel);

                                            dialog.Content(rootPanel);
                                          {
                                              Json::Value payload{ Json::objectValue };
                                              payload["nodeIndex"] = nodeIndex;
                                              std::ignore = terminal::workspacechat::AppendWorkspaceDiagnosticLog(L"workspace_icon_picker_show_dialog", payload);
                                          }
                                          const auto presenter = self->_dialogPresenter.get();
                                          if (!presenter)
                                          {
                                              Json::Value payload{ Json::objectValue };
                                              payload["nodeIndex"] = nodeIndex;
                                              std::ignore = terminal::workspacechat::AppendWorkspaceDiagnosticLog(L"workspace_icon_picker_missing_presenter", payload);
                                              co_return;
                                          }
                                          const auto result = co_await presenter.ShowDialog(dialog);
                                          {
                                              Json::Value payload{ Json::objectValue };
                                              payload["nodeIndex"] = nodeIndex;
                                              payload["dialogResult"] = static_cast<int>(result);
                                              std::ignore = terminal::workspacechat::AppendWorkspaceDiagnosticLog(L"workspace_icon_picker_dialog_result", payload);
                                          }
                                          if (!selectedIcon->empty())
                                          {
                                              Json::Value payload{ Json::objectValue };
                                              payload["nodeIndex"] = nodeIndex;
                                              std::wstring previousIcon;
                                              terminal::workspacechat::AddDiagnosticTextFields(payload, "selectedIcon", *selectedIcon);
                                              if (const auto* currentBefore = self->_SelectedWorkspaceForEditing(); currentBefore && nodeIndex < currentBefore->Nodes.size())
                                              {
                                                  previousIcon = currentBefore->Nodes.at(nodeIndex).Icon;
                                                  terminal::workspacechat::AddDiagnosticTextFields(payload, "previousIcon", previousIcon);
                                              }
                                              std::ignore = terminal::workspacechat::AppendWorkspaceDiagnosticLog(L"workspace_icon_picker_selected", payload);
                                              if (previousIcon == *selectedIcon)
                                              {
                                                  Json::Value noopPayload{ Json::objectValue };
                                                  noopPayload["nodeIndex"] = nodeIndex;
                                                  std::ignore = terminal::workspacechat::AppendWorkspaceDiagnosticLog(L"workspace_icon_picker_noop_same_icon", noopPayload);
                                              }
                                              else if (auto* current = self->_SelectedWorkspaceForEditing(); current && nodeIndex < current->Nodes.size())
                                              {
                                                  current->Nodes.at(nodeIndex).Icon = *selectedIcon;
                                                  self->_workspaceExtension->WorkspaceDefinitionsDirty() = true;

                                                  Json::Value appliedPayload{ Json::objectValue };
                                                  appliedPayload["nodeIndex"] = nodeIndex;
                                                  terminal::workspacechat::AddDiagnosticTextFields(appliedPayload, "appliedIcon", current->Nodes.at(nodeIndex).Icon);
                                                  std::ignore = terminal::workspacechat::AppendWorkspaceDiagnosticLog(L"workspace_icon_picker_applied", appliedPayload);

                                                  Json::Value refreshPayload{ Json::objectValue };
                                                  refreshPayload["nodeIndex"] = nodeIndex;
                                                  std::ignore = terminal::workspacechat::AppendWorkspaceDiagnosticLog(L"workspace_icon_picker_rebuild_page_begin", refreshPayload);

                                                  const auto navSelection = Microsoft::Terminal::Settings::Model::implementation::WorkspaceManagerNavSelectionForWorkspaceNode(self->_workspaceExtension->WorkspaceEditorSelectedIndex(), nodeIndex);
                                                  self->_workspaceExtension->WorkspaceManagerNavSelection() = navSelection;
                                                  self->_RebuildWorkspaceManagerTab();

                                                  std::ignore = terminal::workspacechat::AppendWorkspaceDiagnosticLog(L"workspace_icon_picker_rebuild_page_done", refreshPayload);
                                            }
                                        }
                                          }
                                          catch (const winrt::hresult_error& ex)
                                          {
                                              Json::Value payload{ Json::objectValue };
                                              payload["nodeIndex"] = nodeIndex;
                                              terminal::workspacechat::AppendExceptionDiagnostic(payload, ex);
                                              std::ignore = terminal::workspacechat::AppendWorkspaceDiagnosticLog(L"workspace_icon_picker_exception", payload);
                                              throw;
                                          }
                                          catch (const std::exception& ex)
                                          {
                                              Json::Value payload{ Json::objectValue };
                                              payload["nodeIndex"] = nodeIndex;
                                              terminal::workspacechat::AppendExceptionDiagnostic(payload, ex);
                                              std::ignore = terminal::workspacechat::AppendWorkspaceDiagnosticLog(L"workspace_icon_picker_exception", payload);
                                              throw;
                                          }
                                          catch (...)
                                          {
                                              Json::Value payload{ Json::objectValue };
                                              payload["nodeIndex"] = nodeIndex;
                                              terminal::workspacechat::AppendUnknownExceptionDiagnostic(payload);
                                              std::ignore = terminal::workspacechat::AppendWorkspaceDiagnosticLog(L"workspace_icon_picker_exception", payload);
                                              throw;
                                          }
                                      }
                                      co_return;
                                  }();
                              });
                          }
                        nodeRoot.Children().Append(makeWorkspaceSetting(L"图标", iconPanel));

                        const auto addNodePathPicker = [&](const wchar_t* label, const std::wstring& initialValue, const bool pickFolder) {
                            auto panel = StackPanel{};
                            panel.Orientation(Orientation::Horizontal);
                            panel.Spacing(8);
                            auto pathBox = TextBox{};
                            applyWorkspaceStyle(pathBox, L"WorkspaceTextBoxSettingStyle");
                            pathBox.Text(initialValue);
                            pathBox.IsReadOnly(!_workspaceEditorEditMode);
                            if (_workspaceEditorEditMode)
                            {
                                pathBox.TextChanged([weakThis{ get_weak() }, nodeIndex, pickFolder](auto&& sender, auto&&) {
                                    if (auto self{ weakThis.get() })
                                    {
                                        if (auto* current = self->_SelectedWorkspaceForEditing(); current && nodeIndex < current->Nodes.size())
                                        {
                                            auto& value = pickFolder ? current->Nodes.at(nodeIndex).StartupDirectory : current->Nodes.at(nodeIndex).StartupAction;
                                            value = sender.as<TextBox>().Text().c_str();
                                            self->_workspaceExtension->WorkspaceDefinitionsDirty() = true;
                                        }
                                    }
                                });
                            }
                            panel.Children().Append(pathBox);
                            auto browseButton = Button{};
                            auto browseIcon = SymbolIcon{};
                            browseIcon.Symbol(pickFolder ? Symbol::Folder : Symbol::OpenFile);
                            browseButton.Content(browseIcon);
                            browseButton.IsEnabled(_workspaceEditorEditMode);
                            ToolTipService::SetToolTip(browseButton, box_value(pickFolder ? L"选择文件夹" : L"选择文件"));
                            browseButton.Click([weakThis{ get_weak() }, nodeIndex, pickFolder, pathBox](auto&&, auto&&) {
                                [weakThis, nodeIndex, pickFolder, pathBox]() -> safe_void_coroutine {
                                    if (auto self{ weakThis.get() })
                                    {
                                        const auto path = co_await OpenFilePicker(self->_hostingHwnd.value_or(nullptr), [pickFolder](auto&& dialog) {
                                            if (pickFolder)
                                            {
                                                DWORD flags{};
                                                THROW_IF_FAILED(dialog->GetOptions(&flags));
                                                THROW_IF_FAILED(dialog->SetOptions(flags | FOS_PICKFOLDERS));
                                            }
                                        });
                                        if (!path.empty())
                                        {
                                            if (auto* current = self->_SelectedWorkspaceForEditing(); current && nodeIndex < current->Nodes.size())
                                            {
                                                auto& value = pickFolder ? current->Nodes.at(nodeIndex).StartupDirectory : current->Nodes.at(nodeIndex).StartupAction;
                                                value = path.c_str();
                                                pathBox.Text(path);
                                                self->_workspaceExtension->WorkspaceDefinitionsDirty() = true;
                                            }
                                        }
                                    }
                                }();
                            });
                            panel.Children().Append(browseButton);
                            nodeRoot.Children().Append(makeWorkspaceSetting(label, panel));
                        };
                        addNodePathPicker(RS_(L"WorkspaceEditor_StartupDirectory").c_str(), node.StartupDirectory, true);
                        addNodePathPicker(L"启动命令", node.StartupAction, false);
                        auto tabColorPanel = StackPanel{};
                        tabColorPanel.Orientation(Orientation::Horizontal);
                        tabColorPanel.Spacing(8);

                        auto tabColorPreviewRow = StackPanel{};
                        tabColorPreviewRow.Orientation(Orientation::Horizontal);
                        tabColorPreviewRow.Spacing(8);

                        auto tabColorPreview = Border{};
                        tabColorPreview.Width(32);
                        tabColorPreview.Height(24);
                        tabColorPreview.CornerRadius(CornerRadiusHelper::FromUniformRadius(4));
                        tabColorPreview.BorderBrush(SolidColorBrush{ Colors::DarkGray() });
                        tabColorPreview.BorderThickness(WUX::ThicknessHelper::FromLengths(1, 1, 1, 1));

                        auto tabColorValue = TextBlock{};
                        tabColorValue.VerticalAlignment(VerticalAlignment::Center);

                        const auto applyNodeColorPreview = [weakThis{ get_weak() }, nodeIndex, tabColorPreview, tabColorValue]() {
                            if (auto self{ weakThis.get() })
                            {
                                if (const auto* current = self->_SelectedWorkspaceForEditing(); current && nodeIndex < current->Nodes.size())
                                {
                                    const auto resolvedColorText = _workspaceNodeColorDisplayValue(*current, nodeIndex, self->_settings);
                                    if (const auto parsedColor = _parseWorkspaceColor(resolvedColorText))
                                    {
                                        tabColorPreview.Background(SolidColorBrush{ *parsedColor });
                                        tabColorValue.Text(winrt::hstring{ resolvedColorText });
                                    }
                                    else
                                    {
                                        tabColorPreview.Background(SolidColorBrush{ Colors::Transparent() });
                                        tabColorValue.Text(RS_(L"WorkspaceEditor_BackgroundColorAuto"));
                                    }
                                }
                            }
                        };

                        applyNodeColorPreview();
                        if (_workspaceEditorEditMode)
                        {
                            profilePicker.SelectionChanged([weakThis{ get_weak() }, nodeIndex, applyNodeColorPreview](auto&&, auto&&) {
                                if (auto self{ weakThis.get() })
                                {
                                    applyNodeColorPreview();
                                }
                            });
                        }
                        tabColorPreviewRow.Children().Append(tabColorPreview);
                        tabColorPreviewRow.Children().Append(tabColorValue);
                        tabColorPanel.Children().Append(tabColorPreviewRow);

                        if (_workspaceEditorEditMode)
                        {
                            // The swatch is the color control. Opening it keeps the
                            // currently selected color visible while a replacement is
                            // chosen, instead of exposing an implementation string.
                            auto nodeColorFlyout = winrt::make<ColorPickupFlyout>();
                            if (const auto parsedColor = _parseWorkspaceColor(_workspaceNodeColorDisplayValue(*workspace, nodeIndex, _settings)))
                            {
                                nodeColorFlyout.Color(*parsedColor);
                            }
                            nodeColorFlyout.ColorSelected([weakThis{ get_weak() }, nodeIndex, applyNodeColorPreview](const winrt::Windows::UI::Color& color) {
                                if (auto self{ weakThis.get() })
                                {
                                    if (auto* current = self->_SelectedWorkspaceForEditing(); current && nodeIndex < current->Nodes.size())
                                    {
                                        current->Nodes.at(nodeIndex).TabColor = _workspaceColorToString(color);
                                        self->_workspaceExtension->WorkspaceDefinitionsDirty() = true;
                                        applyNodeColorPreview();
                                    }
                                }
                            });
                            tabColorPreviewRow.Tapped([nodeColorFlyout, tabColorPreviewRow](auto&&, auto&&) {
                                nodeColorFlyout.ShowAt(tabColorPreviewRow);
                            });

                            auto tabColorButtons = StackPanel{};
                            tabColorButtons.Orientation(Orientation::Horizontal);
                            tabColorButtons.Spacing(8);

                            auto reselectTabColorButton = Button{};
                            auto reselectTabColorIcon = SymbolIcon{};
                            reselectTabColorIcon.Symbol(Symbol::Refresh);
                            reselectTabColorButton.Content(reselectTabColorIcon);
                            ToolTipService::SetToolTip(reselectTabColorButton, box_value(L"换一个"));
                            Automation::AutomationProperties::SetName(reselectTabColorButton, L"换一个");
                            reselectTabColorButton.Click([weakThis{ get_weak() }, nodeIndex, applyNodeColorPreview](auto&&, auto&&) {
                                if (auto self{ weakThis.get() })
                                {
                                    if (auto* current = self->_SelectedWorkspaceForEditing(); current && nodeIndex < current->Nodes.size())
                                    {
                                        const auto previousColor = current->Nodes.at(nodeIndex).TabColor;
                                        static constexpr std::array<std::wstring_view, 12> palette{
                                            L"#C50F1F", L"#0063B1", L"#0F7B0F", L"#CA5010",
                                            L"#8E562E", L"#744DA9", L"#038387", L"#881798",
                                            L"#498205", L"#515C6B", L"#567C73", L"#7A7574",
                                        };
                                        std::unordered_set<std::wstring> usedColors;
                                        for (size_t i = 0; i < current->Nodes.size(); ++i)
                                        {
                                            if (i != nodeIndex && !current->Nodes.at(i).TabColor.empty())
                                            {
                                                usedColors.emplace(current->Nodes.at(i).TabColor);
                                            }
                                        }
                                        const auto replacement = std::find_if(palette.begin(), palette.end(), [&](const auto candidate) {
                                            return candidate != std::wstring_view{ previousColor } && !usedColors.contains(std::wstring{ candidate });
                                        });
                                        current->Nodes.at(nodeIndex).TabColor = replacement != palette.end() ? std::wstring{ *replacement } : std::wstring{};
                                        EnsureWorkspaceNodeTabColors(*current, self->_settings);
                                        self->_workspaceExtension->WorkspaceDefinitionsDirty() = true;
                                        applyNodeColorPreview();
                                    }
                                }
                            });

                            tabColorButtons.Children().Append(reselectTabColorButton);
                            tabColorPanel.Children().Append(tabColorButtons);
                        }

                        nodeRoot.Children().Append(makeWorkspaceSetting(RS_(L"WorkspaceEditor_TabColor"), tabColorPanel));
                        const auto appendToggleRow = [&](const winrt::hstring& labelText, const WUX::Controls::ToggleSwitch& toggle) {
                            toggle.HorizontalAlignment(HorizontalAlignment::Right);
                            nodeRoot.Children().Append(makeWorkspaceSetting(labelText, toggle));
                        };
                        appendToggleRow(RS_(L"WorkspaceEditor_ShowTab"), showTabToggle);

                        auto showInputPanelToggle = WUX::Controls::ToggleSwitch{};
                        applyWorkspaceStyle(showInputPanelToggle, L"WorkspaceToggleSwitchStyle");
                        showInputPanelToggle.Header(nullptr);
                        showInputPanelToggle.IsOn(node.ShowInputPanel);
                        showInputPanelToggle.IsEnabled(_workspaceEditorEditMode);
                        if (_workspaceEditorEditMode)
                        {
                            showInputPanelToggle.Toggled([weakThis{ get_weak() }, nodeIndex](auto&& sender, auto&&) {
                                if (auto self{ weakThis.get() })
                                {
                                    if (auto* current = self->_SelectedWorkspaceForEditing(); current && nodeIndex < current->Nodes.size())
                                    {
                                        if (const auto toggle = sender.try_as<WUX::Controls::ToggleSwitch>())
                                        {
                                            current->Nodes.at(nodeIndex).ShowInputPanel = toggle.IsOn();
                                            self->_workspaceExtension->WorkspaceDefinitionsDirty() = true;
                                        }
                                    }
                                }
                            });
                        }
                        appendToggleRow(RS_(L"WorkspaceEditor_ShowInputPanel"), showInputPanelToggle);

                        auto useDefinedTitleToggle = WUX::Controls::ToggleSwitch{};
                        applyWorkspaceStyle(useDefinedTitleToggle, L"WorkspaceToggleSwitchStyle");
                        useDefinedTitleToggle.Header(nullptr);
                        useDefinedTitleToggle.IsOn(node.UseNodeNameAsTabTitle);
                        useDefinedTitleToggle.IsEnabled(_workspaceEditorEditMode);
                        if (_workspaceEditorEditMode)
                        {
                            useDefinedTitleToggle.Toggled([weakThis{ get_weak() }, nodeIndex](auto&& sender, auto&&) {
                                if (auto self{ weakThis.get() })
                                {
                                    if (auto* current = self->_SelectedWorkspaceForEditing(); current && nodeIndex < current->Nodes.size())
                                    {
                                        if (const auto toggle = sender.try_as<WUX::Controls::ToggleSwitch>())
                                        {
                                            current->Nodes.at(nodeIndex).UseNodeNameAsTabTitle = toggle.IsOn();
                                            self->_workspaceExtension->WorkspaceDefinitionsDirty() = true;
                                        }
                                    }
                                }
                            });
                        }
                        appendToggleRow(RS_(L"WorkspaceEditor_UseNodeNameAsTabTitle"), useDefinedTitleToggle);

                        root.Children().Append(nodeRoot);
                    }
                }
            }
        }
        else
        {
            root.Children().Append(makeSectionTitle(L"工作区管理"));
        }

        auto footer = Grid{};
        footer.Height(56);
        footer.BorderBrush(SolidColorBrush{ Colors::DarkGray() });
        footer.BorderThickness(WUX::ThicknessHelper::FromLengths(0, 1, 0, 0));
        footer.Padding(WUX::ThicknessHelper::FromLengths(16, 0, 16, 0));
        footer.ColumnDefinitions().Append(ColumnDefinition{});
        auto buttonsColumn = ColumnDefinition{};
        buttonsColumn.Width(GridLengthHelper::Auto());
        footer.ColumnDefinitions().Append(buttonsColumn);
        Controls::Grid::SetRow(footer, 1);

        auto footerButtons = StackPanel{};
        footerButtons.Orientation(Orientation::Horizontal);
        footerButtons.HorizontalAlignment(HorizontalAlignment::Right);
        footerButtons.VerticalAlignment(VerticalAlignment::Center);
        Controls::Grid::SetColumn(footerButtons, 1);

        auto saveButton = Button{};
        saveButton.Content(box_value(RS_(L"WorkspaceEditor_DialogPrimaryButton")));
        applyOptionalStyle(saveButton, L"AccentButtonStyle");
        saveButton.IsEnabled(true);
        saveButton.Margin(WUX::ThicknessHelper::FromLengths(0, 0, 12, 0));
        saveButton.Click([weakThis{ get_weak() }](auto&&, auto&&) {
            if (auto self{ weakThis.get() })
            {
                self->_SaveWorkspaceEditorState();
            }
        });
        footerButtons.Children().Append(saveButton);

        auto resetButton = Button{};
        resetButton.Content(box_value(RS_(L"WorkspaceEditor_ResetButton")));
        resetButton.IsEnabled(_workspaceDefinitionsDirty);
        resetButton.Click([weakThis{ get_weak() }](auto&&, auto&&) {
            if (auto self{ weakThis.get() })
            {
                self->_workspaceExtension->WorkspaceDefinitionsDirty() = false;
                self->_workspaceExtension->WorkspaceEditorEditMode() = true;
                self->_LoadWorkspaceEditorState(false);
                self->_workspaceExtension->WorkspaceManagerNavSelection() = Microsoft::Terminal::Settings::Model::implementation::ResolveWorkspaceManagerNavSelectionForEditor(self->_workspaceExtension->WorkspaceEditorManager().Workspaces().size(),
                                                                                                                                                                            self->_workspaceExtension->WorkspaceEditorSelectedIndex());
                self->_RebuildWorkspaceManagerTab();
            }
        });
        footerButtons.Children().Append(resetButton);

        footer.Children().Append(footerButtons);
        contentGrid.Children().Append(footer);

        nav.Content(contentGrid);
        {
            Json::Value payload{ Json::objectValue };
            payload["workspaceCount"] = gsl::narrow<Json::ArrayIndex>(workspaces.size());
            payload["navSelection"] = _workspaceManagerNavSelection;
            payload["selectedWorkspaceIndex"] = _workspaceEditorSelectedIndex;
            payload["menuItemCount"] = nav.MenuItems().Size();
            payload["isFirstRun"] = workspaces.empty();
            std::ignore = terminal::workspacechat::AppendWorkspaceDiagnosticLog(L"workspace_manager_build_end", payload);
        }
        return nav;
    }

    void TerminalPage::_WorkspaceManagerPrimaryButtonClick(const IInspectable& /*sender*/, const ContentDialogButtonClickEventArgs& eventArgs)
    {
        if (!_workspaceEditorEditMode)
        {
            eventArgs.Cancel(true);
            return;
        }

        if (!_SaveWorkspaceEditorState())
        {
            eventArgs.Cancel(true);
        }
    }
