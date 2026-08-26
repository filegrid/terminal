// Copyright (c) Tommy Yan <tommy.yxd@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-only

#include "pch.h"
#include "WorkspaceManagerIconPickerDialog.h"
#include "winrt/Microsoft.Terminal.UI.h"
#include "../../core/chat/WorkspaceDiagnosticLog.h"
#include <winrt/Windows.UI.Xaml.Controls.Primitives.h>
#include "../../../microsoft/src/cascadia/WinRTUtils/inc/Utils.h"

using namespace winrt;
using namespace winrt::Windows::UI;
using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Windows::UI::Xaml::Controls;
using namespace winrt::Windows::UI::Xaml::Media;

namespace winrt
{
    namespace WUX = Windows::UI::Xaml;
}

namespace terminal::workspace
{
    namespace
    {
        struct IconSection
        {
            const wchar_t* Key;
            const wchar_t* Label;
            uint32_t Count;
        };

        constexpr std::array<const wchar_t*, 4> Families{ L"color", L"outline", L"duotone", L"sharp" };
        constexpr std::array<IconSection, 6> Sections{ {
            { L"numbers", L"数字 0-9", 10 }, { L"letters", L"字母 A-Z", 26 }, { L"daily", L"日常", 20 },
            { L"development", L"研发", 20 }, { L"office", L"办公", 20 }, { L"windows", L"Windows / OS", 20 },
        } };

        std::wstring _descriptor(const std::wstring& family, const IconSection& section, const uint32_t index)
        {
            return std::wstring{ L"workspace-icon://" } + family + L"/" + section.Key + L"/" + std::to_wstring(index);
        }

        std::wstring _familyFromIcon(const std::wstring_view icon)
        {
            constexpr std::wstring_view prefix{ L"workspace-icon://" };
            if (!icon.starts_with(prefix))
            {
                return L"color";
            }
            const auto payload = icon.substr(prefix.size());
            const auto slash = payload.find(L'/');
            return slash == std::wstring_view::npos ? L"color" : std::wstring{ payload.substr(0, slash) };
        }

        void _buildSections(const std::shared_ptr<std::wstring>& selected,
                            const std::shared_ptr<std::wstring>& family,
                            const StackPanel& panel,
                            const ContentDialog& dialog)
        {
            panel.Children().Clear();
            for (const auto& section : Sections)
            {
                auto rows = StackPanel{};
                rows.Spacing(1);
                StackPanel row{};
                uint32_t inRow{};
                for (uint32_t index{}; index < section.Count; ++index)
                {
                    if (!row || inRow == 0)
                    {
                        row = StackPanel{};
                        row.Orientation(Orientation::Horizontal);
                        row.Spacing(1);
                        rows.Children().Append(row);
                    }
                    const auto value = _descriptor(*family, section, index);
                    auto button = Button{};
                    button.Width(40); button.Height(40); button.MinWidth(40); button.MinHeight(40);
                    button.Padding(WUX::ThicknessHelper::FromLengths(0, 0, 0, 0));
                    const auto active = *selected == value;
                    button.BorderThickness(WUX::ThicknessHelper::FromLengths(active ? 2 : 1, active ? 2 : 1, active ? 2 : 1, active ? 2 : 1));
                    button.BorderBrush(SolidColorBrush{ active ? Colors::DodgerBlue() : Colors::DimGray() });
                    if (const auto preview = winrt::Microsoft::Terminal::UI::IconPathConverter::IconWUX(hstring{ value }))
                    {
                        if (const auto element = preview.try_as<FrameworkElement>())
                        {
                            element.Width(32); element.Height(32);
                            element.HorizontalAlignment(HorizontalAlignment::Center);
                            element.VerticalAlignment(VerticalAlignment::Center);
                        }
                        button.Content(preview);
                    }
                    button.Click([dialog, selected, value](auto&&, auto&&) { *selected = value; dialog.Hide(); });
                    row.Children().Append(button);
                    if (++inRow == 10) { row = nullptr; inRow = 0; }
                }
                panel.Children().Append(rows);
            }
        }
    }

    winrt::Windows::Foundation::IAsyncOperation<winrt::hstring> PickWorkspaceManagerIcon(
        TerminalPageBase& host,
        std::wstring initialIcon,
        const std::optional<size_t> nodeIndex)
    {
        auto dialog = ContentDialog{};
        dialog.PrimaryButtonText(L""); dialog.CloseButtonText(L""); dialog.FullSizeDesired(false);
        auto selected = std::make_shared<std::wstring>(std::move(initialIcon));
        auto family = std::make_shared<std::wstring>(_familyFromIcon(*selected));
        auto root = StackPanel{};
        root.Spacing(4); root.Width(412); root.MinWidth(412); root.Height(544); root.MinHeight(544);
        dialog.Title(box_value(L"选择图标"));

        auto header = StackPanel{};
        header.Orientation(Orientation::Horizontal); header.Spacing(4);
        auto sections = StackPanel{};
        sections.Spacing(1);
        const auto rebuild = [selected, family, sections, dialog]() { _buildSections(selected, family, sections, dialog); };
        for (const auto familyName : Families)
        {
            auto button = Button{};
            button.Content(box_value(familyName)); button.Tag(box_value(familyName));
            button.Padding(WUX::ThicknessHelper::FromLengths(6, 3, 6, 3)); button.MinWidth(64); button.MinHeight(28);
            button.Click([family, rebuild, familyValue = std::wstring{ familyName }](auto&&, auto&&) { *family = familyValue; rebuild(); });
            header.Children().Append(button);
        }
        auto chooseFile = HyperlinkButton{};
        chooseFile.Content(box_value(L"选择本地文件"));
        chooseFile.Click([dialog, selected](auto&&, auto&&) {
            [](ContentDialog dialog, std::shared_ptr<std::wstring> selected) -> safe_void_coroutine {
                const auto path = co_await OpenImagePicker(nullptr);
                if (!path.empty()) { *selected = path.c_str(); dialog.Hide(); }
            }(dialog, selected);
        });
        header.Children().Append(chooseFile);
        root.Children().Append(header);
        rebuild();
        root.Children().Append(sections);
        dialog.Content(root);
        if (nodeIndex.has_value())
        {
            Json::Value payload{ Json::objectValue }; payload["nodeIndex"] = *nodeIndex;
            std::ignore = terminal::workspacechat::AppendWorkspaceDiagnosticLog(L"workspace_icon_picker_show_dialog", payload);
        }
        std::ignore = co_await host.ShowWorkspaceDialog(dialog);
        co_return hstring{ *selected };
    }
}
