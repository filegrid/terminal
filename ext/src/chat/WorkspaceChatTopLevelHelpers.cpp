    static constexpr double WorkspaceChatDefaultExpandedHeight{ 36.0 };
    static constexpr double WorkspaceChatMinimumExpandedHeight{ 36.0 };
    static constexpr auto WorkspaceChatSubmitKeyDelay{ 200ms };
    static constexpr auto WorkspaceChatKeyboardSubmitInterKeyDelay{ 80ms };
    static constexpr WORD WorkspaceChatSubmitScanCode{ 0x1c };
    static constexpr auto WorkspaceStartupInitialReplayDelay{ 400ms };
    static constexpr auto WorkspaceStartupCommandReplayDelay{ 150ms };
    static constexpr auto WorkspaceStartupSshTtyReadyPollDelay{ 250ms };
    static constexpr auto WorkspaceStartupSshTtyReadyTimeout{ 8s };

    std::string _visibilityName(const WUX::Visibility value)
    {
        switch (value)
        {
        case WUX::Visibility::Visible:
            return "visible";
        case WUX::Visibility::Collapsed:
            return "collapsed";
        default:
            return "unknown";
        }
    }

    void _logWorkspaceChatDiagnostic(const std::wstring_view eventName, const Json::Value& payload)
    {
        std::ignore = terminal::workspacechat::AppendWorkspaceDiagnosticLog(eventName, payload);
    }

    bool _sendKeyboardEnterToFocusedWindow()
    {
        INPUT inputs[2]{};
        inputs[0].type = INPUT_KEYBOARD;
        inputs[0].ki.wVk = VK_RETURN;
        inputs[0].ki.wScan = WorkspaceChatSubmitScanCode;
        inputs[0].ki.dwFlags = KEYEVENTF_SCANCODE;

        inputs[1] = inputs[0];
        inputs[1].ki.dwFlags = KEYEVENTF_SCANCODE | KEYEVENTF_KEYUP;

        return ::SendInput(gsl::narrow_cast<UINT>(std::size(inputs)), inputs, sizeof(INPUT)) == std::size(inputs);
    }

    void _appendPhysicalKeyboardInput(std::vector<INPUT>& inputs, const WORD virtualKey, const HKL keyboardLayout, const bool keyUp, const bool extended = false)
    {
        INPUT input{};
        input.type = INPUT_KEYBOARD;
        input.ki.wScan = gsl::narrow_cast<WORD>(MapVirtualKeyExW(virtualKey, MAPVK_VK_TO_VSC_EX, keyboardLayout));
        input.ki.dwFlags = KEYEVENTF_SCANCODE | (keyUp ? KEYEVENTF_KEYUP : 0) | (extended ? KEYEVENTF_EXTENDEDKEY : 0);
        inputs.push_back(input);
    }

    bool _sendKeyboardTextToFocusedWindow(const std::wstring_view text, wchar_t& unmappedCharacter)
    {
        const auto keyboardLayout = GetKeyboardLayout(0);
        std::vector<INPUT> inputs;
        inputs.reserve(text.size() * 8);
        unmappedCharacter = L'\0';

        for (const auto ch : text)
        {
            const auto keyInfo = VkKeyScanExW(ch, keyboardLayout);
            if (keyInfo == -1)
            {
                unmappedCharacter = ch;
                return false;
            }

            const auto virtualKey = gsl::narrow_cast<WORD>(LOBYTE(keyInfo));
            const auto shiftState = HIBYTE(keyInfo);
            const auto needsShift = (shiftState & 0x01) != 0;
            const auto needsCtrl = (shiftState & 0x02) != 0;
            const auto needsAlt = (shiftState & 0x04) != 0;

            if (needsShift)
            {
                _appendPhysicalKeyboardInput(inputs, VK_LSHIFT, keyboardLayout, false);
            }
            if (needsCtrl)
            {
                _appendPhysicalKeyboardInput(inputs, VK_LCONTROL, keyboardLayout, false);
            }
            if (needsAlt)
            {
                _appendPhysicalKeyboardInput(inputs, VK_LMENU, keyboardLayout, false);
            }

            _appendPhysicalKeyboardInput(inputs, virtualKey, keyboardLayout, false);
            _appendPhysicalKeyboardInput(inputs, virtualKey, keyboardLayout, true);

            if (needsAlt)
            {
                _appendPhysicalKeyboardInput(inputs, VK_LMENU, keyboardLayout, true);
            }
            if (needsCtrl)
            {
                _appendPhysicalKeyboardInput(inputs, VK_LCONTROL, keyboardLayout, true);
            }
            if (needsShift)
            {
                _appendPhysicalKeyboardInput(inputs, VK_LSHIFT, keyboardLayout, true);
            }
        }

        return inputs.empty() || ::SendInput(gsl::narrow_cast<UINT>(inputs.size()), inputs.data(), sizeof(INPUT)) == inputs.size();
    }

    std::wstring _releasePressedKeyboardModifiers()
    {
        std::vector<INPUT> inputs;
        std::wstring released;

        const auto appendRelease = [&](const int virtualKey, const std::wstring_view name) {
            if ((::GetAsyncKeyState(virtualKey) & 0x8000) == 0)
            {
                return false;
            }

            INPUT keyUp{};
            keyUp.type = INPUT_KEYBOARD;
            keyUp.ki.wVk = gsl::narrow_cast<WORD>(virtualKey);
            keyUp.ki.dwFlags = KEYEVENTF_KEYUP;
            inputs.push_back(keyUp);

            if (!released.empty())
            {
                released.append(L",");
            }
            released.append(name);
            return true;
        };

        const auto ctrlReleased = appendRelease(VK_LCONTROL, L"lctrl") | appendRelease(VK_RCONTROL, L"rctrl");
        if (!ctrlReleased)
        {
            appendRelease(VK_CONTROL, L"ctrl");
        }

        const auto shiftReleased = appendRelease(VK_LSHIFT, L"lshift") | appendRelease(VK_RSHIFT, L"rshift");
        if (!shiftReleased)
        {
            appendRelease(VK_SHIFT, L"shift");
        }

        const auto altReleased = appendRelease(VK_LMENU, L"lalt") | appendRelease(VK_RMENU, L"ralt");
        if (!altReleased)
        {
            appendRelease(VK_MENU, L"alt");
        }

        appendRelease(VK_LWIN, L"lwin");
        appendRelease(VK_RWIN, L"rwin");

        if (!inputs.empty())
        {
            ::SendInput(gsl::narrow_cast<UINT>(inputs.size()), inputs.data(), sizeof(INPUT));
        }

        return released;
    }

    std::wstring _focusedElementDescriptor(const WUX::XamlRoot& root)
    {
        if (!root)
        {
            return {};
        }

        const auto focused = WUX::Input::FocusManager::GetFocusedElement(root);
        const auto frameworkElement = focused.try_as<WUX::FrameworkElement>();
        if (!frameworkElement)
        {
            return focused ? L"(non-framework-element)" : L"(no-focus)";
        }

        const auto name = frameworkElement.Name();
        if (!name.empty())
        {
            return name.c_str();
        }
        return L"(unnamed-framework-element)";
    }

    bool _isFocusedElementWithin(const WUX::XamlRoot& root, const WUX::DependencyObject& ancestor)
    {
        if (!root || !ancestor)
        {
            return false;
        }

        auto focused = WUX::Input::FocusManager::GetFocusedElement(root).try_as<WUX::DependencyObject>();
        while (focused)
        {
            if (focused == ancestor)
            {
                return true;
            }
            focused = WUX::Media::VisualTreeHelper::GetParent(focused);
        }
        return false;
    }
