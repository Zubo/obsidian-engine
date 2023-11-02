#pragma once

struct SDL_Renderer;
struct ImGuiIO;

namespace obsidian {

class ObsidianEngine;

}

namespace obsidian::editor {

struct DataContext;

void editor(SDL_Renderer& renderer, ImGuiIO& imguiIO, DataContext& context,
            ObsidianEngine& engine, bool& engineStarted);

void fileDropped(char const* file, ObsidianEngine& engine);

} /*namespace obsidian::editor*/
