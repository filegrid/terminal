using Workspace = winrt::Microsoft::Terminal::Settings::Model::implementation::Workspace;
using WorkspaceManager = winrt::Microsoft::Terminal::Settings::Model::implementation::WorkspaceManager;
using WorkspaceNode = winrt::Microsoft::Terminal::Settings::Model::implementation::WorkspaceNode;
using WorkspaceNodeRuntimeRegistrationInput = winrt::Microsoft::Terminal::Settings::Model::implementation::WorkspaceNodeRuntimeRegistrationInput;
using winrt::Microsoft::Terminal::Settings::Model::implementation::EnsureWorkspaceNodeTabColors;
using winrt::Microsoft::Terminal::Settings::Model::implementation::PickWorkspacePaletteColor;
using winrt::Microsoft::Terminal::Settings::Model::implementation::ResolveWorkspaceNodeTabColor;

#include "../../../../src/glue/chat/WorkspaceChatTopLevelHelpers.cpp"

#include "../../../../src/glue/workspace/WorkspaceTerminalPageHelpers.cpp"
