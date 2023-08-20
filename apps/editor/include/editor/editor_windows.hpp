#pragma once

struct SDL_Renderer;
struct ImGuiIO;

namespace obsidian::editor {

struct DataContext;

void editor(SDL_Renderer& renderer, ImGuiIO& imguiIO, DataContext& context);

} /*namespace obsidian::editor*/
