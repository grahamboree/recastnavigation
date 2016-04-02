//
//  imgui_Extensions.cpp
//  RecastDemo
//
//  Created by Graham Pentheny on 3/30/16.
//
//

#include "imgui_Extensions.h"


void ImGui::ScreenspaceText(const char* text, ImVec2 position)
{
	ImGui::SetNextWindowPos(position);
	if (ImGui::Begin(text, 0, position, 0,
					 ImGuiWindowFlags_NoTitleBar |
					 ImGuiWindowFlags_NoResize |
					 ImGuiWindowFlags_NoMove |
					 ImGuiWindowFlags_NoSavedSettings |
					 ImGuiWindowFlags_NoInputs |
					 ImGuiWindowFlags_NoScrollbar |
					 ImGuiWindowFlags_NoScrollWithMouse |
					 ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::Text("%s", text);
		//ImGui::TextColored(ImVec4(255, 255, 255, 128), text);
	}
	ImGui::End();
}

void ImGui::ScreenspaceText(const char* text, ImVec4 color, ImVec2 position)
{
	ImGui::SetNextWindowPos(position);
	if (ImGui::Begin(text, 0, position, 0,
					 ImGuiWindowFlags_NoTitleBar |
					 ImGuiWindowFlags_NoResize |
					 ImGuiWindowFlags_NoMove |
					 ImGuiWindowFlags_NoSavedSettings |
					 ImGuiWindowFlags_NoInputs |
					 ImGuiWindowFlags_NoScrollbar |
					 ImGuiWindowFlags_NoScrollWithMouse |
					 ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::TextColored(color, "%s", text);
	}
	ImGui::End();
}