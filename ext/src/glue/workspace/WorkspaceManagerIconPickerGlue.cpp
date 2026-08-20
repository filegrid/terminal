#include "../chat/WorkspaceDiagnosticLog.h"
#include "winrt/Microsoft.Terminal.UI.h"

    namespace
    {
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

        static constexpr std::array<WorkspaceIconFamilyOption, 4> _workspaceIconFamilies{ {
            { L"color", L"彩色" },
            { L"outline", L"描边" },
            { L"duotone", L"双色" },
            { L"sharp", L"硬朗" },
        } };

        static constexpr std::array<WorkspaceIconSectionOption, 5> _workspaceIconSections{ {
            { L"numbers", L"数字 0-9", 10 },
            { L"letters", L"字母 A-Z", 26 },
            { L"daily", L"日常", 20 },
            { L"development", L"研发", 20 },
            { L"office", L"办公", 20 },
        } };

        static constexpr WorkspaceIconSectionOption _workspaceWindowsSection{ L"windows", L"Windows / OS", 20 };

        std::wstring _makeWorkspaceIconDescriptor(const std::wstring& family, const std::wstring& section, const uint32_t index)
        {
            return std::wstring{ L"workspace-icon://" } + family + L"/" + section + L"/" + std::to_wstring(index);
        }

        std::wstring _workspaceIconFamilyFromValue(const std::wstring_view iconValue)
        {
            static constexpr std::wstring_view prefix{ L"workspace-icon://" };
            if (!iconValue.starts_with(prefix))
            {
                return L"color";
            }

            const auto payload = iconValue.substr(prefix.size());
            const auto slash = payload.find(L'/');
            if (slash == std::wstring_view::npos)
            {
                return L"color";
            }

            return std::wstring{ payload.substr(0, slash) };
        }

        void _appendWorkspaceIconPickerSections(TerminalPage* const,
                                                const std::shared_ptr<std::wstring>& selectedIcon,
                                                const std::shared_ptr<std::wstring>& currentFamily,
                                                const StackPanel& iconSectionsPanel,
                                                const ContentDialog& dialog,
                                                const std::optional<size_t> nodeIndex)
        {
            iconSectionsPanel.Children().Clear();

            if (nodeIndex.has_value())
            {
                Json::Value payload{ Json::objectValue };
                payload["nodeIndex"] = *nodeIndex;
                terminal::workspacechat::AddDiagnosticTextFields(payload, "family", *currentFamily);
                std::ignore = terminal::workspacechat::AppendWorkspaceDiagnosticLog(L"workspace_icon_picker_rebuild_begin", payload);
            }

            const auto appendSection = [&](const WorkspaceIconSectionOption& section) {
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

                    const auto descriptor = _makeWorkspaceIconDescriptor(*currentFamily, section.key, iconIndex);
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
                    auto preview = winrt::Microsoft::Terminal::UI::IconPathConverter::IconWUX(winrt::hstring{ descriptor });
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

                if (nodeIndex.has_value())
                {
                    Json::Value sectionPayload{ Json::objectValue };
                    sectionPayload["nodeIndex"] = *nodeIndex;
                    terminal::workspacechat::AddDiagnosticTextFields(sectionPayload, "family", *currentFamily);
                    terminal::workspacechat::AddDiagnosticTextFields(sectionPayload, "section", section.key);
                    sectionPayload["iconCount"] = section.count;
                    sectionPayload["rowCount"] = rowCount;
                    std::ignore = terminal::workspacechat::AppendWorkspaceDiagnosticLog(L"workspace_icon_picker_section_layout", sectionPayload);
                }

                sectionPanel.Children().Append(sectionRows);
                iconSectionsPanel.Children().Append(sectionPanel);
            };

            for (const auto& section : _workspaceIconSections)
            {
                appendSection(section);
            }
            appendSection(_workspaceWindowsSection);

            if (nodeIndex.has_value())
            {
                Json::Value payload{ Json::objectValue };
                payload["nodeIndex"] = *nodeIndex;
                terminal::workspacechat::AddDiagnosticTextFields(payload, "family", *currentFamily);
                std::ignore = terminal::workspacechat::AppendWorkspaceDiagnosticLog(L"workspace_icon_picker_rebuild_end", payload);
            }
        }

        template<typename TApplySelection>
        winrt::Windows::Foundation::IAsyncAction _showWorkspaceIconPickerDialog(TerminalPage& page,
                                                                                std::wstring initialIcon,
                                                                                TApplySelection&& applySelection,
                                                                                const std::optional<size_t> nodeIndex = std::nullopt)
        {
            auto dialog = ContentDialog{};
            dialog.PrimaryButtonText(L"");
            dialog.CloseButtonText(L"");
            dialog.FullSizeDesired(false);

            auto selectedIcon = std::make_shared<std::wstring>(std::move(initialIcon));
            auto currentFamily = std::make_shared<std::wstring>(_workspaceIconFamilyFromValue(*selectedIcon));

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

            auto iconSectionsPanel = StackPanel{};
            iconSectionsPanel.Spacing(1);
            const auto rebuildIconSections = [&page, selectedIcon, currentFamily, iconSectionsPanel, dialog, nodeIndex]() {
                _appendWorkspaceIconPickerSections(&page, selectedIcon, currentFamily, iconSectionsPanel, dialog, nodeIndex);
            };

            for (const auto& family : _workspaceIconFamilies)
            {
                auto familyButton = Button{};
                familyButton.Content(box_value(family.label));
                familyButton.Tag(box_value(family.key));
                familyButton.Padding(WUX::ThicknessHelper::FromLengths(6, 3, 6, 3));
                familyButton.MinWidth(64);
                familyButton.MinHeight(28);
                familyButton.Click([currentFamily, updateFamilyButtonsState, rebuildIconSections, familyKey = std::wstring{ family.key }](auto&&, auto&&) {
                    *currentFamily = familyKey;
                    updateFamilyButtonsState();
                    rebuildIconSections();
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
            chooseFileButton.Click([weakThis = page.get_weak(), dialog, selectedIcon](auto&&, auto&&) {
                [](auto weakThis, ContentDialog dialog, std::shared_ptr<std::wstring> selectedIcon) -> safe_void_coroutine {
                    if (auto self{ weakThis.get() })
                    {
                        const auto selectedPath = co_await OpenImagePicker(nullptr);
                        if (!selectedPath.empty())
                        {
                            *selectedIcon = selectedPath.c_str();
                            dialog.Hide();
                        }
                    }
                }(weakThis, dialog, selectedIcon);
            });
            Controls::Grid::SetColumn(chooseFileButton, 2);
            headerPanel.Children().Append(chooseFileButton);

            rootPanel.Children().Append(headerPanel);
            rebuildIconSections();
            rootPanel.Children().Append(iconSectionsPanel);
            dialog.Content(rootPanel);

            if (nodeIndex.has_value())
            {
                Json::Value payload{ Json::objectValue };
                payload["nodeIndex"] = *nodeIndex;
                std::ignore = terminal::workspacechat::AppendWorkspaceDiagnosticLog(L"workspace_icon_picker_show_dialog", payload);
            }

            const auto presenter = page.DialogPresenter();
            if (!presenter)
            {
                if (nodeIndex.has_value())
                {
                    Json::Value payload{ Json::objectValue };
                    payload["nodeIndex"] = *nodeIndex;
                    std::ignore = terminal::workspacechat::AppendWorkspaceDiagnosticLog(L"workspace_icon_picker_missing_presenter", payload);
                }
                co_return;
            }

            const auto result = co_await presenter.ShowDialog(dialog);
            if (nodeIndex.has_value())
            {
                Json::Value payload{ Json::objectValue };
                payload["nodeIndex"] = *nodeIndex;
                payload["dialogResult"] = static_cast<int>(result);
                std::ignore = terminal::workspacechat::AppendWorkspaceDiagnosticLog(L"workspace_icon_picker_dialog_result", payload);
            }

            if (!selectedIcon->empty())
            {
                applySelection(*selectedIcon);
            }
        }
    }

    safe_void_coroutine TerminalPage::_ShowWorkspaceManagerWorkspaceIconPicker()
    {
        try
        {
            std::wstring initialIcon;
            if (const auto* current = _SelectedWorkspaceForEditing())
            {
                initialIcon = current->Icon;
            }

            co_await _showWorkspaceIconPickerDialog(*this, std::move(initialIcon), [weakThis = get_weak()](const std::wstring& iconValue) {
                if (auto self{ weakThis.get() })
                {
                    self->_ApplyWorkspaceManagerWorkspaceIconSelection(iconValue);
                }
            });
        }
        catch (...)
        {
            throw;
        }
    }

    safe_void_coroutine TerminalPage::_ShowWorkspaceManagerNodeIconPicker(const size_t nodeIndex)
    {
        Json::Value startPayload{ Json::objectValue };
        startPayload["nodeIndex"] = nodeIndex;
        std::ignore = terminal::workspacechat::AppendWorkspaceDiagnosticLog(L"workspace_icon_picker_click", startPayload);

        try
        {
            std::wstring initialIcon;
            if (const auto* current = _SelectedWorkspaceForEditing(); current && nodeIndex < current->Nodes.size())
            {
                initialIcon = current->Nodes.at(nodeIndex).Icon;
            }

            co_await _showWorkspaceIconPickerDialog(*this, std::move(initialIcon), [weakThis = get_weak(), nodeIndex](const std::wstring& iconValue) {
                if (auto self{ weakThis.get() })
                {
                    Json::Value payload{ Json::objectValue };
                    payload["nodeIndex"] = nodeIndex;
                    std::wstring previousIcon;
                    terminal::workspacechat::AddDiagnosticTextFields(payload, "selectedIcon", iconValue);
                    if (const auto* currentBefore = self->_SelectedWorkspaceForEditing(); currentBefore && nodeIndex < currentBefore->Nodes.size())
                    {
                        previousIcon = currentBefore->Nodes.at(nodeIndex).Icon;
                        terminal::workspacechat::AddDiagnosticTextFields(payload, "previousIcon", previousIcon);
                    }
                    std::ignore = terminal::workspacechat::AppendWorkspaceDiagnosticLog(L"workspace_icon_picker_selected", payload);
                    if (previousIcon == iconValue)
                    {
                        Json::Value noopPayload{ Json::objectValue };
                        noopPayload["nodeIndex"] = nodeIndex;
                        std::ignore = terminal::workspacechat::AppendWorkspaceDiagnosticLog(L"workspace_icon_picker_noop_same_icon", noopPayload);
                    }
                    else
                    {
                        self->_ApplyWorkspaceManagerNodeIconSelection(nodeIndex, iconValue);
                    }
                }
            }, nodeIndex);
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
