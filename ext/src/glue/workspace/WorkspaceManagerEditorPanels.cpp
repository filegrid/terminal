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
                    std::vector<WorkspaceNode> orderedNodes;
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
                        std::unordered_set<std::wstring> usedColors;
                        for (const auto& other : self->_workspaceExtension->WorkspaceEditorManager().Workspaces())
                        {
                            if (&other != current && !other.BackgroundColor.empty())
                            {
                                usedColors.emplace(other.BackgroundColor);
                            }
                        }
                        current->BackgroundColor = PickWorkspacePaletteColor(
                            usedColors,
                            self->_workspaceExtension->WorkspaceEditorManager().Workspaces().size(),
                            current->BackgroundColor);
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
            workspaceIconButton.Click([weakThis{ get_weak() }](auto&&, auto&&) {
                if (auto self{ weakThis.get() })
                {
                    self->_ShowWorkspaceManagerWorkspaceIconPicker();
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
                if (const auto* current = self->_SelectedWorkspaceForEditing(); current && nodeIndex < current->Nodes.size())
                {
                    const auto& currentNode = current->Nodes.at(nodeIndex);
                    WUX::Controls::IconElement previewIcon{ nullptr };
                    if (!currentNode.Icon.empty())
                    {
                        previewIcon = self->_CreateNewTabFlyoutIcon(winrt::hstring{ currentNode.Icon });
                    }
                    if (!previewIcon)
                    {
                        if (const auto guid = _tryParseGuid(currentNode.ProfileGuid); guid.has_value())
                        {
                            if (const auto profile = self->_settings.FindProfile(*guid))
                            {
                                previewIcon = self->_CreateNewTabFlyoutIcon(profile.Icon().Resolved());
                            }
                        }
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
                    self->_ShowWorkspaceManagerNodeIconPicker(*stableNodeIndex);
                }
            });
        }
        nodeRoot.Children().Append(makeWorkspaceSetting(L"图标", iconPanel));

        addNodeTextBox(RS_(L"WorkspaceEditor_StartupDirectory").c_str(), node.StartupDirectory, [weakThis{ get_weak() }, nodeIndex](auto&& sender, auto&&) {
            if (auto self{ weakThis.get() })
            {
                if (auto* current = self->_SelectedWorkspaceForEditing(); current && nodeIndex < current->Nodes.size())
                {
                    current->Nodes.at(nodeIndex).StartupDirectory = sender.as<TextBox>().Text().c_str();
                    self->_workspaceExtension->WorkspaceDefinitionsDirty() = true;
                }
            }
        });
        addNodeTextBox(L"启动命令", node.StartupAction, [weakThis{ get_weak() }, nodeIndex](auto&& sender, auto&&) {
            if (auto self{ weakThis.get() })
            {
                if (auto* current = self->_SelectedWorkspaceForEditing(); current && nodeIndex < current->Nodes.size())
                {
                    current->Nodes.at(nodeIndex).StartupAction = sender.as<TextBox>().Text().c_str();
                    self->_workspaceExtension->WorkspaceDefinitionsDirty() = true;
                }
            }
        });

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
                        std::unordered_set<std::wstring> usedColors;
                        for (size_t i = 0; i < current->Nodes.size(); ++i)
                        {
                            if (i != nodeIndex && !current->Nodes.at(i).TabColor.empty())
                            {
                                usedColors.emplace(current->Nodes.at(i).TabColor);
                            }
                        }
                        current->Nodes.at(nodeIndex).TabColor = PickWorkspacePaletteColor(usedColors, nodeIndex, previousColor);
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
