// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include <WexTestClass.h>

#include "..\..\..\..\ext\src\glue\chat\TerminalInputHarness.h"
#include "..\..\..\..\ext\src\glue\chat\WorkspaceChatStateHelpers.h"

using namespace WEX::Logging;
using namespace WEX::TestExecution;

namespace TerminalAppLocalTests
{
    class TerminalInputHarnessTests
    {
        BEGIN_TEST_CLASS(TerminalInputHarnessTests)
            TEST_CLASS_PROPERTY(L"RunAs", L"UAP")
            TEST_CLASS_PROPERTY(L"UAP:AppXManifest", L"TestHostAppXManifest.xml")
        END_TEST_CLASS()

        TEST_METHOD(SplitInputIntoSingleLines);
        TEST_METHOD(TracksWindowsCdChain);
        TEST_METHOD(TracksPowerShellSetLocationChain);
        TEST_METHOD(TracksPosixPushdPopd);
        TEST_METHOD(TracksWindowsDriveSwitch);
        TEST_METHOD(InfersOperatingSystemAndShell);
        TEST_METHOD(UsesReportedWorkingDirectoryAsAnchor);
        TEST_METHOD(UsesLinuxOsHintForAbsolutePosixPaths);
        TEST_METHOD(TracksWindowsPathsInsideSshSession);
        TEST_METHOD(BuildsTerminalInputCaptureResultFromPendingInput);
        TEST_METHOD(BuildsTerminalInputCaptureResultFromInputOverride);
    };

    using terminal::workspacechat::BuildTerminalInputCaptureResult;
    using terminal::workspacechat::SplitTerminalInputLines;
    using terminal::workspacechat::TerminalCaptureState;
    using terminal::workspacechat::TerminalInputState;
    using terminal::workspacechat::TrackTerminalInput;

    void TerminalInputHarnessTests::SplitInputIntoSingleLines()
    {
        const auto lines = SplitTerminalInputLines(L"git status\r\ncd test\n\npwd\r");
        VERIFY_ARE_EQUAL(3u, lines.size());
        VERIFY_ARE_EQUAL(L"git status", lines[0].c_str());
        VERIFY_ARE_EQUAL(L"cd test", lines[1].c_str());
        VERIFY_ARE_EQUAL(L"pwd", lines[2].c_str());
    }

    void TerminalInputHarnessTests::TracksWindowsCdChain()
    {
        TerminalInputState state;
        const auto snapshot = TrackTerminalInput(state, L"cd /d D:\\work && git status", L"C:\\repo");
        VERIFY_ARE_EQUAL(L"D:\\work", snapshot.WorkingDirectory.c_str());
        VERIFY_ARE_EQUAL(L"git status", snapshot.Command.c_str());
    }

    void TerminalInputHarnessTests::TracksPowerShellSetLocationChain()
    {
        TerminalInputState state;
        const auto snapshot = TrackTerminalInput(state, L"Set-Location -Path .\\sub; dir", L"C:\\repo");
        VERIFY_ARE_EQUAL(L"C:\\repo\\sub", snapshot.WorkingDirectory.c_str());
        VERIFY_ARE_EQUAL(L"dir", snapshot.Command.c_str());
        VERIFY_ARE_EQUAL(L"windows", snapshot.OperatingSystem.c_str());
        VERIFY_ARE_EQUAL(L"powershell", snapshot.ShellType.c_str());
    }

    void TerminalInputHarnessTests::TracksPosixPushdPopd()
    {
        TerminalInputState state;
        auto snapshot = TrackTerminalInput(state, L"pushd project && ls", L"/home/dev");
        VERIFY_ARE_EQUAL(L"/home/dev/project", snapshot.WorkingDirectory.c_str());
        VERIFY_ARE_EQUAL(L"ls", snapshot.Command.c_str());

        snapshot = TrackTerminalInput(state, L"popd; pwd");
        VERIFY_ARE_EQUAL(L"/home/dev", snapshot.WorkingDirectory.c_str());
        VERIFY_ARE_EQUAL(L"pwd", snapshot.Command.c_str());
        VERIFY_ARE_EQUAL(L"linux", snapshot.OperatingSystem.c_str());
    }

    void TerminalInputHarnessTests::TracksWindowsDriveSwitch()
    {
        TerminalInputState state;
        auto snapshot = TrackTerminalInput(state, L"cd /d D:\\work", L"C:\\repo");
        VERIFY_ARE_EQUAL(L"D:\\work", snapshot.WorkingDirectory.c_str());

        snapshot = TrackTerminalInput(state, L"C:");
        VERIFY_ARE_EQUAL(L"C:\\repo", snapshot.WorkingDirectory.c_str());
        VERIFY_ARE_EQUAL(L"windows", snapshot.OperatingSystem.c_str());
    }

    void TerminalInputHarnessTests::InfersOperatingSystemAndShell()
    {
        TerminalInputState state;
        auto snapshot = TrackTerminalInput(state, L"ssh dev@box");
        VERIFY_ARE_EQUAL(L"linux", snapshot.OperatingSystem.c_str());
        VERIFY_ARE_EQUAL(L"ssh", snapshot.ShellType.c_str());

        snapshot = TrackTerminalInput(state, L"chdir D:\\tools", L"C:\\repo");
        VERIFY_ARE_EQUAL(L"windows", snapshot.OperatingSystem.c_str());
        VERIFY_ARE_EQUAL(L"cmd", snapshot.ShellType.c_str());
    }

    void TerminalInputHarnessTests::UsesReportedWorkingDirectoryAsAnchor()
    {
        TerminalInputState state;
        auto snapshot = TrackTerminalInput(state, L"cd sub", L"C:\\one");
        VERIFY_ARE_EQUAL(L"C:\\one\\sub", snapshot.WorkingDirectory.c_str());

        snapshot = TrackTerminalInput(state, L"git status", L"C:\\two");
        VERIFY_ARE_EQUAL(L"C:\\two", snapshot.WorkingDirectory.c_str());
        VERIFY_ARE_EQUAL(L"git status", snapshot.Command.c_str());
    }

    void TerminalInputHarnessTests::UsesLinuxOsHintForAbsolutePosixPaths()
    {
        TerminalInputState state;
        state.LastWorkingDirectory = L"C:\\repo";
        state.OperatingSystem = L"linux";

        const auto snapshot = TrackTerminalInput(state, L"cd /app && pwd");
        VERIFY_ARE_EQUAL(L"/app", snapshot.WorkingDirectory.c_str());
        VERIFY_ARE_EQUAL(L"pwd", snapshot.Command.c_str());
        VERIFY_ARE_EQUAL(L"linux", snapshot.OperatingSystem.c_str());
    }

    void TerminalInputHarnessTests::TracksWindowsPathsInsideSshSession()
    {
        TerminalInputState state;
        state.ShellType = L"ssh";

        auto snapshot = TrackTerminalInput(state, L"cd /d D:\\work");
        VERIFY_ARE_EQUAL(L"D:\\work", snapshot.WorkingDirectory.c_str());
        VERIFY_ARE_EQUAL(L"windows", snapshot.OperatingSystem.c_str());
        VERIFY_ARE_EQUAL(L"ssh", snapshot.ShellType.c_str());

        snapshot = TrackTerminalInput(state, L"C:");
        VERIFY_ARE_EQUAL(L"C:\\", snapshot.WorkingDirectory.c_str());
        VERIFY_ARE_EQUAL(L"windows", snapshot.OperatingSystem.c_str());
        VERIFY_ARE_EQUAL(L"ssh", snapshot.ShellType.c_str());
    }

    void TerminalInputHarnessTests::BuildsTerminalInputCaptureResultFromPendingInput()
    {
        TerminalCaptureState state;
        state.PendingInput = L"cd sub\r";

        const auto result = BuildTerminalInputCaptureResult(state,
                                                            {},
                                                            L"cd sub",
                                                            L"C:\\repo",
                                                            L"prompt> ");

        VERIFY_IS_TRUE(result.StateChanged);
        VERIFY_ARE_EQUAL(1u, result.Entries.size());
        VERIFY_ARE_EQUAL(L"cd sub", result.FlushPlan.NormalizedInput.c_str());
        VERIFY_ARE_EQUAL(L"cd sub", result.Entries[0].Text.c_str());
        VERIFY_ARE_EQUAL(L"C:\\repo\\sub", result.Entries[0].Snapshot.WorkingDirectory.c_str());
        VERIFY_ARE_EQUAL(L"prompt> ", state.LastBufferSnapshot.c_str());
        VERIFY_IS_TRUE(state.HasBufferSnapshot);
        VERIFY_IS_TRUE(state.PendingInput.empty());
    }

    void TerminalInputHarnessTests::BuildsTerminalInputCaptureResultFromInputOverride()
    {
        TerminalCaptureState state;
        state.PendingInput = L"stale";

        const auto result = BuildTerminalInputCaptureResult(state,
                                                            L"git status\r",
                                                            {},
                                                            L"C:\\repo",
                                                            {});

        VERIFY_IS_TRUE(result.StateChanged);
        VERIFY_ARE_EQUAL(5u, result.PendingInput.PendingInputLength);
        VERIFY_ARE_EQUAL(L"git status\r", result.PendingInput.InputText.c_str());
        VERIFY_ARE_EQUAL(1u, result.Entries.size());
        VERIFY_ARE_EQUAL(L"git status", result.Entries[0].Text.c_str());
        VERIFY_ARE_EQUAL(L"C:\\repo", result.Entries[0].Snapshot.WorkingDirectory.c_str());
        VERIFY_ARE_EQUAL(L"git status", result.Entries[0].Snapshot.Command.c_str());
        VERIFY_IS_TRUE(state.PendingInput.empty());
    }
}
