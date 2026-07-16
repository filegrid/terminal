    UIElement TerminalPage::_BuildWorkspaceManagerContent()
    {
        const auto marginBottom = [](const double bottom) {
            return WUX::ThicknessHelper::FromLengths(0, 0, 0, bottom);
        };
        const auto sectionBorder = [&]() {
            auto border = Border{};
            border.BorderBrush(SolidColorBrush{ Colors::DarkGray() });
            border.BorderThickness(WUX::ThicknessHelper::FromLengths(1, 1, 1, 1));
            border.Padding(WUX::ThicknessHelper::FromLengths(16, 16, 16, 16));
            border.Margin(marginBottom(16));
            return border;
        };

        const auto makeSectionTitle = [&](const winrt::hstring& text) {
            auto title = TextBlock{};
            title.Text(text);
            title.FontSize(18);
            title.FontWeight(FontWeights::SemiBold());
            title.Margin(marginBottom(8));
            return title;
        };
        const auto workspaceGeneralNavTag = [](const size_t workspaceIndex) {
            return 1000 + gsl::narrow_cast<int32_t>(workspaceIndex * 100);
        };
        const auto workspaceNodeNavTag = [](const size_t workspaceIndex, const size_t nodeIndex) {
            return 1000 + gsl::narrow_cast<int32_t>(workspaceIndex * 100) + 10 + gsl::narrow_cast<int32_t>(nodeIndex);
        };
        const auto workspaceIndexFromNavTag = [](const int32_t navTag) {
            return gsl::narrow_cast<size_t>((navTag - 1000) / 100);
        };
        const auto workspaceSubSelectionFromNavTag = [](const int32_t navTag) {
            return (navTag - 1000) % 100;
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
        if (_workspaceManagerNavSelection >= 1000)
        {
            const auto workspaceIndex = workspaceIndexFromNavTag(_workspaceManagerNavSelection);
            if (workspaces.empty())
            {
                _workspaceManagerNavSelection = 0;
            }
            else if (workspaceIndex >= workspaces.size())
            {
                _workspaceManagerNavSelection = workspaceGeneralNavTag(std::min(_workspaceEditorSelectedIndex, workspaces.size() - 1));
            }
        }

        auto behaviorItem = MUX::Controls::NavigationViewItem{};
        behaviorItem.Content(box_value(RS_(L"WorkspaceEditor_BehaviorTitle")));
        behaviorItem.Tag(box_value(0));
        {
            WUX::Controls::SymbolIcon icon{};
            icon.Symbol(WUX::Controls::Symbol::Setting);
            behaviorItem.Icon(icon);
        }
        nav.MenuItems().Append(behaviorItem);

        nav.MenuItems().Append(MUX::Controls::NavigationViewItemHeader{});
        if (const auto headerItem = nav.MenuItems().GetAt(nav.MenuItems().Size() - 1).try_as<MUX::Controls::NavigationViewItemHeader>())
        {
            headerItem.Content(box_value(RS_(L"WorkspaceEditor_WorkspaceLabel")));
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
            item.IsExpanded(_workspaceManagerNavSelection >= 1000 && workspaceIndexFromNavTag(_workspaceManagerNavSelection) == index);

            WUX::Controls::SymbolIcon icon{};
            icon.Symbol(WUX::Controls::Symbol::OpenFile);
            item.Icon(icon);

            auto generalItem = MUX::Controls::NavigationViewItem{};
            generalItem.Content(box_value(RS_(L"WorkspaceEditor_GeneralNav")));
            generalItem.Tag(box_value(workspaceGeneralNavTag(index)));
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
                nodeItem.Content(box_value(winrt::hstring{ node.Name.empty() ? node.Id : node.Name }));
                nodeItem.Tag(box_value(workspaceNodeNavTag(index, nodeIndex)));
                {
                    WUX::Controls::SymbolIcon childIcon{};
                    childIcon.Symbol(WUX::Controls::Symbol::Page);
                    nodeItem.Icon(childIcon);
                }
                item.MenuItems().Append(nodeItem);
                nodeItems.emplace_back(nodeItem);
            }
            workspaceNodeItems.emplace_back(std::move(nodeItems));

            nav.MenuItems().Append(item);
        }

        auto openYamlItem = MUX::Controls::NavigationViewItem{};
        openYamlItem.Content(box_value(RS_(L"WorkspaceEditor_OpenYaml")));
        openYamlItem.Tag(box_value(-1));
        openYamlItem.SelectsOnInvoked(false);
        {
            WUX::Controls::SymbolIcon icon{};
            icon.Symbol(WUX::Controls::Symbol::Document);
            openYamlItem.Icon(icon);
        }
        nav.FooterMenuItems().Append(openYamlItem);

        if (_workspaceManagerNavSelection == 0)
        {
            nav.SelectedItem(behaviorItem);
        }
        else if (_workspaceManagerNavSelection >= 1000)
        {
            const auto workspaceIndex = workspaceIndexFromNavTag(_workspaceManagerNavSelection);
            const auto workspaceSubSelection = workspaceSubSelectionFromNavTag(_workspaceManagerNavSelection);
            if (workspaceIndex < workspaceGeneralItems.size())
            {
                if (workspaceSubSelection < 10)
                {
                    nav.SelectedItem(workspaceGeneralItems[workspaceIndex]);
                }
                else
                {
                    const auto nodeIndex = gsl::narrow_cast<size_t>(workspaceSubSelection - 10);
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
                        if (value == -1)
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
                    if (value >= 1000)
                    {
                        self->_SetSelectedWorkspaceIndex(gsl::narrow_cast<size_t>((value - 1000) / 100));
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
        root.HorizontalAlignment(HorizontalAlignment::Stretch);
        root.Margin(WUX::ThicknessHelper::FromLengths(16, 0, 16, 16));
        scrollViewer.Content(root);
        contentGrid.Children().Append(scrollViewer);

        if (_workspaceManagerNavSelection == 0)
        {
            root.Children().Append(makeSectionTitle(RS_(L"WorkspaceEditor_BehaviorTitle")));

            auto behaviorBorder = sectionBorder();
            auto behaviorPanel = StackPanel{};
            behaviorBorder.Child(behaviorPanel);

            auto launchToggle = CheckBox{};
            launchToggle.Content(box_value(RS_(L"WorkspaceOpenInNewWindow")));
            launchToggle.IsChecked(Microsoft::Terminal::Settings::Model::ApplicationState::SharedInstance().OpenInNewWindow());
            launchToggle.Margin(marginBottom(8));
            launchToggle.Click([](auto&& sender, auto&&) {
                if (const auto toggle = sender.try_as<CheckBox>())
                {
                    const auto state = Microsoft::Terminal::Settings::Model::ApplicationState::SharedInstance();
                    state.OpenInNewWindow(toggle.IsChecked().GetBoolean());
                    state.Flush();
                }
            });
            behaviorPanel.Children().Append(launchToggle);

            root.Children().Append(behaviorBorder);
        }
        else
        {
            auto* workspace = _SelectedWorkspaceForEditing();
            const auto selectedWorkspaceSubSelection = workspaceSubSelectionFromNavTag(_workspaceManagerNavSelection);
            if (workspace == nullptr)
            {
                auto empty = TextBlock{};
                empty.Text(RS_(L"WorkspaceEditor_NoneSaved"));
                empty.TextWrapping(TextWrapping::Wrap);
                root.Children().Append(empty);
            }
            else
            {
                root.Children().Append(makeSectionTitle(winrt::hstring{ _WorkspaceDisplayName(*workspace) }));

                const auto addLabeledTextBox = [&](StackPanel& panel, const wchar_t* labelText, const std::wstring& initialValue, const auto& onChanged, const bool readOnly, const bool multiline = false) {
                    auto label = TextBlock{};
                    label.Text(labelText);
                    label.Margin(marginBottom(4));
                    panel.Children().Append(label);

                    auto textBox = TextBox{};
                    textBox.Text(initialValue);
                    textBox.IsReadOnly(readOnly);
                    textBox.AcceptsReturn(multiline);
                    textBox.TextWrapping(multiline ? TextWrapping::Wrap : TextWrapping::NoWrap);
                    textBox.Margin(marginBottom(12));
                    if (!readOnly)
                    {
                        textBox.TextChanged(onChanged);
                    }
                    panel.Children().Append(textBox);
                };

                if (selectedWorkspaceSubSelection < 10)
                {
                    auto generalBorder = sectionBorder();
                    auto generalPanel = StackPanel{};
                    generalBorder.Child(generalPanel);

                    auto generalHeader = StackPanel{};
                    generalHeader.Orientation(Orientation::Horizontal);
                    generalHeader.Margin(marginBottom(8));

                    auto generalTitle = TextBlock{};
                    generalTitle.Text(RS_(L"WorkspaceEditor_GeneralSection"));
                    generalTitle.FontSize(18);
                    generalTitle.FontWeight(FontWeights::SemiBold());
                    generalTitle.VerticalAlignment(VerticalAlignment::Center);
                    generalTitle.Margin(WUX::ThicknessHelper::FromLengths(0, 0, 8, 0));
                    generalHeader.Children().Append(generalTitle);

                    if (_workspaceEditorEditMode)
                    {
                        auto deleteWorkspaceButton = Button{};
                        deleteWorkspaceButton.Content(box_value(RS_(L"WorkspaceEditor_DeleteWorkspaceButton")));
                        deleteWorkspaceButton.Click([weakThis{ get_weak() }](auto&&, auto&&) {
                            if (auto self{ weakThis.get() })
                            {
                                self->_DeleteSelectedWorkspaceDefinition();
                                self->_RebuildWorkspaceManagerTab();
                            }
                        });
                        generalHeader.Children().Append(deleteWorkspaceButton);
                    }

                    generalPanel.Children().Append(generalHeader);
                    addLabeledTextBox(generalPanel, RS_(L"WorkspaceEditor_WorkspaceName").c_str(), workspace->Name, [weakThis{ get_weak() }](auto&& sender, auto&&) {
                        if (auto self{ weakThis.get() })
                        {
                            if (auto* current = self->_SelectedWorkspaceForEditing())
                            {
                                current->Name = sender.as<TextBox>().Text().c_str();
                                self->_workspaceExtension->WorkspaceDefinitionsDirty() = true;
                            }
                        }
                    }, !_workspaceEditorEditMode);
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

                    auto colorLabel = TextBlock{};
                    colorLabel.Text(RS_(L"WorkspaceEditor_BackgroundColor"));
                    colorLabel.Margin(marginBottom(4));
                    generalPanel.Children().Append(colorLabel);

                    auto colorPanel = StackPanel{};
                    colorPanel.Margin(marginBottom(12));
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
                            colorValue.Text(RS_(L"WorkspaceEditor_BackgroundColorAuto"));
                        }
                    };

                    applyWorkspaceColorPreview(workspace->BackgroundColor);
                    colorPreviewRow.Children().Append(colorPreview);
                    colorPreviewRow.Children().Append(colorValue);
                    colorPanel.Children().Append(colorPreviewRow);

                    if (_workspaceEditorEditMode)
                    {
                        auto colorButtons = StackPanel{};
                        colorButtons.Orientation(Orientation::Horizontal);
                        colorButtons.Spacing(8);

                        auto chooseColorButton = Button{};
                        chooseColorButton.Content(box_value(RS_(L"WorkspaceEditor_ChooseColor")));

                        auto clearColorButton = Button{};
                        clearColorButton.Content(box_value(RS_(L"WorkspaceEditor_ClearColor")));
                        clearColorButton.IsEnabled(!workspace->BackgroundColor.empty());

                        auto backgroundColorFlyout = winrt::make<ColorPickupFlyout>();
                        backgroundColorFlyout.ColorSelected([weakThis{ get_weak() }, applyWorkspaceColorPreview, clearColorButton](const winrt::Windows::UI::Color& color) {
                            if (auto self{ weakThis.get() })
                            {
                                if (auto* current = self->_SelectedWorkspaceForEditing())
                                {
                                    current->BackgroundColor = _workspaceColorToString(color);
                                    self->_workspaceExtension->WorkspaceDefinitionsDirty() = true;
                                    applyWorkspaceColorPreview(current->BackgroundColor);
                                    clearColorButton.IsEnabled(true);
                                }
                            }
                        });
                        backgroundColorFlyout.ColorCleared([weakThis{ get_weak() }, applyWorkspaceColorPreview, clearColorButton]() {
                            if (auto self{ weakThis.get() })
                            {
                                if (auto* current = self->_SelectedWorkspaceForEditing())
                                {
                                    current->BackgroundColor.clear();
                                    self->_workspaceExtension->WorkspaceDefinitionsDirty() = true;
                                    applyWorkspaceColorPreview(current->BackgroundColor);
                                    clearColorButton.IsEnabled(false);
                                }
                            }
                        });

                        chooseColorButton.Click([backgroundColorFlyout, chooseColorButton](auto&&, auto&&) {
                            backgroundColorFlyout.ShowAt(chooseColorButton);
                        });
                        clearColorButton.Click([weakThis{ get_weak() }, applyWorkspaceColorPreview, clearColorButton](auto&&, auto&&) {
                            if (auto self{ weakThis.get() })
                            {
                                if (auto* current = self->_SelectedWorkspaceForEditing())
                                {
                                    current->BackgroundColor.clear();
                                    self->_workspaceExtension->WorkspaceDefinitionsDirty() = true;
                                    applyWorkspaceColorPreview(current->BackgroundColor);
                                    clearColorButton.IsEnabled(false);
                                }
                            }
                        });

                        colorButtons.Children().Append(chooseColorButton);
                        colorButtons.Children().Append(clearColorButton);
                        colorPanel.Children().Append(colorButtons);
                    }

                    generalPanel.Children().Append(colorPanel);
                    root.Children().Append(generalBorder);
                }

                if (selectedWorkspaceSubSelection >= 10)
                {
                    const auto nodeIndex = gsl::narrow_cast<size_t>(selectedWorkspaceSubSelection - 10);
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

                        auto nodeBorder = sectionBorder();
                        auto nodeRoot = StackPanel{};
                        nodeBorder.Child(nodeRoot);

                        auto nodeHeader = StackPanel{};
                        nodeHeader.Orientation(Orientation::Horizontal);
                        nodeHeader.Margin(marginBottom(8));

                        auto nodeTitle = TextBlock{};
                        nodeTitle.Text(node.Name.empty() ? node.Id : node.Name);
                        nodeTitle.VerticalAlignment(VerticalAlignment::Center);
                        nodeTitle.FontWeight(FontWeights::SemiBold());
                        nodeTitle.Margin(WUX::ThicknessHelper::FromLengths(0, 0, 8, 0));
                        nodeHeader.Children().Append(nodeTitle);

                        if (_workspaceEditorEditMode)
                        {
                            auto deleteNodeButton = Button{};
                            deleteNodeButton.Content(box_value(RS_(L"WorkspaceEditor_DeleteNodeButton")));
                            deleteNodeButton.Click([weakThis{ get_weak() }, nodeIndex](auto&&, auto&&) {
                                if (auto self{ weakThis.get() })
                                {
                                    self->_DeleteWorkspaceNode(nodeIndex);
                                    self->_RebuildWorkspaceManagerTab();
                                }
                            });
                            nodeHeader.Children().Append(deleteNodeButton);
                        }

                        nodeRoot.Children().Append(nodeHeader);

                        const auto addNodeTextBox = [&](const wchar_t* labelText, const std::wstring& initialValue, const auto& onChanged, const bool multiline = false) {
                            auto label = TextBlock{};
                            label.Text(labelText);
                            label.Margin(marginBottom(4));
                            nodeRoot.Children().Append(label);

                            auto textBox = TextBox{};
                            textBox.Text(initialValue);
                            textBox.IsReadOnly(!_workspaceEditorEditMode);
                            textBox.AcceptsReturn(multiline);
                            textBox.TextWrapping(multiline ? TextWrapping::Wrap : TextWrapping::NoWrap);
                            textBox.Margin(marginBottom(8));
                            if (_workspaceEditorEditMode)
                            {
                                textBox.TextChanged(onChanged);
                            }
                            nodeRoot.Children().Append(textBox);
                        };

                        addNodeTextBox(RS_(L"WorkspaceEditor_NodeName").c_str(), node.Name, [weakThis{ get_weak() }, nodeIndex](auto&& sender, auto&&) {
                            if (auto self{ weakThis.get() })
                            {
                                if (auto* current = self->_SelectedWorkspaceForEditing(); current && nodeIndex < current->Nodes.size())
                                {
                                    current->Nodes.at(nodeIndex).Name = sender.as<TextBox>().Text().c_str();
                                    self->_workspaceExtension->WorkspaceDefinitionsDirty() = true;
                                    self->_ApplyWorkspaceNodeTitlePolicy(nodeIndex);
                                }
                            }
                        });

                        auto showTabToggle = CheckBox{};
                        showTabToggle.Content(box_value(RS_(L"WorkspaceEditor_ShowTab")));
                        showTabToggle.IsChecked(node.ShowTab);
                        showTabToggle.Margin(marginBottom(8));
                        showTabToggle.IsEnabled(_workspaceEditorEditMode);
                        if (_workspaceEditorEditMode)
                        {
                            showTabToggle.Click([weakThis{ get_weak() }, nodeIndex](auto&& sender, auto&&) {
                                if (auto self{ weakThis.get() })
                                {
                                    if (auto* current = self->_SelectedWorkspaceForEditing(); current && nodeIndex < current->Nodes.size())
                                    {
                                        if (const auto toggle = sender.try_as<CheckBox>())
                                        {
                                            current->Nodes.at(nodeIndex).ShowTab = toggle.IsChecked().GetBoolean();
                                            self->_workspaceExtension->WorkspaceDefinitionsDirty() = true;
                                            self->_ApplyWorkspaceNodeLoadState(nodeIndex);
                                        }
                                    }
                                }
                            });
                        }
                        nodeRoot.Children().Append(showTabToggle);

                        auto profileLabel = TextBlock{};
                        profileLabel.Text(RS_(L"WorkspaceEditor_Source"));
                        profileLabel.Margin(marginBottom(4));
                        nodeRoot.Children().Append(profileLabel);

                        auto profilePicker = ComboBox{};
                        profilePicker.Margin(marginBottom(8));
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
                                    item.Content(box_value(node.ProfileGuid));
                                }
                            }
                            else
                            {
                                item.Content(box_value(node.ProfileGuid));
                            }
                            item.Tag(box_value(node.ProfileGuid));
                            profilePicker.Items().Append(item);
                            selectedProfileIndex = gsl::narrow_cast<int32_t>(profilePicker.Items().Size() - 1);
                        }

                        profilePicker.SelectedIndex(selectedProfileIndex);
                        if (_workspaceEditorEditMode)
                        {
                            profilePicker.SelectionChanged([weakThis{ get_weak() }, nodeIndex](auto&& sender, auto&&) {
                                if (auto self{ weakThis.get() })
                                {
                                    if (auto* current = self->_SelectedWorkspaceForEditing(); current && nodeIndex < current->Nodes.size())
                                    {
                                        if (const auto picker = sender.try_as<ComboBox>())
                                        {
                                            if (const auto item = picker.SelectedItem().try_as<ComboBoxItem>())
                                            {
                                                current->Nodes.at(nodeIndex).ProfileGuid = winrt::unbox_value<winrt::hstring>(item.Tag()).c_str();
                                                self->_workspaceExtension->WorkspaceDefinitionsDirty() = true;
                                            }
                                        }
                                    }
                                }
                            });
                        }
                        nodeRoot.Children().Append(profilePicker);

                        auto tabColorLabel = TextBlock{};
                        tabColorLabel.Text(RS_(L"WorkspaceEditor_TabColor"));
                        tabColorLabel.Margin(marginBottom(4));
                        nodeRoot.Children().Append(tabColorLabel);

                        auto tabColorPanel = StackPanel{};
                        tabColorPanel.Margin(marginBottom(12));
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
                                    self->_ApplyWorkspaceNodeTabColor(nodeIndex);
                                }
                            });
                        }
                        tabColorPreviewRow.Children().Append(tabColorPreview);
                        tabColorPreviewRow.Children().Append(tabColorValue);
                        tabColorPanel.Children().Append(tabColorPreviewRow);

                        if (_workspaceEditorEditMode)
                        {
                            auto tabColorButtons = StackPanel{};
                            tabColorButtons.Orientation(Orientation::Horizontal);
                            tabColorButtons.Spacing(8);

                            auto chooseTabColorButton = Button{};
                            chooseTabColorButton.Content(box_value(RS_(L"WorkspaceEditor_ChooseColor")));

                            auto clearTabColorButton = Button{};
                            clearTabColorButton.Content(box_value(RS_(L"WorkspaceEditor_ClearColor")));
                            clearTabColorButton.IsEnabled(!node.TabColor.empty());

                            auto tabColorFlyout = winrt::make<ColorPickupFlyout>();
                            tabColorFlyout.ColorSelected([weakThis{ get_weak() }, nodeIndex, applyNodeColorPreview, clearTabColorButton](const winrt::Windows::UI::Color& color) {
                                if (auto self{ weakThis.get() })
                                {
                                    if (auto* current = self->_SelectedWorkspaceForEditing(); current && nodeIndex < current->Nodes.size())
                                    {
                                        current->Nodes.at(nodeIndex).TabColor = _workspaceColorToString(color);
                                        self->_workspaceExtension->WorkspaceDefinitionsDirty() = true;
                                        applyNodeColorPreview();
                                        clearTabColorButton.IsEnabled(true);
                                        self->_ApplyWorkspaceNodeTabColor(nodeIndex);
                                    }
                                }
                            });
                            tabColorFlyout.ColorCleared([weakThis{ get_weak() }, nodeIndex, applyNodeColorPreview, clearTabColorButton]() {
                                if (auto self{ weakThis.get() })
                                {
                                    if (auto* current = self->_SelectedWorkspaceForEditing(); current && nodeIndex < current->Nodes.size())
                                    {
                                        current->Nodes.at(nodeIndex).TabColor.clear();
                                        self->_workspaceExtension->WorkspaceDefinitionsDirty() = true;
                                        applyNodeColorPreview();
                                        clearTabColorButton.IsEnabled(false);
                                        self->_ApplyWorkspaceNodeTabColor(nodeIndex);
                                    }
                                }
                            });

                            chooseTabColorButton.Click([tabColorFlyout, chooseTabColorButton](auto&&, auto&&) {
                                tabColorFlyout.ShowAt(chooseTabColorButton);
                            });
                            clearTabColorButton.Click([weakThis{ get_weak() }, nodeIndex, applyNodeColorPreview, clearTabColorButton](auto&&, auto&&) {
                                if (auto self{ weakThis.get() })
                                {
                                    if (auto* current = self->_SelectedWorkspaceForEditing(); current && nodeIndex < current->Nodes.size())
                                    {
                                        current->Nodes.at(nodeIndex).TabColor.clear();
                                        self->_workspaceExtension->WorkspaceDefinitionsDirty() = true;
                                        applyNodeColorPreview();
                                        clearTabColorButton.IsEnabled(false);
                                        self->_ApplyWorkspaceNodeTabColor(nodeIndex);
                                    }
                                }
                            });

                            tabColorButtons.Children().Append(chooseTabColorButton);
                            tabColorButtons.Children().Append(clearTabColorButton);
                            tabColorPanel.Children().Append(tabColorButtons);
                        }

                        nodeRoot.Children().Append(tabColorPanel);

                        addNodeTextBox(RS_(L"WorkspaceEditor_ConnectionReference").c_str(), node.ConnectionRef, [weakThis{ get_weak() }, nodeIndex](auto&& sender, auto&&) {
                            if (auto self{ weakThis.get() })
                            {
                                if (auto* current = self->_SelectedWorkspaceForEditing(); current && nodeIndex < current->Nodes.size())
                                {
                                    current->Nodes.at(nodeIndex).ConnectionRef = sender.as<TextBox>().Text().c_str();
                                    self->_workspaceExtension->WorkspaceDefinitionsDirty() = true;
                                }
                            }
                        });

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

                        addNodeTextBox(RS_(L"WorkspaceEditor_StartupCommandOrScript").c_str(), node.StartupAction, [weakThis{ get_weak() }, nodeIndex](auto&& sender, auto&&) {
                            if (auto self{ weakThis.get() })
                            {
                                if (auto* current = self->_SelectedWorkspaceForEditing(); current && nodeIndex < current->Nodes.size())
                                {
                                    current->Nodes.at(nodeIndex).StartupAction = sender.as<TextBox>().Text().c_str();
                                    self->_workspaceExtension->WorkspaceDefinitionsDirty() = true;
                                }
                            }
                        }, true);

                        addNodeTextBox(RS_(L"WorkspaceEditor_OperatingSystem").c_str(), node.OperatingSystem, [weakThis{ get_weak() }, nodeIndex](auto&& sender, auto&&) {
                            if (auto self{ weakThis.get() })
                            {
                                if (auto* current = self->_SelectedWorkspaceForEditing(); current && nodeIndex < current->Nodes.size())
                                {
                                    current->Nodes.at(nodeIndex).OperatingSystem = sender.as<TextBox>().Text().c_str();
                                    self->_workspaceExtension->WorkspaceDefinitionsDirty() = true;
                                }
                            }
                        });

                        addNodeTextBox(RS_(L"WorkspaceEditor_ShellType").c_str(), node.ShellType, [weakThis{ get_weak() }, nodeIndex](auto&& sender, auto&&) {
                            if (auto self{ weakThis.get() })
                            {
                                if (auto* current = self->_SelectedWorkspaceForEditing(); current && nodeIndex < current->Nodes.size())
                                {
                                    current->Nodes.at(nodeIndex).ShellType = sender.as<TextBox>().Text().c_str();
                                    self->_workspaceExtension->WorkspaceDefinitionsDirty() = true;
                                }
                            }
                        });

                        auto showInputPanelToggle = CheckBox{};
                        showInputPanelToggle.Content(box_value(RS_(L"WorkspaceEditor_ShowInputPanel")));
                        showInputPanelToggle.IsChecked(node.ShowInputPanel);
                        showInputPanelToggle.Margin(marginBottom(8));
                        showInputPanelToggle.IsEnabled(_workspaceEditorEditMode);
                        if (_workspaceEditorEditMode)
                        {
                            showInputPanelToggle.Click([weakThis{ get_weak() }, nodeIndex](auto&& sender, auto&&) {
                                if (auto self{ weakThis.get() })
                                {
                                    if (auto* current = self->_SelectedWorkspaceForEditing(); current && nodeIndex < current->Nodes.size())
                                    {
                                        if (const auto toggle = sender.try_as<CheckBox>())
                                        {
                                            current->Nodes.at(nodeIndex).ShowInputPanel = toggle.IsChecked().GetBoolean();
                                            self->_workspaceExtension->WorkspaceDefinitionsDirty() = true;
                                            self->_ApplyWorkspaceNodeInputVisibility(nodeIndex, current->Nodes.at(nodeIndex).ShowInputPanel);
                                        }
                                    }
                                }
                            });
                        }
                        nodeRoot.Children().Append(showInputPanelToggle);

                        auto useDefinedTitleToggle = CheckBox{};
                        useDefinedTitleToggle.Content(box_value(RS_(L"WorkspaceEditor_UseNodeNameAsTabTitle")));
                        useDefinedTitleToggle.IsChecked(node.UseNodeNameAsTabTitle);
                        useDefinedTitleToggle.Margin(marginBottom(8));
                        useDefinedTitleToggle.IsEnabled(_workspaceEditorEditMode);
                        if (_workspaceEditorEditMode)
                        {
                            useDefinedTitleToggle.Click([weakThis{ get_weak() }, nodeIndex](auto&& sender, auto&&) {
                                if (auto self{ weakThis.get() })
                                {
                                    if (auto* current = self->_SelectedWorkspaceForEditing(); current && nodeIndex < current->Nodes.size())
                                    {
                                        if (const auto toggle = sender.try_as<CheckBox>())
                                        {
                                            current->Nodes.at(nodeIndex).UseNodeNameAsTabTitle = toggle.IsChecked().GetBoolean();
                                            self->_workspaceExtension->WorkspaceDefinitionsDirty() = true;
                                            self->_ApplyWorkspaceNodeTitlePolicy(nodeIndex);
                                        }
                                    }
                                }
                            });
                        }
                        nodeRoot.Children().Append(useDefinedTitleToggle);

                        root.Children().Append(nodeBorder);
                    }
                }
            }
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
        saveButton.Style(Application::Current().Resources().Lookup(box_value(L"AccentButtonStyle")).try_as<winrt::Windows::UI::Xaml::Style>());
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
                if (!self->_workspaceExtension->WorkspaceEditorManager().Workspaces().empty())
                {
                    self->_workspaceExtension->WorkspaceManagerNavSelection() = 1000 + gsl::narrow_cast<int32_t>(self->_workspaceExtension->WorkspaceEditorSelectedIndex() * 100);
                }
                self->_RebuildWorkspaceManagerTab();
            }
        });
        footerButtons.Children().Append(resetButton);

        footer.Children().Append(footerButtons);
        contentGrid.Children().Append(footer);

        nav.Content(contentGrid);
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
