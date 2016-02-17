//
// Copyright (c) 2016 Graham Pentheny g@grahamboree.com
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

#ifndef RECASTDEMO_H
#define RECASTDEMO_H

#include <functional>
#include <map>
#include <string>
#include <vector>

#include "SDL.h"
#include "SDL_opengl.h"
#ifdef __APPLE__
#	include <OpenGL/glu.h>
#else
#	include <GL/glu.h>
#endif

#include "SampleInterfaces.h"
#include "TestCase.h"

class InputGeom;
class Sample;

class RecastDemo
{
public:
	RecastDemo();
	~RecastDemo();

	RecastDemo(const RecastDemo&) = delete;
	RecastDemo(RecastDemo&&) = delete;
	RecastDemo& operator=(const RecastDemo&) = delete;
	RecastDemo& operator=(RecastDemo&&) = delete;

	bool initSDL(bool presentationMode = false);

	// Returns true when the we should exit.
	bool run();

private: // methods
	bool ReadUserInput();
	void HitTestMesh();
	void HandleKeyboardMovement(float dt, GLdouble* modelviewMatrix);
	void ResetCameraAndFogToMeshBounds();

	// UI
	std::string* RenderSelectFileUI();
	void RenderPropertiesMenu();
	void RenderSampleSelectionDialog();
	void RenderLevelSelectionDialog();
	void RenderTestCaseSelectionDialog();

private: // fields
	int width;
	int height;

	typedef std::function<Sample*()> sampleGenerator;
	std::map<std::string, sampleGenerator> samples;

	SDL_Window* window;

	float t;
	float timeAcc;
	uint32_t prevFrameTime;
	int mousePos[2];
	int origMousePos[2]; // Used to compute mouse movement totals across frames.

	float cameraEulers[2];
	float cameraPos[3];
	float camr;
	float origCameraEulers[2]; // Used to compute rotational changes across frames.

	float moveW;
	float moveA;
	float moveS;
	float moveD;

	float scrollZoom;
	bool rotate;
	bool movedDuringRotate;
	float rayStart[3];
	float rayEnd[3];
	bool mouseOverMenu;

	bool showMenu;
	bool showLog;
	bool showTools;
	bool showLevels;
	bool showSampleMenu;
	bool showTestCases;

	// Window scroll positions.
	int propScroll;
	int logScroll;
	int toolsScroll;

	std::string sampleName;

	std::vector<std::string> files;
	std::string meshName;

	GLdouble markerPosition[3];
	bool markerPositionSet;

	InputGeom* geom;
	Sample* sample;

	TestCase* test;

	BuildContext ctx;

	// User input.
	int mouseScroll;
	bool processHitTest;
	bool processHitTestShift;
	unsigned char mouseButtonMask;

	GLint viewport[4];
};

#endif // RECASTDEMO_H
