// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"

namespace winrt::Microsoft::Terminal::Control
{
    // This test DLL consumes the public KeyChord projection. Linking
    // TerminalControlLib also supplies its implementation-side constructor,
    // which collides with the projection-side constructor emitted by
    // KeyChordSerialization.cpp. Provide the bool convenience overload here
    // and invoke the consumer-side activation factory directly instead.
    KeyChord::KeyChord(const winrt::Windows::System::VirtualKeyModifiers& modifiers, const int32_t vkey, const int32_t scanCode) :
        KeyChord(winrt::impl::call_factory<KeyChord, IKeyChordFactory>([&](IKeyChordFactory const& factory) {
            return factory.CreateInstance(modifiers, vkey, scanCode);
        }))
    {
    }

    KeyChord::KeyChord(const bool ctrl, const bool alt, const bool shift, const bool win, const int32_t vkey, const int32_t scanCode) :
        KeyChord(winrt::impl::call_factory<KeyChord, IKeyChordFactory>([&](IKeyChordFactory const& factory) {
            return factory.CreateInstance2(ctrl, alt, shift, win, vkey, scanCode);
        }))
    {
    }
}
