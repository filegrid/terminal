    namespace
    {
        constexpr int _splitWeightUnits{ 20 };

        std::wstring _legacyCommandId(const WorkspaceNode& node)
        {
            return node.Id.empty() ? L"legacy-command" : node.Id + L":legacy-command";
        }

        std::vector<double> _quantizeSplitWeights(std::vector<double> weights, const size_t count)
        {
            if (count == 0 || count > static_cast<size_t>(_splitWeightUnits))
            {
                return {};
            }
            if (weights.size() != count)
            {
                weights.assign(count, 1.0 / static_cast<double>(count));
            }

            double total{};
            for (const auto weight : weights)
            {
                if (weight > 0.0 && std::isfinite(weight))
                {
                    total += weight;
                }
            }
            if (total <= 0.0)
            {
                weights.assign(count, 1.0 / static_cast<double>(count));
                total = 1.0;
            }

            std::vector<int> units(count, 1);
            std::vector<std::pair<double, size_t>> remainders;
            remainders.reserve(count);
            int assigned = static_cast<int>(count);
            for (size_t i = 0; i < count; ++i)
            {
                const auto normalized = std::max(0.0, weights[i]) / total;
                const auto ideal = normalized * static_cast<double>(_splitWeightUnits - static_cast<int>(count));
                const auto extra = static_cast<int>(std::floor(ideal));
                units[i] += extra;
                assigned += extra;
                remainders.emplace_back(ideal - extra, i);
            }
            std::stable_sort(remainders.begin(), remainders.end(), [](const auto& lhs, const auto& rhs) {
                return lhs.first > rhs.first;
            });
            for (int i = assigned; i < _splitWeightUnits; ++i)
            {
                ++units[remainders[static_cast<size_t>(i - assigned) % remainders.size()].second];
            }

            std::vector<double> result;
            result.reserve(count);
            for (const auto unit : units)
            {
                result.emplace_back(static_cast<double>(unit) / _splitWeightUnits);
            }
            return result;
        }

        void _syncLegacyCommandFields(WorkspaceNode& node)
        {
            if (!node.Commands.empty())
            {
                // startupAction is intentionally kept in sync as the old
                // reader's single-command fallback. Name is a directory-backed
                // node identity and must not be overwritten here.
                node.StartupAction = node.Commands.front().Command;
                if (!node.Commands.front().Icon.empty())
                {
                    node.Icon = node.Commands.front().Icon;
                }
            }
        }
    }

    std::vector<WorkspaceNodeCommand> EffectiveWorkspaceNodeCommands(const WorkspaceNode& node)
    {
        if (!node.Commands.empty())
        {
            return node.Commands;
        }
        return { WorkspaceNodeCommand{
            .Id = _legacyCommandId(node),
            .Icon = node.Icon,
            .Name = node.Name,
            .Command = node.StartupAction,
        } };
    }

    WorkspaceMultiWindowValidationResult ValidateWorkspaceNodeMultiWindowConfig(const WorkspaceNode& node)
    {
        const auto commands = EffectiveWorkspaceNodeCommands(node);
        if (commands.size() < WorkspaceNodeMinCommandCount || commands.size() > WorkspaceNodeMaxCommandCount)
        {
            return { false, L"A workspace node must contain one to three commands." };
        }
        std::unordered_set<std::wstring> ids;
        for (const auto& command : commands)
        {
            if (command.Id.empty() || !ids.emplace(command.Id).second)
            {
                return { false, L"Each workspace command needs a unique id." };
            }
        }
        return { true, {} };
    }

    void NormalizeWorkspaceNodeMultiWindowConfig(WorkspaceNode& node)
    {
        if (node.Commands.empty())
        {
            return;
        }
        node.Commands.resize(std::min(node.Commands.size(), WorkspaceNodeMaxCommandCount));
        std::unordered_set<std::wstring> ids;
        for (size_t i = 0; i < node.Commands.size(); ++i)
        {
            auto& command = node.Commands[i];
            if (command.Id.empty() || !ids.emplace(command.Id).second)
            {
                command.Id = node.Id + L":command-" + std::to_wstring(i + 1);
                while (!ids.emplace(command.Id).second)
                {
                    command.Id.push_back(L'_');
                }
            }
        }
        node.MultiWindowPreference.SplitWeights = _quantizeSplitWeights(
            node.MultiWindowPreference.SplitWeights, node.Commands.size());
        _syncLegacyCommandFields(node);
    }

    bool SetWorkspaceNodeCommands(WorkspaceNode& node, std::vector<WorkspaceNodeCommand> commands)
    {
        // This is an edit intent, not a compatibility read. An explicit empty
        // list must be rejected instead of silently falling back to the legacy
        // startupAction field.
        if (commands.empty())
        {
            return false;
        }
        WorkspaceNode proposed = node;
        proposed.Commands = std::move(commands);
        if (!ValidateWorkspaceNodeMultiWindowConfig(proposed).IsValid)
        {
            return false;
        }

        const auto previous = EffectiveWorkspaceNodeCommands(node);
        const auto previousWeights = _quantizeSplitWeights(node.MultiWindowPreference.SplitWeights, previous.size());
        std::unordered_map<std::wstring, double> oldWeights;
        for (size_t i = 0; i < previous.size(); ++i)
        {
            oldWeights.emplace(previous[i].Id, previousWeights[i]);
        }
        size_t newCount{};
        double retainedTotal{};
        std::vector<double> migrated;
        migrated.reserve(proposed.Commands.size());
        for (const auto& command : proposed.Commands)
        {
            if (const auto it = oldWeights.find(command.Id); it != oldWeights.end())
            {
                retainedTotal += it->second;
                migrated.emplace_back(it->second);
            }
            else
            {
                ++newCount;
                migrated.emplace_back(0.0);
            }
        }
        if (newCount != 0 && retainedTotal > 0.0)
        {
            const auto newTotal = static_cast<double>(newCount) / proposed.Commands.size();
            for (const auto& command : proposed.Commands)
            {
                const auto index = static_cast<size_t>(&command - proposed.Commands.data());
                if (const auto it = oldWeights.find(command.Id); it != oldWeights.end())
                {
                    migrated[index] = it->second / retainedTotal * (1.0 - newTotal);
                }
                else
                {
                    migrated[index] = newTotal / newCount;
                }
            }
        }
        proposed.MultiWindowPreference.SplitWeights = _quantizeSplitWeights(std::move(migrated), proposed.Commands.size());
        NormalizeWorkspaceNodeMultiWindowConfig(proposed);
        node = std::move(proposed);
        return true;
    }

    bool ReorderWorkspaceNodeCommands(WorkspaceNode& node, const std::vector<std::wstring>& orderedCommandIds)
    {
        auto commands = EffectiveWorkspaceNodeCommands(node);
        if (orderedCommandIds.size() != commands.size())
        {
            return false;
        }
        std::unordered_map<std::wstring, WorkspaceNodeCommand> byId;
        for (auto& command : commands)
        {
            byId.emplace(command.Id, std::move(command));
        }
        std::vector<WorkspaceNodeCommand> reordered;
        reordered.reserve(orderedCommandIds.size());
        for (const auto& id : orderedCommandIds)
        {
            const auto it = byId.find(id);
            if (it == byId.end())
            {
                return false;
            }
            reordered.emplace_back(std::move(it->second));
            byId.erase(it);
        }
        return SetWorkspaceNodeCommands(node, std::move(reordered));
    }

    bool SetWorkspaceNodeSplitWeights(WorkspaceNode& node, const std::vector<double>& weights)
    {
        const auto count = EffectiveWorkspaceNodeCommands(node).size();
        if (weights.size() != count)
        {
            return false;
        }
        node.MultiWindowPreference.SplitWeights = _quantizeSplitWeights(weights, count);
        return true;
    }

    bool ResizeWorkspaceNodeSplit(WorkspaceNode& node, const size_t dividerIndex, const double leftRatio)
    {
        const auto count = EffectiveWorkspaceNodeCommands(node).size();
        if (dividerIndex + 1 >= count || !std::isfinite(leftRatio))
        {
            return false;
        }
        auto weights = _quantizeSplitWeights(node.MultiWindowPreference.SplitWeights, count);
        const auto pairUnits = static_cast<int>(std::lround((weights[dividerIndex] + weights[dividerIndex + 1]) * _splitWeightUnits));
        const auto leftUnits = std::clamp(static_cast<int>(std::lround(leftRatio * pairUnits)), 1, pairUnits - 1);
        weights[dividerIndex] = static_cast<double>(leftUnits) / _splitWeightUnits;
        weights[dividerIndex + 1] = static_cast<double>(pairUnits - leftUnits) / _splitWeightUnits;
        node.MultiWindowPreference.SplitWeights = std::move(weights);
        return true;
    }

    WorkspaceSplitLayoutResult CalculateWorkspaceNodeSplitLayout(const WorkspaceNode& node,
                                                                  const double availableWidth,
                                                                  const double minimumWindowWidth)
    {
        const auto commands = EffectiveWorkspaceNodeCommands(node);
        if (commands.empty())
        {
            return {};
        }

        const auto minimum = std::max(0.0, minimumWindowWidth);
        const auto available = std::max(0.0, availableWidth);
        const auto required = minimum * commands.size();
        WorkspaceSplitLayoutResult result;
        result.RequiresHorizontalScroll = available < required;
        if (result.RequiresHorizontalScroll)
        {
            result.WindowWidths.assign(commands.size(), minimum);
            return result;
        }

        const auto weights = _quantizeSplitWeights(node.MultiWindowPreference.SplitWeights, commands.size());
        result.WindowWidths.reserve(weights.size());
        for (const auto weight : weights)
        {
            result.WindowWidths.emplace_back(weight * available);
        }
        return result;
    }

    bool SetWorkspaceCommandRuntimeTitle(const WorkspaceNode& node,
                                         WorkspaceNodeSessionState& session,
                                         const std::wstring_view commandId,
                                         std::wstring title)
    {
        const auto commands = EffectiveWorkspaceNodeCommands(node);
        if (const auto configured = std::find_if(commands.begin(), commands.end(), [&](const auto& command) {
                return command.Id == commandId;
            }); configured == commands.end())
        {
            return false;
        }

        const auto runtime = std::find_if(session.Commands.begin(), session.Commands.end(), [&](const auto& item) {
            return item.CommandId == commandId;
        });
        if (runtime == session.Commands.end())
        {
            session.Commands.emplace_back(WorkspaceCommandRuntimeState{ std::wstring{ commandId }, std::move(title), true });
        }
        else
        {
            runtime->Title = std::move(title);
        }
        return true;
    }

    bool SetWorkspaceNodeActiveCommand(const WorkspaceNode& node,
                                       WorkspaceNodeSessionState& session,
                                       const std::wstring_view commandId)
    {
        const auto commands = EffectiveWorkspaceNodeCommands(node);
        if (std::none_of(commands.begin(), commands.end(), [&](const auto& command) {
                return command.Id == commandId;
            }))
        {
            return false;
        }
        session.ActiveCommandId.assign(commandId);
        return true;
    }

    std::wstring ResolveWorkspaceCommandDisplayName(const WorkspaceNodeCommand& command,
                                                    const WorkspaceNodeSessionState& session)
    {
        if (!command.Name.empty())
        {
            return command.Name;
        }
        if (const auto runtime = std::find_if(session.Commands.begin(), session.Commands.end(), [&](const auto& item) {
                return item.CommandId == command.Id;
            }); runtime != session.Commands.end() && !runtime->Title.empty())
        {
            return runtime->Title;
        }
        return command.Command;
    }
