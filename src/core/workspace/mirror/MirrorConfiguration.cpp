// Copyright (c) Tommy Yan <tommy.yxd@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-only

#include "MirrorConfiguration.h"

#include <limits>

    namespace
    {
        bool _parseBoundedUnsigned(const std::wstring_view value, uint32_t& target)
        {
            try
            {
                size_t parsed{};
                const auto number = std::stoull(std::wstring{ value }, &parsed);
                if (parsed != value.size() || number == 0 || number > std::numeric_limits<uint32_t>::max())
                {
                    return false;
                }
                target = static_cast<uint32_t>(number);
                return true;
            }
            catch (...)
            {
                return false;
            }
        }
    }

    bool ApplyWorkspaceMirrorConfigurationField(WorkspaceNodeMirrorConfiguration& configuration,
                                                const std::wstring_view key,
                                                const std::wstring_view value)
    {
        if (key == L"mirrorMode")
        {
            configuration.Mode = value == L"node-session" ? WorkspaceNodeMirrorMode::NodeSession : WorkspaceNodeMirrorMode::Disabled;
            return true;
        }
        if (key == L"mirrorMaximumEvents")
        {
            return _parseBoundedUnsigned(value, configuration.MaximumEvents);
        }
        if (key == L"mirrorMaximumCheckpoints")
        {
            return _parseBoundedUnsigned(value, configuration.MaximumCheckpoints);
        }
        return false;
    }

    std::wstring WorkspaceMirrorModeToString(const WorkspaceNodeMirrorMode mode)
    {
        return mode == WorkspaceNodeMirrorMode::NodeSession ? L"node-session" : L"disabled";
    }
