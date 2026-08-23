#include "../../../../microsoft/src/cascadia/WinRTUtils/inc/Utils.h"
#include "../chat/WorkspaceDiagnosticLog.h"

    int32_t TerminalPage::_WorkspaceManagerWorkspaceNavSelection(const size_t workspaceIndex) const
    {
        return Microsoft::Terminal::Settings::Model::implementation::WorkspaceManagerNavSelectionForWorkspace(workspaceIndex);
    }

    int32_t TerminalPage::_WorkspaceManagerWorkspaceNodeNavSelection(const size_t workspaceIndex, const size_t nodeIndex) const
    {
        return Microsoft::Terminal::Settings::Model::implementation::WorkspaceManagerNavSelectionForWorkspaceNode(workspaceIndex, nodeIndex);
    }

    int32_t TerminalPage::_WorkspaceManagerEditorNavSelection() const
    {
        return Microsoft::Terminal::Settings::Model::implementation::ResolveWorkspaceManagerNavSelectionForEditor(_workspaceExtension->WorkspaceEditorManager().Workspaces().size(),
                                                                                                                    _workspaceExtension->WorkspaceEditorSelectedIndex());
    }

    void TerminalPage::_ApplyWorkspaceManagerNavSelection(const int32_t navSelection, const bool rebuild)
    {
        _workspaceExtension->WorkspaceManagerNavSelection() = navSelection;
        if (const auto workspaceIndex = Microsoft::Terminal::Settings::Model::implementation::ResolveWorkspaceIndexFromManagerNavSelection(navSelection))
        {
            _SetSelectedWorkspaceIndex(*workspaceIndex);
        }

        if (rebuild)
        {
            _RebuildWorkspaceManagerTab();
        }
    }

    void TerminalPage::_ApplyWorkspaceManagerWorkspaceIconSelection(const std::wstring_view iconValue)
    {
        if (auto* current = _SelectedWorkspaceForEditing(); current && current->Icon != iconValue)
        {
            current->Icon = iconValue;
            _workspaceExtension->WorkspaceDefinitionsDirty() = true;
            _ApplyWorkspaceManagerNavSelection(_WorkspaceManagerWorkspaceNavSelection(_workspaceExtension->WorkspaceEditorSelectedIndex()));
        }
    }

    void TerminalPage::_ApplyWorkspaceManagerNodeIconSelection(const size_t nodeIndex, const std::wstring_view iconValue)
    {
        if (auto* current = _SelectedWorkspaceForEditing(); current && nodeIndex < current->Nodes.size())
        {
            current->Nodes.at(nodeIndex).Icon = iconValue;
            _workspaceExtension->WorkspaceDefinitionsDirty() = true;

            Json::Value appliedPayload{ Json::objectValue };
            appliedPayload["nodeIndex"] = nodeIndex;
            terminal::workspacechat::AddDiagnosticTextFields(appliedPayload, "appliedIcon", current->Nodes.at(nodeIndex).Icon);
            std::ignore = terminal::workspacechat::AppendWorkspaceDiagnosticLog(L"workspace_icon_picker_applied", appliedPayload);

            Json::Value refreshPayload{ Json::objectValue };
            refreshPayload["nodeIndex"] = nodeIndex;
            std::ignore = terminal::workspacechat::AppendWorkspaceDiagnosticLog(L"workspace_icon_picker_rebuild_page_begin", refreshPayload);
            _ApplyWorkspaceManagerNavSelection(_WorkspaceManagerWorkspaceNodeNavSelection(_workspaceExtension->WorkspaceEditorSelectedIndex(), nodeIndex));
            std::ignore = terminal::workspacechat::AppendWorkspaceDiagnosticLog(L"workspace_icon_picker_rebuild_page_done", refreshPayload);
        }
    }

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
        const auto loadedFlyoutState = ::terminal::workspace::LoadWorkspaceFlyoutState(_currentWorkspaceId.c_str());
        std::unordered_set<std::wstring> openWorkspaceIds;
        openWorkspaceIds.reserve(loadedFlyoutState.FlyoutState.Entries.size());
        for (const auto& entry : loadedFlyoutState.FlyoutState.Entries)
        {
            if (entry.IsOpen)
            {
                openWorkspaceIds.emplace(entry.Definition.Id);
            }
        }
        const auto& workspaces = _workspaceEditorManager.Workspaces();
        auto nav = MUX::Controls::NavigationView{};
        nav.Background(SolidColorBrush{ Colors::Transparent() });
        nav.IsBackButtonVisible(MUX::Controls::NavigationViewBackButtonVisible::Collapsed);
        nav.IsPaneToggleButtonVisible(false);
        nav.IsSettingsVisible(false);
        nav.PaneDisplayMode(MUX::Controls::NavigationViewPaneDisplayMode::Left);
        nav.OpenPaneLength(320);
        nav.AlwaysShowHeader(false);

        if (_workspaceManagerNavSelection == 0 && !workspaces.empty())
        {
            _workspaceManagerNavSelection = _WorkspaceManagerWorkspaceNavSelection(0);
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
                _workspaceManagerNavSelection = _WorkspaceManagerEditorNavSelection();
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
            const auto isOpen = openWorkspaceIds.contains(workspace.Id);
            auto item = MUX::Controls::NavigationViewItem{};
            auto workspaceHeader = Grid{};
            auto workspaceNameColumn = ColumnDefinition{};
            workspaceNameColumn.Width(GridLengthHelper::FromValueAndType(1.0, GridUnitType::Star));
            workspaceHeader.ColumnDefinitions().Append(workspaceNameColumn);
            auto workspaceStateColumn = ColumnDefinition{};
            workspaceStateColumn.Width(GridLengthHelper::Auto());
            workspaceHeader.ColumnDefinitions().Append(workspaceStateColumn);

            auto workspaceNameText = TextBlock{};
            workspaceNameText.Text(winrt::hstring{ _WorkspaceDisplayName(workspace) });
            workspaceNameText.TextTrimming(TextTrimming::CharacterEllipsis);
            workspaceNameText.VerticalAlignment(VerticalAlignment::Center);
            workspaceHeader.Children().Append(workspaceNameText);

            if (isOpen)
            {
                auto workspaceStateText = TextBlock{};
                workspaceStateText.Text(L"打开中");
                workspaceStateText.Margin(WUX::ThicknessHelper::FromLengths(8, 0, 0, 0));
                workspaceStateText.VerticalAlignment(VerticalAlignment::Center);
                workspaceStateText.Opacity(0.72);
                Controls::Grid::SetColumn(workspaceStateText, 1);
                workspaceHeader.Children().Append(workspaceStateText);
            }

            item.Content(workspaceHeader);
            item.SelectsOnInvoked(false);
            item.IsExpanded(_workspaceManagerNavSelection >= 1000 &&
                             Microsoft::Terminal::Settings::Model::implementation::ResolveWorkspaceIndexFromManagerNavSelection(_workspaceManagerNavSelection) == index);
            item.Tapped([item](auto&&, auto&&) {
                item.IsExpanded(!item.IsExpanded());
            });
            WUX::Controls::ToolTipService::SetToolTip(item, box_value(isOpen ? L"工作区当前已打开" : L"工作区当前未打开"));

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
                    self->_ApplyWorkspaceManagerNavSelection(self->_WorkspaceManagerWorkspaceNodeNavSelection(index, nodes.size() - 1));
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
                            self->_ApplyWorkspaceManagerNavSelection(self->_WorkspaceManagerWorkspaceNodeNavSelection(index, nodes.size() - 1));
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
            generalItem.Tag(box_value(_WorkspaceManagerWorkspaceNavSelection(index)));
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
                                self->_ApplyWorkspaceManagerNavSelection(self->_WorkspaceManagerWorkspaceNodeNavSelection(index, target));
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
                nodeItem.Tag(box_value(_WorkspaceManagerWorkspaceNodeNavSelection(index, nodeIndex)));
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
                                    self->_ApplyWorkspaceManagerNavSelection(self->_WorkspaceManagerWorkspaceNavSelection(workspaces.size() - 1));
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
                                            self->_ApplyWorkspaceManagerNavSelection(self->_WorkspaceManagerWorkspaceNavSelection(workspaces.size() - 1));
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
                    self->_ApplyWorkspaceManagerNavSelection(value);
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
        root.VerticalAlignment(workspaces.empty() ? VerticalAlignment::Center : VerticalAlignment::Stretch);
        root.Margin(WUX::ThicknessHelper::FromLengths(16, 0, 16, 16));
        root.Resources().MergedDictionaries().Append(workspaceResources);
        applyWorkspaceStyle(root, L"WorkspaceSettingsStackStyle");
        scrollViewer.Content(root);
        contentGrid.Children().Append(scrollViewer);

        if (workspaces.empty())
        {
            auto empty = TextBlock{};
            empty.Text(L"暂时没有工作区");
            empty.TextWrapping(TextWrapping::Wrap);
            empty.HorizontalTextAlignment(TextAlignment::Center);
            empty.HorizontalAlignment(HorizontalAlignment::Center);
            empty.VerticalAlignment(VerticalAlignment::Center);
            root.Children().Append(empty);
        }
        else if (_workspaceManagerNavSelection >= 1000)
        {
            auto* workspace = _SelectedWorkspaceForEditing();
            const auto selectedNodeIndex = Microsoft::Terminal::Settings::Model::implementation::ResolveWorkspaceNodeIndexFromManagerNavSelection(_workspaceManagerNavSelection);
            if (workspace == nullptr)
            {
                auto empty = TextBlock{};
                empty.Text(L"暂时没有工作区");
                empty.TextWrapping(TextWrapping::Wrap);
                empty.HorizontalTextAlignment(TextAlignment::Center);
                empty.HorizontalAlignment(HorizontalAlignment::Center);
                root.Children().Append(empty);
            }
            else
            {
                if (!selectedNodeIndex.has_value())
                {
                    _AppendWorkspaceManagerWorkspaceEditorContent(root, workspaceResources);
                }

                if (selectedNodeIndex.has_value())
                {
                    _AppendWorkspaceManagerNodeEditorContent(root, *selectedNodeIndex, workspaceResources);
                }
            }
        }
        else
        {
            root.Children().Append(makeSectionTitle(L"工作区管理"));
        }

        if (!workspaces.empty())
        {
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
                    self->_ApplyWorkspaceManagerNavSelection(self->_WorkspaceManagerEditorNavSelection());
                }
            });
            footerButtons.Children().Append(resetButton);

            footer.Children().Append(footerButtons);
            contentGrid.Children().Append(footer);
        }

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
