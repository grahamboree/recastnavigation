//
// Copyright (c) 2009-2010 Mikko Mononen memon@inside.org
//
// This software is provided 'as-is', without any express or implied
// warranty.  In no event will the authors be held liable for any damages
// arising from the use of this software.
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it
// freely, subject to the following restrictions:
// 1. The origin of this software must not be misrepresented; you must not
//    claim that you wrote the original software. If you use this software
//    in a product, an acknowledgment in the product documentation would be
//    appreciated but is not required.
// 2. Altered source versions must be plainly marked as such, and must not be
//    misrepresented as being the original software.
// 3. This notice may not be removed or altered from any source distribution.
//

#include "Tool_TempObstacleCreate.h"

#include "Sample_TempObstacles.h"

#include <imgui.h>

SampleToolType TempObstacleCreateTool::type()
{
	return SampleToolType::TEMP_OBSTACLE_CREATE;
}

void TempObstacleCreateTool::init(Sample* sample)
{
	this->sample = static_cast<Sample_TempObstacles*>(sample);
}

void TempObstacleCreateTool::reset() {}

void TempObstacleCreateTool::drawMenuUI()
{
	ImGui::Text("Create Temp Obstacles");

	if (ImGui::Button("Remove All"))
	{
		sample->clearAllTempObstacles();
	}

	ImGui::Separator();

	ImGui::Text("Click LMB to create an obstacle.");
	ImGui::Text("Shift+LMB to remove an obstacle.");
}

void TempObstacleCreateTool::onClick(const float* s, const float* p, bool shift)
{
	if (sample)
	{
		if (shift)
		{
			sample->removeTempObstacle(s, p);
		}
		else
		{
			sample->addTempObstacle(p);
		}
	}
}

void TempObstacleCreateTool::onToggle() {}
void TempObstacleCreateTool::singleStep() {}
void TempObstacleCreateTool::update(const float /*dt*/) {}
void TempObstacleCreateTool::render() {}
void TempObstacleCreateTool::drawOverlayUI() {}