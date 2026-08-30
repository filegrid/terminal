// Copyright (c) Tommy Yan <tommy.yxd@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include <json/json.h>

#include <cstdint>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <locale>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>

namespace terminal::workspacechat::core
{
    inline void AppendUtf8CodePoint(std::string& output, uint32_t codePoint)
    {
        if (codePoint <= 0x7F)
        {
            output.push_back(static_cast<char>(codePoint));
        }
        else if (codePoint <= 0x7FF)
        {
            output.push_back(static_cast<char>(0xC0 | (codePoint >> 6)));
            output.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
        }
        else if (codePoint <= 0xFFFF)
        {
            output.push_back(static_cast<char>(0xE0 | (codePoint >> 12)));
            output.push_back(static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F)));
            output.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
        }
        else
        {
            output.push_back(static_cast<char>(0xF0 | (codePoint >> 18)));
            output.push_back(static_cast<char>(0x80 | ((codePoint >> 12) & 0x3F)));
            output.push_back(static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F)));
            output.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
        }
    }

    inline void AppendWideCodePoint(std::wstring& output, uint32_t codePoint)
    {
        if (codePoint <= 0xFFFF)
        {
            output.push_back(static_cast<wchar_t>(codePoint));
            return;
        }

        codePoint -= 0x10000;
        output.push_back(static_cast<wchar_t>(0xD800 + (codePoint >> 10)));
        output.push_back(static_cast<wchar_t>(0xDC00 + (codePoint & 0x3FF)));
    }

    inline std::wstring LocalDateStamp()
    {
        const auto now = std::time(nullptr);
        std::tm localTime{};
        localtime_s(&localTime, &now);

        wchar_t buffer[16]{};
        wcsftime(buffer, sizeof(buffer) / sizeof(buffer[0]), L"%Y-%m-%d", &localTime);
        return buffer;
    }

    inline std::string ToUtf8(std::wstring_view value)
    {
        std::string output;
        output.reserve(value.size() * 3);

        for (size_t i = 0; i < value.size(); ++i)
        {
            uint32_t codePoint = static_cast<uint16_t>(value[i]);
            if (codePoint >= 0xD800 && codePoint <= 0xDBFF)
            {
                if (i + 1 < value.size())
                {
                    const auto trail = static_cast<uint16_t>(value[i + 1]);
                    if (trail >= 0xDC00 && trail <= 0xDFFF)
                    {
                        codePoint = 0x10000 + (((codePoint - 0xD800) << 10) | (trail - 0xDC00));
                        ++i;
                    }
                    else
                    {
                        codePoint = 0xFFFD;
                    }
                }
                else
                {
                    codePoint = 0xFFFD;
                }
            }
            else if (codePoint >= 0xDC00 && codePoint <= 0xDFFF)
            {
                codePoint = 0xFFFD;
            }

            AppendUtf8CodePoint(output, codePoint);
        }

        return output;
    }

    inline std::wstring ToWide(std::string_view value)
    {
        std::wstring output;
        output.reserve(value.size());

        size_t i = 0;
        while (i < value.size())
        {
            const auto byte0 = static_cast<uint8_t>(value[i]);
            uint32_t codePoint = 0xFFFD;
            size_t advance = 1;

            if (byte0 <= 0x7F)
            {
                codePoint = byte0;
            }
            else if ((byte0 & 0xE0) == 0xC0 && i + 1 < value.size())
            {
                const auto byte1 = static_cast<uint8_t>(value[i + 1]);
                if ((byte1 & 0xC0) == 0x80)
                {
                    codePoint = ((byte0 & 0x1F) << 6) | (byte1 & 0x3F);
                    advance = 2;
                    if (codePoint < 0x80)
                    {
                        codePoint = 0xFFFD;
                    }
                }
            }
            else if ((byte0 & 0xF0) == 0xE0 && i + 2 < value.size())
            {
                const auto byte1 = static_cast<uint8_t>(value[i + 1]);
                const auto byte2 = static_cast<uint8_t>(value[i + 2]);
                if ((byte1 & 0xC0) == 0x80 && (byte2 & 0xC0) == 0x80)
                {
                    codePoint = ((byte0 & 0x0F) << 12) | ((byte1 & 0x3F) << 6) | (byte2 & 0x3F);
                    advance = 3;
                    if (codePoint < 0x800 || (codePoint >= 0xD800 && codePoint <= 0xDFFF))
                    {
                        codePoint = 0xFFFD;
                    }
                }
            }
            else if ((byte0 & 0xF8) == 0xF0 && i + 3 < value.size())
            {
                const auto byte1 = static_cast<uint8_t>(value[i + 1]);
                const auto byte2 = static_cast<uint8_t>(value[i + 2]);
                const auto byte3 = static_cast<uint8_t>(value[i + 3]);
                if ((byte1 & 0xC0) == 0x80 && (byte2 & 0xC0) == 0x80 && (byte3 & 0xC0) == 0x80)
                {
                    codePoint = ((byte0 & 0x07) << 18) | ((byte1 & 0x3F) << 12) | ((byte2 & 0x3F) << 6) | (byte3 & 0x3F);
                    advance = 4;
                    if (codePoint < 0x10000 || codePoint > 0x10FFFF)
                    {
                        codePoint = 0xFFFD;
                    }
                }
            }

            AppendWideCodePoint(output, codePoint);
            i += advance;
        }

        return output;
    }

    inline bool EnsureParent(const std::filesystem::path& path)
    {
        std::error_code ec;
        if (const auto parent = path.parent_path(); !parent.empty())
        {
            std::filesystem::create_directories(parent, ec);
        }
        return !ec;
    }

    inline std::optional<Json::Value> ParseJsonLine(const std::string& line)
    {
        Json::CharReaderBuilder builder;
        std::string errors;
        Json::Value value;
        std::istringstream stream{ line };
        if (!Json::parseFromStream(builder, stream, &value, &errors))
        {
            return std::nullopt;
        }
        return value;
    }

    inline bool AppendJsonLine(const std::filesystem::path& path, const Json::Value& value)
    {
        if (!EnsureParent(path))
        {
            return false;
        }

        Json::StreamWriterBuilder builder;
        builder["indentation"] = "";
        const auto serialized = Json::writeString(builder, value);

        std::ofstream output{ path, std::ios::binary | std::ios::app };
        if (!output)
        {
            return false;
        }

        output.write(serialized.data(), static_cast<std::streamsize>(serialized.size()));
        output.put('\n');
        return output.good();
    }
}
