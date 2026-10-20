#pragma once
#include "imgui.h"

namespace esp {
// Called once per frame between ImGui::NewFrame() and ImGui::Render().
void draw(const ImVec2& display);
}
