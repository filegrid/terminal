#include "pch.h"
#include "IconPathConverter.h"
#include "IconPathConverter.g.cpp"

#include "Utils.h"

#include "../types/inc/utils.hpp"

#include <array>
#include <Shlobj.h>
#include <Shlobj_core.h>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <winrt/Windows.Graphics.Imaging.h>
#include <winrt/Windows.Storage.h>
#include <til/unicode.h>
#include <unordered_map>
#include <wincodec.h>

namespace winrt
{
    namespace MUX = Microsoft::UI::Xaml;
}

using namespace winrt::Windows;
using namespace winrt::Windows::UI::Xaml;

using namespace winrt::Windows::Graphics::Imaging;
using namespace winrt::Windows::Storage;
using namespace winrt::Windows::Storage::Streams;

namespace winrt::Microsoft::Terminal::UI::implementation
{
    namespace
    {
        std::filesystem::path _workspaceIconLogPath()
        {
            wchar_t* userProfileValue = nullptr;
            size_t userProfileLength = 0;
            std::wstring userProfile;
            if (_wdupenv_s(&userProfileValue, &userProfileLength, L"USERPROFILE") == 0 && userProfileValue && userProfileLength > 0)
            {
                userProfile.assign(userProfileValue);
            }

            if (userProfileValue)
            {
                free(userProfileValue);
            }

            const auto root = userProfile.empty() ? (std::filesystem::current_path() / L".wt") : (std::filesystem::path{ userProfile } / L".wt");
            return root / L"logs" / L"workspace-chat-diagnostics.jsonl";
        }

        std::wstring _workspaceIconTimestamp()
        {
            SYSTEMTIME localTime{};
            GetLocalTime(&localTime);

            TIME_ZONE_INFORMATION timeZoneInfo{};
            const auto timeZoneState = GetTimeZoneInformation(&timeZoneInfo);
            long biasMinutes = timeZoneInfo.Bias;
            if (timeZoneState == TIME_ZONE_ID_DAYLIGHT)
            {
                biasMinutes += timeZoneInfo.DaylightBias;
            }
            else if (timeZoneState == TIME_ZONE_ID_STANDARD)
            {
                biasMinutes += timeZoneInfo.StandardBias;
            }

            const auto utcOffsetMinutes = -biasMinutes;
            const auto offsetHours = utcOffsetMinutes / 60;
            const auto offsetMinutes = std::abs(utcOffsetMinutes % 60);

            return fmt::format(L"{:04}-{:02}-{:02}T{:02}:{:02}:{:02}.{:03}{:+03}:{:02}",
                               localTime.wYear,
                               localTime.wMonth,
                               localTime.wDay,
                               localTime.wHour,
                               localTime.wMinute,
                               localTime.wSecond,
                               localTime.wMilliseconds,
                               offsetHours,
                               offsetMinutes);
        }

        std::string _workspaceIconEscapeJson(const std::wstring_view value)
        {
            std::string escaped;
            escaped.reserve(value.size() + 16);
            for (const auto ch : value)
            {
                switch (ch)
                {
                case L'\\':
                    escaped += "\\\\";
                    break;
                case L'"':
                    escaped += "\\\"";
                    break;
                case L'\r':
                    escaped += "\\r";
                    break;
                case L'\n':
                    escaped += "\\n";
                    break;
                case L'\t':
                    escaped += "\\t";
                    break;
                default:
                    escaped += til::u16u8(std::wstring_view{ &ch, 1 });
                    break;
                }
            }
            return escaped;
        }

        void _workspaceIconFileLog(std::wstring_view eventName, std::wstring_view message)
        {
            static std::mutex logLock;
            std::scoped_lock guard{ logLock };

            const auto logPath = _workspaceIconLogPath();
            std::error_code ec;
            std::filesystem::create_directories(logPath.parent_path(), ec);
            if (ec)
            {
                return;
            }

            std::ofstream file{ logPath, std::ios::app | std::ios::binary };
            if (!file.is_open())
            {
                return;
            }

            const auto ts = til::u16u8(_workspaceIconTimestamp());
            const auto eventUtf8 = til::u16u8(std::wstring{ eventName });
            const auto messageUtf8 = _workspaceIconEscapeJson(message);
            file << "{\"ts\":\"" << ts
                 << "\",\"event\":\"" << eventUtf8
                 << "\",\"pid\":" << GetCurrentProcessId()
                 << ",\"tid\":" << GetCurrentThreadId()
                 << ",\"payload\":{\"message\":\"" << messageUtf8
                 << "\"}}\n";
        }

        void _workspaceIconDebug(std::wstring_view message)
        {
            const auto line = std::wstring{ L"[WorkspaceIcon] " } + std::wstring{ message } + L"\n";
            OutputDebugStringW(line.c_str());
            _workspaceIconFileLog(L"workspace_icon_uihelpers", message);
        }

        constexpr uint32_t GridIconCellSize{ 48 };

        struct GridIconSpec
        {
            std::wstring path;
            uint32_t row{};
            uint32_t column{};
        };

        std::mutex _gridIconCacheLock;
        std::unordered_map<std::wstring, wil::com_ptr<IWICBitmap>> _gridIconBitmapCache;
        std::unordered_map<std::wstring, SoftwareBitmap> _gridIconCropCache;

        bool _looksLikeGridIconPath(const winrt::hstring& iconPath)
        {
            return std::wstring_view{ iconPath }.find(L'#') != std::wstring_view::npos;
        }

        std::optional<GridIconSpec> _parseGridIconSpec(const winrt::hstring& iconPath)
        {
            const std::wstring_view path{ iconPath };
            const auto hashIndex = path.rfind(L'#');
            if (hashIndex == std::wstring_view::npos)
            {
                return std::nullopt;
            }

            const auto basePath = path.substr(0, hashIndex);
            const auto fragment = path.substr(hashIndex + 1);
            const auto commaIndex = fragment.find(L',');
            if (basePath.empty() || commaIndex == std::wstring_view::npos)
            {
                return std::nullopt;
            }

            const auto row = til::parse_unsigned<uint32_t>(fragment.substr(0, commaIndex));
            const auto column = til::parse_unsigned<uint32_t>(fragment.substr(commaIndex + 1));
            if (!row.has_value() || !column.has_value())
            {
                return std::nullopt;
            }

            return GridIconSpec{ std::wstring{ basePath }, *row, *column };
        }

        wil::com_ptr<IWICImagingFactory> _getWicImagingFactory()
        {
            static wil::com_ptr<IWICImagingFactory> factory;
            static std::once_flag once;
            std::call_once(once, []() {
                THROW_IF_FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(factory.put())));
            });
            return factory;
        }

        static SoftwareBitmap _convertToSoftwareBitmap(gsl::not_null<IWICBitmapSource*> bitmapSource,
                                                       BitmapPixelFormat pixelFormat,
                                                       BitmapAlphaMode alphaMode)
        {
            const auto factory = _getWicImagingFactory();
            wil::com_ptr<IWICBitmap> bitmap;
            THROW_IF_FAILED(factory->CreateBitmapFromSource(bitmapSource.get(), WICBitmapCacheOnLoad, bitmap.put()));

            auto softwareBitmap = winrt::capture<SoftwareBitmap>(
                winrt::create_instance<ISoftwareBitmapNativeFactory>(CLSID_SoftwareBitmapNativeFactory),
                &ISoftwareBitmapNativeFactory::CreateFromWICBitmap,
                bitmap.get(),
                false);

            if (softwareBitmap.BitmapPixelFormat() != pixelFormat || softwareBitmap.BitmapAlphaMode() != alphaMode)
            {
                softwareBitmap = SoftwareBitmap::Convert(softwareBitmap, pixelFormat, alphaMode);
            }

            return softwareBitmap;
        }

        wil::com_ptr<IWICBitmap> _getCachedGridIconBitmap(std::wstring_view imagePath)
        {
            {
                std::scoped_lock guard{ _gridIconCacheLock };
                if (const auto it = _gridIconBitmapCache.find(std::wstring{ imagePath }); it != _gridIconBitmapCache.end())
                {
                    _workspaceIconDebug(fmt::format(L"grid bitmap cache hit path='{}'", imagePath));
                    return it->second;
                }
            }

            _workspaceIconDebug(fmt::format(L"grid bitmap cache miss path='{}'", imagePath));

            const auto factory = _getWicImagingFactory();

            wil::com_ptr<IWICBitmapDecoder> decoder;
            THROW_IF_FAILED(factory->CreateDecoderFromFilename(imagePath.data(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnLoad, decoder.put()));

            wil::com_ptr<IWICBitmapFrameDecode> frame;
            THROW_IF_FAILED(decoder->GetFrame(0, frame.put()));

            wil::com_ptr<IWICFormatConverter> converter;
            THROW_IF_FAILED(factory->CreateFormatConverter(converter.put()));
            THROW_IF_FAILED(converter->Initialize(frame.get(),
                                                  GUID_WICPixelFormat32bppPBGRA,
                                                  WICBitmapDitherTypeNone,
                                                  nullptr,
                                                  0.0,
                                                  WICBitmapPaletteTypeCustom));

            wil::com_ptr<IWICBitmap> bitmap;
            THROW_IF_FAILED(factory->CreateBitmapFromSource(converter.get(), WICBitmapCacheOnLoad, bitmap.put()));

            {
                std::scoped_lock guard{ _gridIconCacheLock };
                _gridIconBitmapCache.emplace(std::wstring{ imagePath }, bitmap);
            }

            return bitmap;
        }

        static winrt::Windows::UI::Xaml::Media::Imaging::SoftwareBitmapSource _makeSoftwareBitmapSource(const SoftwareBitmap& softwareBitmap)
        {
            winrt::Windows::UI::Xaml::Media::Imaging::SoftwareBitmapSource bitmapSource{};
            bitmapSource.SetBitmapAsync(softwareBitmap);
            return bitmapSource;
        }

        SoftwareBitmap _getGridIconBitmap(const winrt::hstring& iconPath)
        {
            {
                std::scoped_lock guard{ _gridIconCacheLock };
                if (const auto it = _gridIconCropCache.find(iconPath.c_str()); it != _gridIconCropCache.end())
                {
                    _workspaceIconDebug(fmt::format(L"grid crop cache hit iconPath='{}'", iconPath.c_str()));
                    return it->second;
                }
            }

            const auto spec = _parseGridIconSpec(iconPath);
            if (!spec.has_value())
            {
                _workspaceIconDebug(fmt::format(L"grid icon parse failed iconPath='{}'", iconPath.c_str()));
                return nullptr;
            }

            _workspaceIconDebug(fmt::format(L"grid crop begin path='{}' row={} column={}",
                                            spec->path,
                                            spec->row,
                                            spec->column));

            const auto bitmap = _getCachedGridIconBitmap(spec->path);
            if (!bitmap)
            {
                return nullptr;
            }

            UINT width{};
            UINT height{};
            THROW_IF_FAILED(bitmap->GetSize(&width, &height));

            const auto pixelX = spec->column * GridIconCellSize;
            const auto pixelY = spec->row * GridIconCellSize;
            if (pixelX + GridIconCellSize > width || pixelY + GridIconCellSize > height)
            {
                _workspaceIconDebug(fmt::format(L"grid icon out of bounds path='{}' row={} column={} size={}x{}",
                                                spec->path,
                                                spec->row,
                                                spec->column,
                                                width,
                                                height));
                return nullptr;
            }

            const auto factory = _getWicImagingFactory();

            wil::com_ptr<IWICBitmapClipper> clipper;
            THROW_IF_FAILED(factory->CreateBitmapClipper(clipper.put()));

            const WICRect rect{
                gsl::narrow<INT>(pixelX),
                gsl::narrow<INT>(pixelY),
                gsl::narrow<INT>(GridIconCellSize),
                gsl::narrow<INT>(GridIconCellSize),
            };
            THROW_IF_FAILED(clipper->Initialize(bitmap.get(), &rect));

            wil::com_ptr<IWICBitmapSource> clippedSource;
            clipper.query_to(clippedSource.put());

            auto softwareBitmap = _convertToSoftwareBitmap(clippedSource.get(),
                                                           BitmapPixelFormat::Bgra8,
                                                           BitmapAlphaMode::Premultiplied);

            {
                std::scoped_lock guard{ _gridIconCacheLock };
                _gridIconCropCache.insert_or_assign(iconPath.c_str(), softwareBitmap);
            }

            _workspaceIconDebug(fmt::format(L"grid crop ready iconPath='{}' width={} height={}",
                                            iconPath.c_str(),
                                            GridIconCellSize,
                                            GridIconCellSize));

            return softwareBitmap;
        }

        struct WorkspaceSpriteSpec
        {
            std::wstring family;
            std::wstring section;
            uint32_t index{};
        };

        constexpr std::wstring_view WorkspaceIconScheme{ L"workspace-icon://" };

        std::optional<WorkspaceSpriteSpec> _parseWorkspaceSpriteSpec(const winrt::hstring& iconPath)
        {
            const std::wstring_view path{ iconPath };
            if (!path.starts_with(WorkspaceIconScheme))
            {
                return std::nullopt;
            }

            const auto descriptorPayload = path.substr(WorkspaceIconScheme.size());
            const auto slash1 = descriptorPayload.find(L'/');
            const auto slash2 = descriptorPayload.find(L'/', slash1 == std::wstring_view::npos ? slash1 : slash1 + 1);
            if (slash1 == std::wstring_view::npos || slash2 == std::wstring_view::npos)
            {
                return std::nullopt;
            }

            WorkspaceSpriteSpec spec;
            spec.family = std::wstring{ descriptorPayload.substr(0, slash1) };
            spec.section = std::wstring{ descriptorPayload.substr(slash1 + 1, slash2 - slash1 - 1) };
            const auto index = til::parse_unsigned<uint32_t>(descriptorPayload.substr(slash2 + 1));
            if (!index.has_value())
            {
                return std::nullopt;
            }
            spec.index = *index;

            _workspaceIconDebug(fmt::format(L"parse iconPath='{}' family='{}' section='{}' index={}",
                                            iconPath.c_str(),
                                            spec.family,
                                            spec.section,
                                            spec.index));
            return spec;
        }

        std::optional<std::wstring> _workspaceIconAssetName(const WorkspaceSpriteSpec& spec)
        {
            if (spec.section == L"numbers" && spec.index < 10)
            {
                return std::to_wstring(spec.index);
            }

            if (spec.section == L"letters" && spec.index < 26)
            {
                return std::wstring{ 1, static_cast<wchar_t>(L'A' + spec.index) };
            }

            const auto makeIndexedName = [&](const wchar_t prefix) -> std::optional<std::wstring> {
                if (spec.index >= 20)
                {
                    return std::nullopt;
                }
                return fmt::format(L"{}{:02}", prefix, spec.index + 1);
            };

            if (spec.section == L"daily")
            {
                return makeIndexedName(L'D');
            }

            if (spec.section == L"development")
            {
                return makeIndexedName(L'R');
            }

            if (spec.section == L"office")
            {
                return makeIndexedName(L'O');
            }

            if (spec.section == L"windows")
            {
                return makeIndexedName(L'W');
            }

            return std::nullopt;
        }

        std::optional<uint32_t> _workspaceIconSectionStartRow(std::wstring_view section)
        {
            if (section == L"numbers")
            {
                return 0;
            }
            if (section == L"letters")
            {
                return 1;
            }
            if (section == L"daily")
            {
                return 4;
            }
            if (section == L"development")
            {
                return 6;
            }
            if (section == L"office")
            {
                return 8;
            }
            if (section == L"windows")
            {
                return 10;
            }
            return std::nullopt;
        }

        std::optional<winrt::hstring> _resolveWorkspaceMergedIconPath(const WorkspaceSpriteSpec& spec)
        {
            const auto startRow = _workspaceIconSectionStartRow(spec.section);
            if (!startRow.has_value())
            {
                _workspaceIconDebug(fmt::format(L"merged icon section unsupported family='{}' section='{}' index={}",
                                                spec.family,
                                                spec.section,
                                                spec.index));
                return std::nullopt;
            }

            wchar_t modulePath[MAX_PATH]{};
            const auto moduleLength = GetModuleFileNameW(nullptr, modulePath, ARRAYSIZE(modulePath));
            if (moduleLength == 0)
            {
                return std::nullopt;
            }

            const auto row = *startRow + (spec.index / 10);
            const auto column = spec.index % 10;
            const auto fileName = std::wstring{ L"merged.png" };

            _workspaceIconDebug(fmt::format(L"merged icon resolve begin family='{}' section='{}' index={} row={} column={}",
                                            spec.family,
                                            spec.section,
                                            spec.index,
                                            row,
                                            column));

            std::vector<std::filesystem::path> roots;
            roots.emplace_back(std::filesystem::path{ modulePath }.parent_path());
            roots.emplace_back(std::filesystem::current_path());

            for (const auto& root : roots)
            {
                auto probe = root;
                for (int depth = 0; depth < 8 && !probe.empty(); ++depth)
                {
                    const std::array candidates{
                        probe / L"ext" / L"res" / L"v1" / L"assets" / spec.family / fileName,
                        probe / L"res" / L"v1" / L"assets" / spec.family / fileName,
                        probe / L"bin" / L"ext" / L"res" / L"v1" / L"assets" / spec.family / fileName,
                        probe / L"bin" / L"res" / L"v1" / L"assets" / spec.family / fileName,
                    };

                    for (const auto& candidate : candidates)
                    {
                        std::error_code ec;
                        _workspaceIconDebug(fmt::format(L"merged icon probe path='{}'", candidate.wstring()));
                        if (std::filesystem::exists(candidate, ec))
                        {
                            _workspaceIconDebug(fmt::format(L"merged icon resolved family='{}' section='{}' index={} path='{}' row={} column={}",
                                                            spec.family,
                                                            spec.section,
                                                            spec.index,
                                                            candidate.wstring(),
                                                            row,
                                                            column));
                            return winrt::hstring{ candidate.wstring() + L"#" + std::to_wstring(row) + L"," + std::to_wstring(column) };
                        }
                    }

                    const auto parent = probe.parent_path();
                    if (parent == probe)
                    {
                        break;
                    }
                    probe = parent;
                }
            }

            _workspaceIconDebug(fmt::format(L"merged icon resolve failed family='{}' section='{}' index={} row={} column={}",
                                            spec.family,
                                            spec.section,
                                            spec.index,
                                            row,
                                            column));
            return std::nullopt;
        }

        std::optional<std::filesystem::path> _resolveWorkspaceIconFile(const WorkspaceSpriteSpec& spec)
        {
            wchar_t modulePath[MAX_PATH]{};
            const auto moduleLength = GetModuleFileNameW(nullptr, modulePath, ARRAYSIZE(modulePath));
            if (moduleLength == 0)
            {
                return std::nullopt;
            }

            const auto assetName = _workspaceIconAssetName(spec);
            if (!assetName.has_value())
            {
                _workspaceIconDebug(fmt::format(L"invalid asset name family='{}' section='{}' index={}",
                                                spec.family,
                                                spec.section,
                                                spec.index));
                return std::nullopt;
            }

            const auto fileName = *assetName + L".png";
            std::vector<std::filesystem::path> roots;
            roots.emplace_back(std::filesystem::path{ modulePath }.parent_path());
            roots.emplace_back(std::filesystem::current_path());

            for (const auto& root : roots)
            {
                auto probe = root;
                for (int depth = 0; depth < 8 && !probe.empty(); ++depth)
                {
                    const std::array candidates{
                        probe / L"ext" / L"res" / L"v1" / L"assets" / spec.family / fileName,
                        probe / L"res" / L"v1" / L"assets" / spec.family / fileName,
                        probe / L"bin" / L"ext" / L"res" / L"v1" / L"assets" / spec.family / fileName,
                        probe / L"bin" / L"res" / L"v1" / L"assets" / spec.family / fileName,
                    };

                    for (const auto& candidate : candidates)
                    {
                        std::error_code ec;
                        if (std::filesystem::exists(candidate, ec))
                        {
                            _workspaceIconDebug(fmt::format(L"resolved png family='{}' section='{}' index={} path='{}'",
                                                            spec.family,
                                                            spec.section,
                                                            spec.index,
                                                            candidate.wstring()));
                            return candidate;
                        }
                    }

                    const auto parent = probe.parent_path();
                    if (parent == probe)
                    {
                        break;
                    }
                    probe = parent;
                }
            }

            _workspaceIconDebug(fmt::format(L"failed to resolve png family='{}' section='{}' index={} file='{}' module='{}' cwd='{}'",
                                            spec.family,
                                            spec.section,
                                            spec.index,
                                            fileName,
                                            std::filesystem::path{ modulePath }.parent_path().wstring(),
                                            std::filesystem::current_path().wstring()));
            return std::nullopt;
        }

        static winrt::Windows::UI::Xaml::Media::Imaging::SoftwareBitmapSource _getWorkspaceBitmapImageSource(const WorkspaceSpriteSpec& spec)
        {
            if (const auto mergedIconPath = _resolveWorkspaceMergedIconPath(spec))
            {
                _workspaceIconDebug(fmt::format(L"workspace bitmap using merged iconPath='{}'", mergedIconPath->c_str()));
                const auto softwareBitmap = _getGridIconBitmap(*mergedIconPath);
                if (softwareBitmap)
                {
                    _workspaceIconDebug(L"workspace bitmap merged ready");
                    return _makeSoftwareBitmapSource(softwareBitmap);
                }

                _workspaceIconDebug(L"workspace bitmap merged returned null");
            }

            const auto iconPath = _resolveWorkspaceIconFile(spec);
            if (!iconPath.has_value())
            {
                _workspaceIconDebug(fmt::format(L"png source missing family='{}' section='{}' index={}",
                                                spec.family,
                                                spec.section,
                                                spec.index));
                return nullptr;
            }

            _workspaceIconDebug(fmt::format(L"png begin family='{}' section='{}' index={} path='{}'",
                                            spec.family,
                                            spec.section,
                                            spec.index,
                                            iconPath->wstring()));

            const auto file = StorageFile::GetFileFromPathAsync(iconPath->wstring()).get();
            const auto stream = file.OpenAsync(FileAccessMode::Read).get();
            const auto decoder = BitmapDecoder::CreateAsync(stream).get();
            auto softwareBitmap = decoder.GetSoftwareBitmapAsync().get();
            if (softwareBitmap.BitmapPixelFormat() != BitmapPixelFormat::Bgra8 ||
                softwareBitmap.BitmapAlphaMode() != BitmapAlphaMode::Premultiplied)
            {
                softwareBitmap = SoftwareBitmap::Convert(softwareBitmap, BitmapPixelFormat::Bgra8, BitmapAlphaMode::Premultiplied);
            }

            auto bitmapSource = _makeSoftwareBitmapSource(softwareBitmap);
            _workspaceIconDebug(L"png ready");
            return bitmapSource;
        }
    }

// These are templates that help us figure out which BitmapIconSource/FontIconSource to use for a given IconSource.
// We have to do this because some of our code still wants to use WUX/MUX IconSources.
#pragma region BitmapIconSource
    template<typename TIconSource>
    struct BitmapIconSource
    {
    };

    template<>
    struct BitmapIconSource<winrt::Microsoft::UI::Xaml::Controls::IconSource>
    {
        using type = winrt::Microsoft::UI::Xaml::Controls::BitmapIconSource;
    };

    template<>
    struct BitmapIconSource<winrt::Windows::UI::Xaml::Controls::IconSource>
    {
        using type = winrt::Windows::UI::Xaml::Controls::BitmapIconSource;
    };
#pragma endregion

#pragma region FontIconSource
    template<typename TIconSource>
    struct FontIconSource
    {
    };

    template<>
    struct FontIconSource<winrt::Microsoft::UI::Xaml::Controls::IconSource>
    {
        using type = winrt::Microsoft::UI::Xaml::Controls::FontIconSource;
    };

    template<>
    struct FontIconSource<winrt::Windows::UI::Xaml::Controls::IconSource>
    {
        using type = winrt::Windows::UI::Xaml::Controls::FontIconSource;
    };
#pragma endregion

    // Method Description:
    // - Creates an IconSource for the given path. The icon returned is a colored
    //   icon. If we couldn't create the icon for any reason, we return an empty
    //   IconElement.
    // Template Types:
    // - <TIconSource>: The type of IconSource (MUX, WUX) to generate.
    // Arguments:
    // - path: the full, expanded path to the icon.
    // Return Value:
    // - An IconElement with its IconSource set, if possible.
    template<typename TIconSource>
    TIconSource _getColoredBitmapIcon(const winrt::hstring& path, bool monochrome)
    {
        // FontIcon uses glyphs in the private use area, whereas valid URIs only contain ASCII characters.
        // To skip throwing on Uri construction, we can quickly check if the first character is ASCII.
        if (!path.empty() && path.front() < 128)
        {
            try
            {
                winrt::Windows::Foundation::Uri iconUri{ path };
                typename BitmapIconSource<TIconSource>::type iconSource;
                // Make sure to set this to false, so we keep the RGB data of the
                // image. Otherwise, the icon will be white for all the
                // non-transparent pixels in the image.
                iconSource.ShowAsMonochrome(monochrome);
                iconSource.UriSource(iconUri);
                return iconSource;
            }
            CATCH_LOG();
        }

        return nullptr;
    }

    static winrt::hstring _expandIconPath(const hstring& iconPath)
    {
        if (iconPath.empty())
        {
            return iconPath;
        }
        winrt::hstring envExpandedPath{ wil::ExpandEnvironmentStringsW<std::wstring>(iconPath.c_str()) };
        return envExpandedPath;
    }

    // Method Description:
    // - Creates an IconSource for the given path.
    //    * If the icon is a path to an image, we'll use that.
    //    * If it isn't, then we'll try and use the text as a FontIcon. If the
    //      character is in the range of symbols reserved for the Segoe MDL2
    //      Asserts, well treat it as such. Otherwise, we'll default to a Sego
    //      UI icon, so things like emoji will work.
    //    * If we couldn't create the icon for any reason, we return an empty
    //      IconElement.
    // Template Types:
    // - <TIconSource>: The type of IconSource (MUX, WUX) to generate.
    // Arguments:
    // - path: the unprocessed path to the icon.
    // Return Value:
    // - An IconElement with its IconSource set, if possible.
    template<typename TIconSource>
    TIconSource _getIconSource(const winrt::hstring& iconPath, bool monochrome)
    {
        TIconSource iconSource{ nullptr };

        if (iconPath.size() != 0)
        {
            const auto expandedIconPath{ _expandIconPath(iconPath) };
            iconSource = _getColoredBitmapIcon<TIconSource>(expandedIconPath, monochrome);

            // If we fail to set the icon source using the "icon" as a path,
            // let's try it as a symbol/emoji.
            if (!iconSource && ::Microsoft::Console::Utils::IsLikelyToBeEmojiOrSymbolIcon(iconPath))
            {
                try
                {
                    typename FontIconSource<TIconSource>::type icon;
                    const auto ch = til::at(iconPath, 0);

                    // The range of MDL2 Icons isn't explicitly defined, but
                    // we're using this based off the table on:
                    // https://docs.microsoft.com/en-us/windows/uwp/design/style/segoe-ui-symbol-font
                    const auto isMDL2Icon = ch >= L'\uE700' && ch <= L'\uF8FF';
                    if (isMDL2Icon)
                    {
                        icon.FontFamily(winrt::Windows::UI::Xaml::Media::FontFamily{ L"Segoe Fluent Icons, Segoe MDL2 Assets" });
                    }
                    else
                    {
                        // Note: you _do_ need to manually set the font here.
                        icon.FontFamily(winrt::Windows::UI::Xaml::Media::FontFamily{ L"Segoe UI" });
                    }
                    icon.FontSize(12);
                    icon.Glyph(iconPath);
                    iconSource = icon;
                }
                CATCH_LOG();
            }
        }
        if (!iconSource)
        {
            // Set the default IconSource to a BitmapIconSource with a null source
            // (instead of just nullptr) because there's a really weird crash when swapping
            // data bound IconSourceElements in a ListViewTemplate (i.e. CommandPalette).
            // Swapping between nullptr IconSources and non-null IconSources causes a crash
            // to occur, but swapping between IconSources with a null source and non-null IconSources
            // work perfectly fine :shrug:.
            typename BitmapIconSource<TIconSource>::type icon;
            icon.UriSource(nullptr);
            iconSource = icon;
        }

        return iconSource;
    }

    Windows::UI::Xaml::Controls::IconSource IconPathConverter::IconSourceWUX(const hstring& path)
    {
        //    * If the icon is a path to an image, we'll use that.
        //    * If it isn't, then we'll try and use the text as a FontIcon. If the
        //      character is in the range of symbols reserved for the Segoe MDL2
        //      Asserts, well treat it as such. Otherwise, we'll default to a Segoe
        //      UI icon, so things like emoji will work.
        return _getIconSource<Windows::UI::Xaml::Controls::IconSource>(path, false);
    }

    static Microsoft::UI::Xaml::Controls::IconSource _IconSourceMUX(const hstring& path, bool monochrome)
    {
        const auto expandedPath{ _expandIconPath(path) };
        if (_looksLikeGridIconPath(expandedPath))
        {
            try
            {
                const auto softwareBitmap = _getGridIconBitmap(expandedPath);
                if (softwareBitmap)
                {
                    MUX::Controls::ImageIconSource imageIconSource{};
                    imageIconSource.ImageSource(_makeSoftwareBitmapSource(softwareBitmap));
                    return imageIconSource;
                }
            }
            CATCH_LOG();

            return _getIconSource<Microsoft::UI::Xaml::Controls::IconSource>({}, monochrome);
        }

        if (const auto workspaceSprite = _parseWorkspaceSpriteSpec(path))
        {
            try
            {
                const auto bitmapSource = _getWorkspaceBitmapImageSource(*workspaceSprite);
                if (bitmapSource)
                {
                    MUX::Controls::ImageIconSource imageIconSource{};
                    imageIconSource.ImageSource(bitmapSource);
                    return imageIconSource;
                }
            }
            catch (const winrt::hresult_error& ex)
            {
                _workspaceIconDebug(fmt::format(L"mux exception iconPath='{}' hr=0x{:08X} message='{}'",
                                                path.c_str(),
                                                static_cast<uint32_t>(ex.code().value),
                                                ex.message().c_str()));
            }
            catch (const std::exception&)
            {
                _workspaceIconDebug(fmt::format(L"mux std::exception iconPath='{}'", path.c_str()));
            }
            catch (...)
            {
                _workspaceIconDebug(fmt::format(L"mux unknown exception iconPath='{}'", path.c_str()));
            }
        }
        return _getIconSource<Microsoft::UI::Xaml::Controls::IconSource>(path, monochrome);
    }

    static SoftwareBitmap _convertToSoftwareBitmap(HICON hicon,
                                                   BitmapPixelFormat pixelFormat,
                                                   BitmapAlphaMode alphaMode,
                                                   gsl::not_null<IWICImagingFactory*> imagingFactory)
    {
        // Load the icon into an IWICBitmap
        wil::com_ptr<IWICBitmap> iconBitmap;
        THROW_IF_FAILED(imagingFactory->CreateBitmapFromHICON(hicon, iconBitmap.put()));

        return _convertToSoftwareBitmap(iconBitmap.get(), pixelFormat, alphaMode);
    }

    static SoftwareBitmap _getBitmapFromIconFileAsync(const winrt::hstring& iconPath,
                                                      int32_t iconIndex,
                                                      uint32_t iconSize)
    {
        wil::unique_hicon hicon;
        LOG_IF_FAILED(SHDefExtractIcon(iconPath.c_str(), iconIndex, 0, &hicon, nullptr, iconSize));

        if (!hicon)
        {
            return nullptr;
        }

        wil::com_ptr<IWICImagingFactory> wicImagingFactory;
        THROW_IF_FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&wicImagingFactory)));

        return _convertToSoftwareBitmap(hicon.get(),
                                        BitmapPixelFormat::Bgra8,
                                        BitmapAlphaMode::Premultiplied,
                                        wicImagingFactory.get());
    }

    // Method Description:
    // - Attempt to get the icon index from the icon path provided
    // Arguments:
    // - iconPath: the full icon path, including the index if present
    // - iconPathWithoutIndex: the place to store the icon path, sans the index if present
    // Return Value:
    // - nullopt if the iconPath is not an exe/dll/lnk file in the first place
    // - 0 if the iconPath is an exe/dll/lnk file but does not contain an index (i.e. we default
    //   to the first icon in the file)
    // - the icon index if the iconPath is an exe/dll/lnk file and contains an index
    static std::optional<int> _getIconIndex(const winrt::hstring& iconPath, std::wstring_view& iconPathWithoutIndex)
    {
        const auto pathView = std::wstring_view{ iconPath };
        // Does iconPath have a comma in it? If so, split the string on the
        // comma and look for the index and extension.
        const auto commaIndex = pathView.find(L',');

        // split the path on the comma
        iconPathWithoutIndex = pathView.substr(0, commaIndex);

        // It's an exe, dll, or lnk, so we need to extract the icon from the file.
        if (!til::ends_with(iconPathWithoutIndex, L".exe") &&
            !til::ends_with(iconPathWithoutIndex, L".dll") &&
            !til::ends_with(iconPathWithoutIndex, L".lnk"))
        {
            return std::nullopt;
        }

        if (commaIndex != std::wstring::npos)
        {
            // Convert the string iconIndex to a signed int to support negative numbers which represent an Icon's ID.
            return til::parse_signed<int>(pathView.substr(commaIndex + 1));
        }

        // We had a binary path, but no index. Default to 0.
        return 0;
    }

    static winrt::Windows::UI::Xaml::Media::Imaging::SoftwareBitmapSource _getImageIconSourceForBinary(std::wstring_view iconPathWithoutIndex,
                                                                                                       int index)
    {
        // Try:
        // * c:\Windows\System32\SHELL32.dll, 210
        // * c:\Windows\System32\notepad.exe, 0
        // * C:\Program Files\PowerShell\6-preview\pwsh.exe, 0 (this doesn't exist for me)
        // * C:\Program Files\PowerShell\7\pwsh.exe, 0

        const auto swBitmap{ _getBitmapFromIconFileAsync(winrt::hstring{ iconPathWithoutIndex }, index, 32) };
        if (swBitmap == nullptr)
        {
            return nullptr;
        }

        return _makeSoftwareBitmapSource(swBitmap);
    }

    MUX::Controls::IconSource IconPathConverter::IconSourceMUX(const winrt::hstring& iconPath,
                                                               const bool monochrome)
    {
        std::wstring_view iconPathWithoutIndex;
        const auto indexOpt = _getIconIndex(iconPath, iconPathWithoutIndex);
        if (!indexOpt.has_value())
        {
            return _IconSourceMUX(iconPath, monochrome);
        }

        const auto bitmapSource = _getImageIconSourceForBinary(iconPathWithoutIndex, indexOpt.value());

        MUX::Controls::ImageIconSource imageIconSource{};
        imageIconSource.ImageSource(bitmapSource);

        return imageIconSource;
    }

    Windows::UI::Xaml::Controls::IconElement IconPathConverter::IconWUX(const winrt::hstring& iconPath)
    {
        const auto expandedIconPath{ _expandIconPath(iconPath) };
        if (_looksLikeGridIconPath(expandedIconPath))
        {
            try
            {
                const auto softwareBitmap = _getGridIconBitmap(expandedIconPath);
                if (softwareBitmap)
                {
                    winrt::Microsoft::UI::Xaml::Controls::ImageIcon icon{};
                    icon.Source(_makeSoftwareBitmapSource(softwareBitmap));
                    icon.Width(32);
                    icon.Height(32);
                    return icon;
                }
            }
            CATCH_LOG();

            auto source = IconSourceWUX({});
            Controls::IconSourceElement icon;
            icon.IconSource(source);
            return icon;
        }

        if (const auto workspaceSprite = _parseWorkspaceSpriteSpec(iconPath))
        {
            try
            {
                const auto bitmapSource = _getWorkspaceBitmapImageSource(*workspaceSprite);
                if (bitmapSource)
                {
                    winrt::Microsoft::UI::Xaml::Controls::ImageIcon icon{};
                    icon.Source(bitmapSource);
                    icon.Width(32);
                    icon.Height(32);
                    return icon;
                }
            }
            catch (const winrt::hresult_error& ex)
            {
                _workspaceIconDebug(fmt::format(L"wux exception iconPath='{}' hr=0x{:08X} message='{}'",
                                                iconPath.c_str(),
                                                static_cast<uint32_t>(ex.code().value),
                                                ex.message().c_str()));
            }
            catch (const std::exception&)
            {
                _workspaceIconDebug(fmt::format(L"wux std::exception iconPath='{}'", iconPath.c_str()));
            }
            catch (...)
            {
                _workspaceIconDebug(fmt::format(L"wux unknown exception iconPath='{}'", iconPath.c_str()));
            }
        }

        std::wstring_view iconPathWithoutIndex;
        const auto indexOpt = _getIconIndex(iconPath, iconPathWithoutIndex);
        if (!indexOpt.has_value())
        {
            auto source = IconSourceWUX(iconPath);
            Controls::IconSourceElement icon;
            icon.IconSource(source);
            return icon;
        }

        const auto bitmapSource = _getImageIconSourceForBinary(iconPathWithoutIndex, indexOpt.value());

        winrt::Microsoft::UI::Xaml::Controls::ImageIcon icon{};
        icon.Source(bitmapSource);
        icon.Width(32);
        icon.Height(32);
        return icon;
    }
}
