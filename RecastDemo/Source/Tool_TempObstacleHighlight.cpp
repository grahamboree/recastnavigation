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

#include "Tool_TempObstacleHighlight.h"

#include "Sample_TempObstacles.h"

#include <SDL_opengl.h>
#include <imgui.h>

SampleToolType TempObstacleHighlightTool::type()
{
	return SampleToolType::TEMP_OBSTACLE_HIGHLIGHT;
}

void TempObstacleHighlightTool::init(Sample* sample)
{
	this->sample = static_cast<Sample_TempObstacles*>(sample);
}

void TempObstacleHighlightTool::reset() {}

void TempObstacleHighlightTool::drawMenuUI()
{
	ImGui::Text("Highlight Tile Cache");
	ImGui::Text("LMB: highlight a tile.");
	ImGui::Separator();
	if (ImGui::RadioButton("Draw Areas", drawType == DRAWDETAIL_AREAS))
	{
		drawType = DRAWDETAIL_AREAS;
	}
	if (ImGui::RadioButton("Draw Regions", drawType == DRAWDETAIL_REGIONS))
	{
		drawType = DRAWDETAIL_REGIONS;
	}
	if (ImGui::RadioButton("Draw Contours", drawType == DRAWDETAIL_CONTOURS))
	{
		drawType = DRAWDETAIL_CONTOURS;
	}
	if (ImGui::RadioButton("Draw Mesh", drawType == DRAWDETAIL_MESH))
	{
		drawType = DRAWDETAIL_MESH;
	}
}

void TempObstacleHighlightTool::onClick(const float* /*s*/, const float* p, bool /*shift*/)
{
	hitPosSet = true;
	rcVcopy(hitPos, p);
}

void TempObstacleHighlightTool::onToggle() {}

void TempObstacleHighlightTool::singleStep() {}

void TempObstacleHighlightTool::update(const float /*dt*/) {}

void TempObstacleHighlightTool::render()
{
	if (hitPosSet && sample)
	{
		const float s = sample->agentRadius;
		glColor4ub(0, 0, 0, 128);
		glLineWidth(2.0f);
		glBegin(GL_LINES);
		glVertex3f(hitPos[0] - s, hitPos[1] + 0.1f, hitPos[2]);
		glVertex3f(hitPos[0] + s, hitPos[1] + 0.1f, hitPos[2]);
		glVertex3f(hitPos[0], hitPos[1] - s + 0.1f, hitPos[2]);
		glVertex3f(hitPos[0], hitPos[1] + s + 0.1f, hitPos[2]);
		glVertex3f(hitPos[0], hitPos[1] + 0.1f, hitPos[2] - s);
		glVertex3f(hitPos[0], hitPos[1] + 0.1f, hitPos[2] + s);
		glEnd();
		glLineWidth(1.0f);

		int tileX = 0, tileY = 0;
		sample->getTilePos(hitPos, tileX, tileY);
		sample->renderCachedTile(tileX, tileY, drawType);
	}
}

void TempObstacleHighlightTool::drawOverlayUI()
{
	if (hitPosSet && sample)
	{
		int tileX = 0, tileY = 0;
		sample->getTilePos(hitPos, tileX, tileY);
		sample->renderCachedTileOverlay(tileX, tileY);
	}
}