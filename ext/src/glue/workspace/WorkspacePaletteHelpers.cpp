    const std::vector<std::wstring_view>& WorkspaceColorPalette() noexcept
    {
        static const std::vector<std::wstring_view> palette{
            L"#C50F1F",
            L"#0063B1",
            L"#0F7B0F",
            L"#CA5010",
            L"#8E562E",
            L"#744DA9",
            L"#038387",
            L"#881798",
            L"#498205",
            L"#515C6B",
            L"#567C73",
            L"#7A7574",
        };
        return palette;
    }

    std::wstring PickWorkspacePaletteColor(const std::unordered_set<std::wstring>& usedColors,
                                           const size_t fallbackIndex,
                                           const std::wstring_view excludedColor)
    {
        const auto& palette = WorkspaceColorPalette();
        for (const auto color : palette)
        {
            if (color != excludedColor && !usedColors.contains(std::wstring{ color }))
            {
                return std::wstring{ color };
            }
        }

        return std::wstring{ palette[fallbackIndex % palette.size()] };
    }
