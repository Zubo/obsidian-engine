#pragma once

struct SDL_Renderer;
struct ImGuiIO;

namespace obsidian {

class ObsidianEngine;

}

namespace obsidian::editor {

struct DataContext;

void begnEditorFrame(ImGuiIO& imguiIO);

void endEditorFrame(SDL_Renderer& renderer, ImGuiIO& imguiIO);

void editorWindow(SDL_Renderer& renderer, ImGuiIO& imguiIO,
                  DataContext& context, ObsidianEngine& engine);

void fileDropped(char const* file, ObsidianEngine& engine);

} /*namespace obsidian::editor*/
