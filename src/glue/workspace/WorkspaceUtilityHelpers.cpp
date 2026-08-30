        std::wstring_view _trimLeft(const std::wstring_view value)
        {
            const auto pos = value.find_first_not_of(L" \t\r\n");
            return pos == std::wstring_view::npos ? std::wstring_view{} : value.substr(pos);
        }

        std::wstring_view _trimRight(const std::wstring_view value)
        {
            const auto pos = value.find_last_not_of(L" \t\r\n");
            return pos == std::wstring_view::npos ? std::wstring_view{} : value.substr(0, pos + 1);
        }

        std::wstring_view _trim(const std::wstring_view value)
        {
            return _trimRight(_trimLeft(value));
        }

        std::wstring _unquote(std::wstring_view value)
        {
            value = _trim(value);
            if (value.size() >= 2)
            {
                const auto first = value.front();
                const auto last = value.back();
                if ((first == L'\'' && last == L'\'') || (first == L'"' && last == L'"'))
                {
                    value.remove_prefix(1);
                    value.remove_suffix(1);
                }
            }

            std::wstring result;
            result.reserve(value.size());
            for (size_t i = 0; i < value.size(); ++i)
            {
                if (value[i] == L'\'' && i + 1 < value.size() && value[i + 1] == L'\'')
                {
                    result.push_back(L'\'');
                    ++i;
                }
                else
                {
                    result.push_back(value[i]);
                }
            }
            return result;
        }

        std::wstring _quote(std::wstring_view value)
        {
            std::wstring result;
            result.reserve(value.size() + 2);
            result.push_back(L'\'');
            for (const auto ch : value)
            {
                result.push_back(ch);
                if (ch == L'\'')
                {
                    result.push_back(L'\'');
                }
            }
            result.push_back(L'\'');
            return result;
        }

        bool _containsLineBreak(std::wstring_view value) noexcept
        {
            return value.find(L'\r') != std::wstring_view::npos ||
                   value.find(L'\n') != std::wstring_view::npos;
        }

        void _writeMultilineValue(std::wostringstream& stream,
                                  std::wstring_view indent,
                                  std::wstring_view key,
                                  std::wstring_view value)
        {
            stream << indent << key << L": |\n";

            size_t start = 0;
            while (start <= value.size())
            {
                auto end = start;
                while (end < value.size() && value[end] != L'\r' && value[end] != L'\n')
                {
                    ++end;
                }

                stream << indent << L"  " << value.substr(start, end - start) << L"\n";
                if (end >= value.size())
                {
                    break;
                }

                if (value[end] == L'\r' && end + 1 < value.size() && value[end + 1] == L'\n')
                {
                    start = end + 2;
                }
                else
                {
                    start = end + 1;
                }
            }
        }

        std::wstring _toLower(std::wstring_view value)
        {
            std::wstring lowered;
            lowered.reserve(value.size());
            for (const auto ch : value)
            {
                lowered.push_back(static_cast<wchar_t>(std::towlower(ch)));
            }
            return lowered;
        }

        bool _isHexDigit(const wchar_t ch) noexcept
        {
            return (ch >= L'0' && ch <= L'9') ||
                   (ch >= L'a' && ch <= L'f') ||
                   (ch >= L'A' && ch <= L'F');
        }

        std::wstring _normalizeColor(std::wstring_view color)
        {
            if (color.size() != 7 || color[0] != L'#')
            {
                return {};
            }

            std::wstring normalized;
            normalized.reserve(color.size());
            normalized.push_back(L'#');
            for (size_t i = 1; i < color.size(); ++i)
            {
                if (!_isHexDigit(color[i]))
                {
                    return {};
                }
                normalized.push_back(static_cast<wchar_t>(std::towupper(color[i])));
            }

            return normalized;
        }

        std::optional<winrt::Windows::UI::Color> _parseColor(std::wstring_view color)
        {
            const auto normalized = _normalizeColor(color);
            if (normalized.empty())
            {
                return std::nullopt;
            }

            return static_cast<winrt::Windows::UI::Color>(::Microsoft::Console::Utils::ColorFromHexString(til::u16u8(normalized)));
        }

        std::wstring _colorToString(const winrt::Windows::UI::Color& color)
        {
            return til::u8u16(::Microsoft::Console::Utils::ColorToHexString(til::color{ color }));
        }

        uint64_t _stableHash(const std::wstring_view value) noexcept
        {
            uint64_t hash = 1469598103934665603ull;
            for (const auto ch : value)
            {
                hash ^= static_cast<uint64_t>(static_cast<uint16_t>(ch));
                hash *= 1099511628211ull;
            }
            return hash;
        }

        COLORREF _toColorRef(const winrt::Windows::UI::Color& color) noexcept
        {
            return RGB(color.R, color.G, color.B);
        }

        winrt::Windows::UI::Color _fromColorRef(const COLORREF color, const uint8_t alpha) noexcept
        {
            winrt::Windows::UI::Color uiColor{};
            uiColor.A = alpha;
            uiColor.R = GetRValue(color);
            uiColor.G = GetGValue(color);
            uiColor.B = GetBValue(color);
            return uiColor;
        }

        std::pair<std::wstring, std::wstring> _parseKeyValue(std::wstring_view content)
        {
            if (content.starts_with(L"- "))
            {
                content.remove_prefix(2);
            }

            const auto colon = content.find(L':');
            if (colon == std::wstring_view::npos)
            {
                return {};
            }

            std::wstring key{ _trim(content.substr(0, colon)) };
            std::wstring value{ _unquote(content.substr(colon + 1)) };
            return { std::move(key), std::move(value) };
        }

        bool _parseBool(const std::wstring& value, const bool fallback) noexcept
        {
            if (value == L"true")
            {
                return true;
            }
            if (value == L"false")
            {
                return false;
            }
            return fallback;
        }
