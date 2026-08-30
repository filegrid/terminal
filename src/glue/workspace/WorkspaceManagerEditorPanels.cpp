    void TerminalPage::_AppendWorkspaceManagerWorkspaceEditorContent(const StackPanel& root,
                                                                    const ResourceDictionary& workspaceResources)
    {
        auto* workspace = _SelectedWorkspaceForEditing();
        if (!workspace)
        {
            auto empty = TextBlock{};
            empty.Text(L"暂时没有工作区");
            empty.TextWrapping(TextWrapping::Wrap);
            empty.HorizontalTextAlignment(TextAlignment::Center);
            empty.HorizontalAlignment(HorizontalAlignment::Center);
            root.Children().Append(empty);
            return;
        }

        const auto marginBottom = [](const double bottom) {
            return WUX::ThicknessHelper::FromLengths(0, 0, 0, bottom);
        };
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
                setting.Content(toggle);
            }
            else
            {
                const auto contentElement = content.as<FrameworkElement>();
                contentElement.HorizontalAlignment(HorizontalAlignment::Right);
                setting.Content(content);
            }
            applyWorkspaceStyle(setting, L"WorkspaceSettingContainerStyle");
            return setting;
        };
        const auto addLabeledTextBox = [&](StackPanel& panel, const wchar_t* labelText, const std::wstring& initialValue, const auto& onChanged, const bool readOnly, const bool multiline = false) {
            auto textBox = TextBox{};
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
                    self->_workspaceExtension->UpdateWorkspaceManagerWorkspaceText(
                        terminal::workspace::WorkspaceManagerWorkspaceTextField::Name,
                        sender.as<TextBox>().Text());
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
            const auto workspaceId = workspace->Id;
            deleteWorkspaceButton.Click([weakThis{ get_weak() }, workspaceId](auto&&, auto&&) {
                if (auto self{ weakThis.get() })
                {
                    self->_workspaceExtension->DeleteWorkspaceManagerWorkspace(winrt::hstring{ workspaceId });
                }
            });
            workspaceNamePanel.Children().Append(deleteWorkspaceButton);
        }
        generalPanel.Children().Append(makeWorkspaceSetting(RS_(L"WorkspaceEditor_WorkspaceName"), workspaceNamePanel));
        addLabeledTextBox(generalPanel, RS_(L"WorkspaceEditor_Description").c_str(), workspace->Description, [weakThis{ get_weak() }](auto&& sender, auto&&) {
            if (auto self{ weakThis.get() })
            {
                self->_workspaceExtension->UpdateWorkspaceManagerWorkspaceText(
                    terminal::workspace::WorkspaceManagerWorkspaceTextField::Description,
                    sender.as<TextBox>().Text());
            }
        }, !_workspaceEditorEditMode, true);

        generalPanel.Children().Append(makeSectionTitle(L"节点顺序"));
        auto reorderList = ListView{};
        reorderList.CanDragItems(_workspaceEditorEditMode);
        reorderList.CanReorderItems(_workspaceEditorEditMode);
        reorderList.AllowDrop(_workspaceEditorEditMode);
        reorderList.Margin(marginBottom(16));
        uint32_t displayOrder = 0;
        for (size_t nodeIndex = 0; nodeIndex < workspace->Nodes.size(); ++nodeIndex)
        {
            const auto& candidate = workspace->Nodes.at(nodeIndex);
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
            const auto resolvedIcon = _workspaceExtension->WorkspaceManagerNodeIconPreviewForEditing(nodeIndex);
            if (!resolvedIcon.empty())
            {
                icon = _CreateNewTabFlyoutIcon(resolvedIcon);
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
                    std::vector<winrt::hstring> order;
                    order.reserve(reorderList.Items().Size());
                    for (uint32_t itemIndex = 0; itemIndex < reorderList.Items().Size(); ++itemIndex)
                    {
                        if (const auto element = reorderList.Items().GetAt(itemIndex).try_as<FrameworkElement>())
                        {
                            order.emplace_back(winrt::unbox_value<winrt::hstring>(element.Tag()));
                        }
                    }
                    if (order.empty())
                    {
                        return;
                    }
                    self->_workspaceExtension->ReorderWorkspaceManagerVisibleNodes(order);
                }
            });
        }
        generalPanel.Children().Append(reorderList);

        generalPanel.Children().Append(makeSectionTitle(L"节点默认值"));
        const auto defaultProfileOptions = _workspaceExtension->WorkspaceManagerProfileOptionsForEditing(
            winrt::hstring{ workspace->NewNodeDefaults.ProfileGuid }, winrt::hstring{ workspace->NewNodeDefaults.ProfileName });
        auto defaultProfilePicker = _workspaceExtension->CreateWorkspaceManagerProfilePicker(
            defaultProfileOptions,
            workspace->NewNodeDefaults.ProfileGuid,
            _workspaceEditorEditMode);
        applyWorkspaceStyle(defaultProfilePicker, L"WorkspaceComboBoxSettingStyle");
        if (_workspaceEditorEditMode)
        {
            defaultProfilePicker.SelectionChanged([weakThis{ get_weak() }](auto&& sender, auto&&) {
                if (auto self{ weakThis.get() })
                {
                    if (const auto item = sender.as<ComboBox>().SelectedItem().try_as<ComboBoxItem>())
                    {
                        self->_workspaceExtension->UpdateWorkspaceManagerDefaultProfile(
                            winrt::unbox_value<winrt::hstring>(item.Tag()),
                            winrt::unbox_value_or<winrt::hstring>(item.Content(), {}));
                    }
                }
            });
        }
        generalPanel.Children().Append(makeWorkspaceSetting(L"来源", defaultProfilePicker));
        addLabeledTextBox(generalPanel, L"启动目录", workspace->NewNodeDefaults.StartupDirectory, [weakThis{ get_weak() }](auto&& sender, auto&&) {
            if (auto self{ weakThis.get() })
            {
                self->_workspaceExtension->UpdateWorkspaceManagerWorkspaceText(
                    terminal::workspace::WorkspaceManagerWorkspaceTextField::DefaultStartupDirectory,
                    sender.as<TextBox>().Text());
            }
        }, !_workspaceEditorEditMode);
        const auto addDefaultToggle = [&](const wchar_t* label, const bool isOn, const auto& onToggled) {
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
                self->_workspaceExtension->UpdateWorkspaceManagerWorkspaceBool(
                    terminal::workspace::WorkspaceManagerWorkspaceBoolField::DefaultShowInputPanel,
                    sender.as<WUX::Controls::ToggleSwitch>().IsOn());
            }
        });
        addDefaultToggle(L"固定标题", workspace->NewNodeDefaults.UseNodeNameAsTabTitle, [weakThis{ get_weak() }](auto&& sender, auto&&) {
            if (auto self{ weakThis.get() })
            {
                self->_workspaceExtension->UpdateWorkspaceManagerWorkspaceBool(
                    terminal::workspace::WorkspaceManagerWorkspaceBoolField::DefaultUseNodeNameAsTabTitle,
                    sender.as<WUX::Controls::ToggleSwitch>().IsOn());
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
                    self->_workspaceExtension->UpdateWorkspaceManagerWorkspaceBool(
                        terminal::workspace::WorkspaceManagerWorkspaceBoolField::DefaultShowTab,
                        sender.as<WUX::Controls::ToggleSwitch>().IsOn());
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
            chooseColorButton.Click([weakThis{ get_weak() }, applyWorkspaceColorPreview](auto&&, auto&&) {
                if (auto self{ weakThis.get() })
                {
                    if (const auto color = self->_workspaceExtension->RotateWorkspaceManagerWorkspaceBackgroundColor(); !color.empty())
                    {
                        applyWorkspaceColorPreview(color.c_str());
                    }
                }
            });
            colorPreviewRow.Tapped([weakThis{ get_weak() }, applyWorkspaceColorPreview, colorPreviewRow](auto&&, auto&&) {
                if (auto self{ weakThis.get() })
                {
                    // Keep workspace colors on the same preset/custom flyout used by tabs.
                    auto picker = winrt::TerminalApp::ColorPickupFlyout{};
                    if (const auto color = _parseWorkspaceColor(self->_workspaceExtension->WorkspaceManagerWorkspaceBackgroundColor().c_str()))
                    {
                        picker.Color(*color);
                    }
                    picker.ColorSelected([weakThis, applyWorkspaceColorPreview](const auto color) {
                        if (auto strong{ weakThis.get() })
                        {
                            wchar_t text[8]{};
                            swprintf_s(text, L"#%02X%02X%02X", color.R, color.G, color.B);
                            if (strong->_workspaceExtension->ApplyWorkspaceManagerWorkspaceBackgroundColor(text))
                            {
                                applyWorkspaceColorPreview(text);
                            }
                        }
                    });
                    picker.ShowAt(colorPreviewRow);
                }
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
                const auto currentIcon = self->_workspaceExtension->WorkspaceManagerWorkspaceIconForEditing();
                {
                    WUX::Controls::IconElement previewIcon{ nullptr };
                    if (!currentIcon.empty())
                    {
                        previewIcon = self->_CreateNewTabFlyoutIcon(currentIcon);
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
            workspaceIconButton.Click([weakThis{ get_weak() }](auto&&, auto&&) {
                if (auto self{ weakThis.get() })
                {
                    self->_workspaceExtension->ShowWorkspaceManagerWorkspaceIconPicker();
                }
            });
        }

        generalPanel.Children().InsertAt(3, makeWorkspaceSetting(RS_(L"WorkspaceEditor_BackgroundColor"), colorPanel));
        generalPanel.Children().InsertAt(4, makeWorkspaceSetting(L"图标", workspaceIconPanel));
        root.Children().Append(generalPanel);
    }

    void TerminalPage::_AppendWorkspaceManagerNodeEditorContent(const StackPanel& root,
                                                               const size_t nodeIndex,
                                                               const ResourceDictionary& workspaceResources)
    {
        auto* workspace = _SelectedWorkspaceForEditing();
        if (!workspace)
        {
            auto empty = TextBlock{};
            empty.Text(L"暂时没有工作区");
            empty.TextWrapping(TextWrapping::Wrap);
            empty.HorizontalTextAlignment(TextAlignment::Center);
            empty.HorizontalAlignment(HorizontalAlignment::Center);
            root.Children().Append(empty);
            return;
        }

        const auto marginBottom = [](const double bottom) {
            return WUX::ThicknessHelper::FromLengths(0, 0, 0, bottom);
        };
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
                setting.Content(toggle);
            }
            else
            {
                const auto contentElement = content.as<FrameworkElement>();
                contentElement.HorizontalAlignment(HorizontalAlignment::Right);
                setting.Content(content);
            }
            applyWorkspaceStyle(setting, L"WorkspaceSettingContainerStyle");
            return setting;
        };

        if (nodeIndex >= workspace->Nodes.size())
        {
            auto emptyNodes = TextBlock{};
            emptyNodes.Text(RS_(L"WorkspaceEditor_NoNodes"));
            emptyNodes.Margin(marginBottom(12));
            root.Children().Append(emptyNodes);
            return;
        }

        const auto stableNodeIndex = std::make_shared<size_t>(nodeIndex);
        const auto& node = workspace->Nodes.at(nodeIndex);
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
                    self->_workspaceExtension->UpdateWorkspaceManagerNodeText(
                        nodeIndex,
                        terminal::workspace::WorkspaceManagerNodeTextField::Name,
                        sender.as<TextBox>().Text());
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
            const auto workspaceId = workspace->Id;
            const auto nodeId = node.Id;
            deleteNodeButton.Click([weakThis{ get_weak() }, workspaceId, nodeId](auto&&, auto&&) {
                if (auto self{ weakThis.get() })
                {
                    self->_workspaceExtension->DeleteWorkspaceManagerNode(winrt::hstring{ workspaceId }, winrt::hstring{ nodeId });
                }
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
                    if (const auto toggle = sender.try_as<WUX::Controls::ToggleSwitch>())
                    {
                        self->_workspaceExtension->UpdateWorkspaceManagerNodeBool(nodeIndex, terminal::workspace::WorkspaceManagerNodeBoolField::ShowTab, toggle.IsOn());
                    }
                }
            });
        }

        const auto profileOptions = _workspaceExtension->WorkspaceManagerProfileOptionsForEditing(
            winrt::hstring{ node.ProfileGuid }, winrt::hstring{ node.ProfileName });
        auto profilePicker = _workspaceExtension->CreateWorkspaceManagerProfilePicker(
            profileOptions,
            node.ProfileGuid,
            _workspaceEditorEditMode);
        applyWorkspaceStyle(profilePicker, L"WorkspaceComboBoxSettingStyle");
        if (_workspaceEditorEditMode)
        {
            profilePicker.SelectionChanged([weakThis{ get_weak() }, stableNodeIndex](auto&& sender, auto&&) {
                if (auto self{ weakThis.get() })
                {
                    const auto nodeIndex = *stableNodeIndex;
                    if (const auto picker = sender.try_as<ComboBox>())
                    {
                        if (const auto item = picker.SelectedItem().try_as<ComboBoxItem>())
                        {
                            self->_workspaceExtension->UpdateWorkspaceManagerNodeProfile(nodeIndex, winrt::unbox_value<winrt::hstring>(item.Tag()), winrt::unbox_value_or<winrt::hstring>(item.Content(), {}));
                        }
                    }
                }
            });
        }
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
                const auto resolvedIcon = self->_workspaceExtension->WorkspaceManagerNodeIconPreviewForEditing(nodeIndex);
                {
                    WUX::Controls::IconElement previewIcon{ nullptr };
                    if (!resolvedIcon.empty())
                    {
                        previewIcon = self->_CreateNewTabFlyoutIcon(resolvedIcon);
                    }
                    if (!previewIcon)
                    {
                        auto fallback = SymbolIcon{};
                        fallback.Symbol(Symbol::Page);
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
                    iconPreview.Content(previewIcon);
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
            iconButton.Click([weakThis{ get_weak() }, stableNodeIndex](auto&&, auto&&) {
                if (auto self{ weakThis.get() })
                {
                    self->_workspaceExtension->ShowWorkspaceManagerNodeIconPicker(*stableNodeIndex);
                }
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
                        self->_workspaceExtension->UpdateWorkspaceManagerNodeText(
                            nodeIndex,
                            pickFolder ? terminal::workspace::WorkspaceManagerNodeTextField::StartupDirectory : terminal::workspace::WorkspaceManagerNodeTextField::StartupAction,
                            sender.as<TextBox>().Text());
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
            Automation::AutomationProperties::SetName(browseButton, pickFolder ? L"选择文件夹" : L"选择文件");
            browseButton.Click([weakThis{ get_weak() }, nodeIndex, pickFolder, pathBox](auto&&, auto&&) {
                [](auto weakThis, size_t nodeIndex, bool pickFolder, TextBox pathBox) -> safe_void_coroutine {
                    if (auto self{ weakThis.get() })
                    {
                        const auto path = co_await self->_workspaceExtension->PickWorkspaceManagerPath(pickFolder);
                        if (auto strong{ weakThis.get() }; strong && !path.empty())
                        {
                            if (strong->_workspaceExtension->UpdateWorkspaceManagerNodeText(
                                    nodeIndex,
                                    pickFolder ? terminal::workspace::WorkspaceManagerNodeTextField::StartupDirectory : terminal::workspace::WorkspaceManagerNodeTextField::StartupAction,
                                    path))
                            {
                                pathBox.Text(path);
                            }
                        }
                    }
                }(weakThis, nodeIndex, pickFolder, pathBox);
            });
            panel.Children().Append(browseButton);
            nodeRoot.Children().Append(makeWorkspaceSetting(label, panel));
        };
        addNodePathPicker(RS_(L"WorkspaceEditor_StartupDirectory").c_str(), node.StartupDirectory, true);
        // A legacy node still renders as one command. Once edited, it uses the
        // ordered Commands collection that the Core serializer persists.
        auto commandHeader = Grid{};
        commandHeader.ColumnDefinitions().Append(ColumnDefinition{});
        auto commandActionsColumn = ColumnDefinition{};
        commandActionsColumn.Width(GridLengthHelper::Auto());
        commandHeader.ColumnDefinitions().Append(commandActionsColumn);
        commandHeader.HorizontalAlignment(HorizontalAlignment::Stretch);
        auto commandHeaderText = makeSectionTitle(L"命令窗口");
        commandHeaderText.VerticalAlignment(VerticalAlignment::Center);
        commandHeader.Children().Append(commandHeaderText);
        auto addCommandButton = Button{};
        addCommandButton.Width(30);
        addCommandButton.Height(30);
        addCommandButton.MinWidth(30);
        addCommandButton.MinHeight(30);
        addCommandButton.Padding(ThicknessHelper::FromLengths(0, 0, 0, 0));
        addCommandButton.VerticalAlignment(VerticalAlignment::Center);
        auto addCommandIcon = SymbolIcon{};
        addCommandIcon.Symbol(Symbol::Add);
        addCommandButton.Content(addCommandIcon);
        Grid::SetColumn(addCommandButton, 1);
        ToolTipService::SetToolTip(addCommandButton, box_value(L"添加命令窗口"));
        addCommandButton.IsEnabled(_workspaceEditorEditMode && (node.Commands.empty() || node.Commands.size() < 3));
        addCommandButton.Click([weakThis{ get_weak() }, nodeIndex](auto&&, auto&&) {
            if (auto self{ weakThis.get() })
            {
                if (auto* workspace = self->_SelectedWorkspaceForEditing(); workspace && nodeIndex < workspace->Nodes.size())
                {
                    auto& target = workspace->Nodes.at(nodeIndex);
                    if (target.Commands.empty())
                    {
                        target.Commands.emplace_back(Microsoft::Terminal::Settings::Model::implementation::WorkspaceNodeCommand{
                            target.Id + L":legacy-command", target.Icon, target.Name, target.StartupAction });
                    }
                    if (target.Commands.size() < 3)
                    {
                        target.Commands.emplace_back(Microsoft::Terminal::Settings::Model::implementation::WorkspaceNodeCommand{
                            target.Id + L":command-" + std::to_wstring(target.Commands.size() + 1), {}, L"未命名命令", {} });
                        target.MultiWindowPreference.SplitWeights.assign(target.Commands.size(), 1.0 / target.Commands.size());
                        self->_workspaceExtension->WorkspaceDefinitionsDirty() = true;
                        self->_RebuildWorkspaceManagerTab();
                    }
                }
            }
        });
        commandHeader.Children().Append(addCommandButton);
        nodeRoot.Children().Append(commandHeader);

        const auto commands = node.Commands.empty() ? std::vector<Microsoft::Terminal::Settings::Model::implementation::WorkspaceNodeCommand>{
            { node.Id + L":legacy-command", node.Icon, node.Name, node.StartupAction } } : node.Commands;
        auto commandList = ListView{};
        commandList.CanDragItems(_workspaceEditorEditMode);
        commandList.CanReorderItems(_workspaceEditorEditMode);
        commandList.AllowDrop(_workspaceEditorEditMode);
        commandList.SelectionMode(ListViewSelectionMode::None);
        for (size_t commandIndex = 0; commandIndex < commands.size(); ++commandIndex)
        {
            const auto& command = commands.at(commandIndex);
            auto row = Grid{};
            row.Padding(ThicknessHelper::FromLengths(8, 6, 8, 6));
            row.HorizontalAlignment(HorizontalAlignment::Stretch);
            auto iconColumn = ColumnDefinition{};
            iconColumn.Width(GridLengthHelper::FromPixels(52));
            row.ColumnDefinitions().Append(iconColumn);
            auto nameColumn = ColumnDefinition{};
            nameColumn.Width(GridLengthHelper::FromPixels(180));
            row.ColumnDefinitions().Append(nameColumn);
            auto gapColumn = ColumnDefinition{};
            gapColumn.Width(GridLengthHelper::FromPixels(8));
            row.ColumnDefinitions().Append(gapColumn);
            row.ColumnDefinitions().Append(ColumnDefinition{});
            auto removeColumn = ColumnDefinition{};
            removeColumn.Width(GridLengthHelper::Auto());
            row.ColumnDefinitions().Append(removeColumn);
            auto iconButton = Button{};
            iconButton.Width(40);
            iconButton.Height(40);
            iconButton.Padding(ThicknessHelper::FromLengths(0, 0, 0, 0));
            if (auto icon = _CreateNewTabFlyoutIcon(winrt::hstring{ command.Icon }))
            {
                iconButton.Content(icon);
            }
            else
            {
                auto fallbackIcon = SymbolIcon{};
                fallbackIcon.Symbol(Symbol::Page);
                iconButton.Content(fallbackIcon);
            }
            iconButton.IsEnabled(_workspaceEditorEditMode);
            ToolTipService::SetToolTip(iconButton, box_value(L"选择命令图标"));
            iconButton.Click([weakThis{ get_weak() }, nodeIndex, commandIndex](auto&&, auto&&) {
                [](winrt::weak_ref<TerminalPage> weakThis, const size_t nodeIndex, const size_t commandIndex) -> safe_void_coroutine {
                    if (auto self{ weakThis.get() }; self && self->_workspaceExtension)
                    {
                        std::wstring initial;
                        if (const auto* workspace = self->_SelectedWorkspaceForEditing(); workspace && nodeIndex < workspace->Nodes.size())
                        {
                            const auto& target = workspace->Nodes.at(nodeIndex);
                            if (commandIndex < target.Commands.size())
                            {
                                initial = target.Commands.at(commandIndex).Icon;
                            }
                        }
                        const auto selected = co_await self->_workspaceExtension->PickWorkspaceManagerIcon(std::move(initial), std::nullopt);
                        if (!selected.empty())
                        {
                            if (auto* workspace = self->_SelectedWorkspaceForEditing(); workspace && nodeIndex < workspace->Nodes.size() && commandIndex < workspace->Nodes.at(nodeIndex).Commands.size())
                            {
                                workspace->Nodes.at(nodeIndex).Commands.at(commandIndex).Icon = selected.c_str();
                                self->_workspaceExtension->WorkspaceDefinitionsDirty() = true;
                                self->_RebuildWorkspaceManagerTab();
                            }
                        }
                    }
                }(weakThis, nodeIndex, commandIndex);
            });
            row.Children().Append(iconButton);
            auto nameBox = TextBox{};
            nameBox.PlaceholderText(L"名字，例如 Codex");
            nameBox.Text(command.Name);
            nameBox.MinWidth(0);
            nameBox.IsEnabled(_workspaceEditorEditMode);
            auto commandBox = TextBox{};
            commandBox.PlaceholderText(L"启动命令，例如 codex --resume（可为空）");
            commandBox.Text(command.Command);
            commandBox.MinWidth(160);
            commandBox.HorizontalAlignment(HorizontalAlignment::Stretch);
            commandBox.IsEnabled(_workspaceEditorEditMode);
            const auto commit = [weakThis{ get_weak() }, nodeIndex, commandIndex, nameBox, commandBox](auto&&, auto&&) {
                if (auto self{ weakThis.get() })
                {
                    if (auto* workspace = self->_SelectedWorkspaceForEditing(); workspace && nodeIndex < workspace->Nodes.size())
                    {
                        auto& target = workspace->Nodes.at(nodeIndex);
                        if (target.Commands.empty())
                        {
                            target.Commands.emplace_back(Microsoft::Terminal::Settings::Model::implementation::WorkspaceNodeCommand{
                                target.Id + L":legacy-command", target.Icon, target.Name, target.StartupAction });
                        }
                        if (commandIndex < target.Commands.size())
                        {
                            target.Commands.at(commandIndex).Name = nameBox.Text().c_str();
                            target.Commands.at(commandIndex).Command = commandBox.Text().c_str();
                            self->_workspaceExtension->WorkspaceDefinitionsDirty() = true;
                        }
                    }
                }
            };
            nameBox.LostFocus(commit);
            commandBox.LostFocus(commit);
            Grid::SetColumn(nameBox, 1);
            row.Children().Append(nameBox);
            Grid::SetColumn(commandBox, 3);
            row.Children().Append(commandBox);
            auto remove = Button{};
            auto removeIcon = SymbolIcon{};
            removeIcon.Symbol(Symbol::Delete);
            remove.Content(removeIcon);
            remove.IsEnabled(_workspaceEditorEditMode && commands.size() > 1);
            Grid::SetColumn(remove, 4);
            ToolTipService::SetToolTip(remove, box_value(L"删除命令窗口"));
            remove.Click([weakThis{ get_weak() }, nodeIndex, commandIndex](auto&&, auto&&) {
                if (auto self{ weakThis.get() })
                {
                    if (auto* workspace = self->_SelectedWorkspaceForEditing(); workspace && nodeIndex < workspace->Nodes.size())
                    {
                        auto& target = workspace->Nodes.at(nodeIndex);
                        if (commandIndex < target.Commands.size() && target.Commands.size() > 1)
                        {
                            target.Commands.erase(target.Commands.begin() + commandIndex);
                            target.MultiWindowPreference.SplitWeights.assign(target.Commands.size(), 1.0 / target.Commands.size());
                            self->_workspaceExtension->WorkspaceDefinitionsDirty() = true;
                            self->_RebuildWorkspaceManagerTab();
                        }
                    }
                }
            });
            row.Children().Append(remove);
            auto item = ListViewItem{};
            applyWorkspaceStyle(item, L"WorkspaceNodeOrderItemStyle");
            item.HorizontalContentAlignment(HorizontalAlignment::Stretch);
            item.Tag(box_value(command.Id));
            item.Content(row);
            commandList.Items().Append(item);
        }
        commandList.DragItemsCompleted([weakThis{ get_weak() }, nodeIndex, commandList](auto&&, auto&&) {
            if (auto self{ weakThis.get() })
            {
                if (auto* workspace = self->_SelectedWorkspaceForEditing(); workspace && nodeIndex < workspace->Nodes.size())
                {
                    auto& target = workspace->Nodes.at(nodeIndex);
                    if (target.Commands.empty())
                    {
                        target.Commands.emplace_back(Microsoft::Terminal::Settings::Model::implementation::WorkspaceNodeCommand{
                            target.Id + L":legacy-command", target.Icon, target.Name, target.StartupAction });
                    }
                    std::vector<Microsoft::Terminal::Settings::Model::implementation::WorkspaceNodeCommand> ordered;
                    std::vector<double> weights;
                    ordered.reserve(commandList.Items().Size());
                    weights.reserve(commandList.Items().Size());
                    auto currentWeights = target.MultiWindowPreference.SplitWeights;
                    if (currentWeights.size() != target.Commands.size()) currentWeights.assign(target.Commands.size(), 1.0 / target.Commands.size());
                    for (uint32_t itemIndex = 0; itemIndex < commandList.Items().Size(); ++itemIndex)
                    {
                        const auto id = winrt::unbox_value<winrt::hstring>(commandList.Items().GetAt(itemIndex).as<ListViewItem>().Tag());
                        const auto found = std::find_if(target.Commands.begin(), target.Commands.end(), [&](const auto& command) { return command.Id == id.c_str(); });
                        if (found != target.Commands.end())
                        {
                            const auto sourceIndex = static_cast<size_t>(std::distance(target.Commands.begin(), found));
                            ordered.emplace_back(*found);
                            weights.emplace_back(currentWeights[sourceIndex]);
                        }
                    }
                    if (ordered.size() == target.Commands.size())
                    {
                        target.Commands = std::move(ordered);
                        target.MultiWindowPreference.SplitWeights = std::move(weights);
                        self->_workspaceExtension->WorkspaceDefinitionsDirty() = true;
                        self->_RebuildWorkspaceManagerTab();
                    }
                }
            }
        });
        nodeRoot.Children().Append(commandList);

        if (commands.size() >= 2)
        {
            auto multiWindowTitle = makeSectionTitle(L"多窗口展示");
            nodeRoot.Children().Append(multiWindowTitle);
            auto modeBox = ComboBox{};
            modeBox.Items().Append(box_value(L"左右分隔"));
            modeBox.Items().Append(box_value(L"Tab"));
            modeBox.SelectedIndex(node.MultiWindowPreference.DisplayMode == Microsoft::Terminal::Settings::Model::implementation::WorkspaceWindowDisplayMode::Tab ? 1 : 0);
            modeBox.IsEnabled(_workspaceEditorEditMode);
            modeBox.SelectionChanged([weakThis{ get_weak() }, nodeIndex](auto&& sender, auto&&) {
                if (auto self{ weakThis.get() })
                {
                    if (auto* workspace = self->_SelectedWorkspaceForEditing(); workspace && nodeIndex < workspace->Nodes.size())
                    {
                        workspace->Nodes.at(nodeIndex).MultiWindowPreference.DisplayMode = sender.as<ComboBox>().SelectedIndex() == 1 ?
                            Microsoft::Terminal::Settings::Model::implementation::WorkspaceWindowDisplayMode::Tab :
                            Microsoft::Terminal::Settings::Model::implementation::WorkspaceWindowDisplayMode::Split;
                        self->_workspaceExtension->WorkspaceDefinitionsDirty() = true;
                        self->_RebuildWorkspaceManagerTab();
                    }
                }
            });
            nodeRoot.Children().Append(makeWorkspaceSetting(L"展示方式", modeBox));

            if (node.MultiWindowPreference.DisplayMode == Microsoft::Terminal::Settings::Model::implementation::WorkspaceWindowDisplayMode::Tab)
            {
                auto placementBox = ComboBox{};
                placementBox.Items().Append(box_value(L"左上（图标加文字）"));
                placementBox.Items().Append(box_value(L"右上（竖排图标）"));
                placementBox.Items().Append(box_value(L"右下（竖排图标）"));
                placementBox.SelectedIndex(static_cast<int32_t>(node.MultiWindowPreference.TabPlacement));
                placementBox.IsEnabled(_workspaceEditorEditMode);
                placementBox.SelectionChanged([weakThis{ get_weak() }, nodeIndex](auto&& sender, auto&&) {
                    if (auto self{ weakThis.get() })
                    {
                        if (auto* workspace = self->_SelectedWorkspaceForEditing(); workspace && nodeIndex < workspace->Nodes.size())
                        {
                            workspace->Nodes.at(nodeIndex).MultiWindowPreference.TabPlacement = static_cast<Microsoft::Terminal::Settings::Model::implementation::WorkspaceTabPlacement>(sender.as<ComboBox>().SelectedIndex());
                            self->_workspaceExtension->WorkspaceDefinitionsDirty() = true;
                        }
                    }
                });
                nodeRoot.Children().Append(makeWorkspaceSetting(L"Tab 位置", placementBox));
            }
            else
            {
                auto weights = node.MultiWindowPreference.SplitWeights;
                if (weights.size() != commands.size()) weights.assign(commands.size(), 1.0 / commands.size());
                auto allocation = Grid{};
                allocation.Height(46);
                allocation.Width(500);
                const auto columns = std::make_shared<std::vector<ColumnDefinition>>();
                const auto labels = std::make_shared<std::vector<TextBlock>>();
                for (size_t i = 0; i < weights.size(); ++i)
                {
                    auto column = ColumnDefinition{};
                    column.Width(GridLengthHelper::FromValueAndType(weights[i], GridUnitType::Star));
                    allocation.ColumnDefinitions().Append(column);
                    columns->emplace_back(column);
                    auto label = TextBlock{};
                    label.Text(winrt::to_hstring(static_cast<int>(weights[i] * 100 + .5)) + L"%");
                    label.HorizontalAlignment(HorizontalAlignment::Center);
                    label.VerticalAlignment(VerticalAlignment::Center);
                    Grid::SetColumn(label, static_cast<int>(i * 2));
                    allocation.Children().Append(label);
                    labels->emplace_back(label);
                    if (i + 1 < weights.size())
                    {
                        auto dividerColumn = ColumnDefinition{};
                        dividerColumn.Width(GridLengthHelper::FromPixels(10));
                        allocation.ColumnDefinitions().Append(dividerColumn);
                        auto divider = Border{};
                        divider.Width(10);
                        divider.Background(SolidColorBrush{ Colors::Gray() });
                        divider.HorizontalAlignment(HorizontalAlignment::Center);
                        Grid::SetColumn(divider, static_cast<int>(i * 2 + 1));
                        struct Drag { bool Active{}; double X{}; double Left{}; double Combined{}; };
                        const auto drag = std::make_shared<Drag>();
                        divider.PointerPressed([allocation, divider, drag](auto&&, const auto& args) {
                            divider.CapturePointer(args.Pointer());
                            drag->Active = true;
                            drag->X = args.GetCurrentPoint(allocation).Position().X;
                        });
                        divider.PointerMoved([weakThis{ get_weak() }, allocation, divider, columns, labels, drag, nodeIndex, i](auto&&, const auto& args) {
                            if (!drag->Active) return;
                            if (auto self{ weakThis.get() })
                            {
                                if (auto* workspace = self->_SelectedWorkspaceForEditing(); workspace && nodeIndex < workspace->Nodes.size())
                                {
                                    auto& current = workspace->Nodes.at(nodeIndex).MultiWindowPreference.SplitWeights;
                                    if (current.size() != labels->size()) current.assign(labels->size(), 1.0 / labels->size());
                                    if (drag->Combined == 0) { drag->Left = current[i]; drag->Combined = current[i] + current[i + 1]; }
                                    const auto width = std::max(1.0, allocation.ActualWidth());
                                    auto left = drag->Left + (args.GetCurrentPoint(allocation).Position().X - drag->X) / width;
                                    left = std::round(left / .05) * .05;
                                    left = std::clamp(left, .05, drag->Combined - .05);
                                    current[i] = left; current[i + 1] = drag->Combined - left;
                                    for (size_t j = 0; j < current.size(); ++j) { columns->at(j).Width(GridLengthHelper::FromValueAndType(current[j], GridUnitType::Star)); labels->at(j).Text(winrt::to_hstring(static_cast<int>(current[j] * 100 + .5)) + L"%"); }
                                    self->_workspaceExtension->WorkspaceDefinitionsDirty() = true;
                                }
                            }
                        });
                        divider.PointerReleased([divider, drag](auto&&, const auto& args) { drag->Active = false; drag->Combined = 0; divider.ReleasePointerCapture(args.Pointer()); });
                        allocation.Children().Append(divider);
                    }
                }
                nodeRoot.Children().Append(makeWorkspaceSetting(L"大小分配", allocation));
            }
        }

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
                const auto resolvedColorText = self->_workspaceExtension->WorkspaceManagerNodeTabColorPreview(nodeIndex);
                {
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
            tabColorPreviewRow.Tapped([weakThis{ get_weak() }, nodeIndex, applyNodeColorPreview, tabColorPreviewRow](auto&&, auto&&) {
                if (auto self{ weakThis.get() })
                {
                    auto picker = winrt::TerminalApp::ColorPickupFlyout{};
                    if (const auto color = _parseWorkspaceColor(self->_workspaceExtension->WorkspaceManagerNodeTabColor(nodeIndex).c_str()))
                    {
                        picker.Color(*color);
                    }
                    picker.ColorSelected([weakThis, nodeIndex, applyNodeColorPreview](const auto color) {
                        if (auto strong{ weakThis.get() })
                        {
                            wchar_t text[8]{};
                            swprintf_s(text, L"#%02X%02X%02X", color.R, color.G, color.B);
                            if (strong->_workspaceExtension->ApplyWorkspaceManagerNodeTabColor(nodeIndex, text))
                            {
                                applyNodeColorPreview();
                            }
                        }
                    });
                    picker.ShowAt(tabColorPreviewRow);
                }
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
                    if (self->_workspaceExtension->RotateWorkspaceManagerNodeTabColor(nodeIndex))
                    {
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
                    if (const auto toggle = sender.try_as<WUX::Controls::ToggleSwitch>())
                    {
                        self->_workspaceExtension->UpdateWorkspaceManagerNodeBool(nodeIndex, terminal::workspace::WorkspaceManagerNodeBoolField::ShowInputPanel, toggle.IsOn());
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
                    if (const auto toggle = sender.try_as<WUX::Controls::ToggleSwitch>())
                    {
                        self->_workspaceExtension->UpdateWorkspaceManagerNodeBool(nodeIndex, terminal::workspace::WorkspaceManagerNodeBoolField::UseNodeNameAsTabTitle, toggle.IsOn());
                    }
                }
            });
        }
        appendToggleRow(RS_(L"WorkspaceEditor_UseNodeNameAsTabTitle"), useDefinedTitleToggle);
        root.Children().Append(nodeRoot);
    }
