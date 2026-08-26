#include "pch.h"
#include "ResourceString.h"
#include "ResourceString.g.cpp"

#include <winrt/Windows.ApplicationModel.Resources.Core.h>

namespace winrt::Microsoft::Terminal::UI::implementation
{
    winrt::Windows::Foundation::IInspectable ResourceString::ProvideValue()
    {
        if (tree_.empty())
        {
            return nullptr;
        }

        auto loader{ winrt::Windows::ApplicationModel::Resources::ResourceLoader::GetForCurrentView(tree_ + L"/Resources") };
        return winrt::box_value(loader.GetString(name_));
    }
}
