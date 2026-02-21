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

#include "Tool_NavMeshTile.h"

#include "Sample_TileMesh.h"
#include "imguiHelpers.h"

#include <SDL_opengl.h>
#include <imgui.h>

SampleToolType NavMeshTileTool::type()
{
	return SampleToolType::TILE_EDIT;
}

void NavMeshTileTool::init(Sample* inSample)
{
	sample = static_cast<Sample_TileMesh*>(inSample);
}

void NavMeshTileTool::reset() {}

void NavMeshTileTool::drawMenuUI()
{
	ImGui::Text("LMB: Rebuild selected tile.");
	ImGui::Text("Shift+LMB: Clear tile selection.");

	ImGui::Spacing();

	ImGui::Text("Create Tiles");
	if (ImGui::Button("Create All"))
	{
		if (sample)
		{
			sample->buildAllTiles();
		}
	}
	ImGui::SameLine();
	if (ImGui::Button("Remove All"))
	{
		if (sample)
		{
			sample->removeAllTiles();
		}
	}
}

void NavMeshTileTool::onClick(const float* /*s*/, const float* p, bool shift)
{
	hitPosSet = true;
	rcVcopy(hitPos, p);
	if (sample)
	{
		if (shift)
		{
			sample->removeTile(hitPos);
		}
		else
		{
			sample->buildTile(hitPos);
		}
	}
}

void NavMeshTileTool::onToggle() {}

void NavMeshTileTool::singleStep() {}

void NavMeshTileTool::update(const float /*dt*/) {}

void NavMeshTileTool::render()
{
	if (!hitPosSet)
	{
		return;
	}

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
}

void NavMeshTileTool::drawOverlayUI()
{
	if (hitPosSet)
	{
		int tx = 0;
		int ty = 0;
		sample->getTilePos(hitPos, tx, ty);
		char text[32];
		snprintf(text, 32, "(%d,%d)", tx, ty);
		DrawWorldspaceText(hitPos[0], hitPos[1], hitPos[2], IM_COL32(0, 0, 0, 220), text);
	}
}