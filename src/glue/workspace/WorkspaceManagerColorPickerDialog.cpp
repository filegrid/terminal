// Copyright (c) Tommy Yan <tommy.yxd@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-only

#include "pch.h"
#include "WorkspaceManagerColorPickerDialog.h"

#include <winrt/Windows.UI.Xaml.Controls.Primitives.h>

using namespace winrt;
using namespace winrt::Windows::UI;
using namespace winrt::Windows::UI::Xaml::Controls;

namespace terminal::workspace
{
    namespace
    {
        std::optional<Color> _parseColor(const std::wstring_view value)
        {
            if (value.size() != 7 || value.front() != L'#')
            {
                return std::nullopt;
            }

            const auto parseByte = [](const wchar_t* text) -> std::optional<uint8_t> {
                wchar_t* end{};
                const auto value = std::wcstoul(text, &end, 16);
                return end == text + 2 && value <= 0xff ? std::optional{ static_cast<uint8_t>(value) } : std::nullopt;
            };
            const auto red = parseByte(value.data() + 1);
            const auto green = parseByte(value.data() + 3);
            const auto blue = parseByte(value.data() + 5);
            if (!red || !green || !blue)
            {
                return std::nullopt;
            }
            return Color{ 255, *red, *green, *blue };
        }

        hstring _formatColor(const Color& color)
        {
            wchar_t text[8]{};
            swprintf_s(text, L"#%02X%02X%02X", color.R, color.G, color.B);
            return hstring{ text };
        }
    }

    winrt::Windows::Foundation::IAsyncOperation<hstring> PickWorkspaceManagerColor(TerminalPageBase& host,
                                                                                     std::wstring initialColor)
    {
        auto picker = ColorPicker{};
        picker.IsAlphaEnabled(false);
        if (const auto color = _parseColor(initialColor))
        {
            picker.Color(*color);
        }

        auto dialog = ContentDialog{};
        dialog.Title(box_value(L"选择颜色"));
        dialog.Content(picker);
        dialog.PrimaryButtonText(L"确定");
        dialog.CloseButtonText(L"取消");
        if (co_await host.ShowWorkspaceDialog(dialog) == ContentDialogResult::Primary)
        {
            co_return _formatColor(picker.Color());
        }
        co_return {};
    }
}
