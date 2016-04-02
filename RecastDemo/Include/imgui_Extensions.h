//
//  imgui_Extensions.h
//  RecastDemo
//
//  Created by Graham Pentheny on 3/30/16.
//
//

#ifndef imgui_Extensions_h
#define imgui_Extensions_h

#include "imgui.h"

namespace ImGui {
	void ScreenspaceText(const char* text, ImVec2 position);
	void ScreenspaceText(const char* text, ImVec4 color, ImVec2 position);
}

#endif /* imgui_Extensions_h */