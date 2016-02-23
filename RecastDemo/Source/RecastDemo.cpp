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

#include "RecastDemo.h"

#include <iomanip>
#include <iostream>
#include <sstream>

#include "DetourAssert.h"
#include "Filelist.h"
#include "InputGeom.h"
#include "Recast.h"
#include "Sample_Debug.h"
#include "Sample_SoloMesh.h"
#include "Sample_TempObstacles.h"
#include "Sample_TileMesh.h"

#include "imgui.h"
#include "imguiRenderGL.h"

using std::string;

static const string meshesFolder = "Meshes";
static const string testCasesFolder = "TestCases";

RecastDemo::RecastDemo()
	: width(800)
	, height(600)
	, window(nullptr)
	, t(0.0f)
	, timeAcc(0.0f)
	, prevFrameTime(0)
	, mousePos{ 0, 0 }
	, origMousePos{ 0, 0 }
	, cameraEulers{ 45, -45 }
	, cameraPos{ 0, 0, 0 }
	, camr(1000)
	, origCameraEulers{ 0, 0 }
	, moveW(0)
	, moveS(0)
	, moveA(0)
	, moveD(0)
	, scrollZoom(0)
	, rotate(false)
	, movedDuringRotate(false)
	, rayStart{ 0, 0, 0 }
	, rayEnd{ 0, 0, 0 }
	, mouseOverMenu(false)
	, showMenu(true)
	, showLog(false)
	, showTools(true)
	, showLevels(false)
	, showSampleMenu(false)
	, showTestCases(false)
	, propScroll(0)
	, logScroll(0)
	, toolsScroll(0)
	, sampleName("Choose Sample...")
	, meshName("Choose Mesh...")
	, markerPosition{ 0, 0, 0 }
	, markerPositionSet(false)
	, geom(nullptr)
	, sample(nullptr)
	, test(nullptr)
	, mouseScroll(0)
	, processHitTest(false)
	, processHitTestShift(false)
	, mouseButtonMask(0)
	, viewport{ 0, 0, 0, 0}
{
	samples["Solo Mesh"] = []() { return new Sample_SoloMesh(); };
	samples["Tile Mesh"] = []() { return new Sample_TileMesh(); };
	samples["Temp Obstacles"] = []() { return new Sample_TempObstacles(); };
	//samples["Debug"] = []() { return new Sample_Debug(); };
}

RecastDemo::~RecastDemo()
{
	imguiRenderGLDestroy();

	SDL_Quit();

	if (sample)
	{
		delete sample;
		sample = nullptr;
	}

	if (geom)
	{
		delete geom;
		geom = nullptr;
	}
}

bool RecastDemo::initSDL(bool presentationMode)
{
	showMenu = !presentationMode;

	if (SDL_Init(SDL_INIT_EVERYTHING) != 0)
	{
		std::cerr << "Could not initialise SDL.\nError: " << SDL_GetError() << "\n";
		return false;
	}

	// Enable depth buffer.
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

	// Set color channel depth.
	SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);

	// 4x MSAA.
	SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
	SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 4);

	// Set the window size.
	Uint32 flags = SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN;
	SDL_DisplayMode displayMode;
	SDL_GetCurrentDisplayMode(0, &displayMode);
	if (presentationMode)
	{
		// Create a fullscreen window at the native resolution.
		width = displayMode.w;
		height = displayMode.h;
		flags |= SDL_WINDOW_FULLSCREEN;
	}
	else
	{
		constexpr float aspect = 16.0f / 9.0f;
		width = rcMin(displayMode.w, static_cast<int>(displayMode.h * aspect)) - 80;
		height = displayMode.h - 80;
	}

	// Create a window.
	window = SDL_CreateWindow("Recast Demo", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, width, height, flags);
	if (!window)
	{
		std::cerr << "Could not initialise SDL opengl.\nError: " << SDL_GetError() << "\n";
		return false;
	}
	SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);

	// Create an opengl context.
	SDL_GL_CreateContext(window);

	// Enable vsync.
	SDL_GL_SetSwapInterval(1);

	// Init the gui renderer.
	if (!imguiRenderGLInit("DroidSans.ttf"))
	{
		printf("Could not init GUI renderer.\n");
		SDL_Quit();
		return false;
	}

	// Enable fog.
	float fogColor[] = { 0.32f, 0.31f, 0.30f, 1.0f };
	glEnable(GL_FOG);
	glFogi(GL_FOG_MODE, GL_LINEAR);
	glFogf(GL_FOG_START, camr * 0.1f);
	glFogf(GL_FOG_END, camr * 1.25f);
	glFogfv(GL_FOG_COLOR, fogColor);

	// Enable backface culling and set the depth buffer culling function.
	glEnable(GL_CULL_FACE);
	glDepthFunc(GL_LEQUAL);

	// Set the viewport.
	glGetIntegerv(GL_VIEWPORT, viewport);

	// Set the clear color.
	glClearColor(0.3f, 0.3f, 0.32f, 1.0f);

	prevFrameTime = SDL_GetTicks();

	return true;
}

bool RecastDemo::ReadUserInput()
{
	// Process all SDL events.
	SDL_Event event;
	while (SDL_PollEvent(&event))
	{
		switch (event.type)
		{
		case SDL_KEYDOWN:
			// Handle any key presses here.
			if (event.key.keysym.sym == SDLK_ESCAPE)
			{
				return true;
			}
			else if (event.key.keysym.sym == SDLK_t)
			{
				showLevels = false;
				showSampleMenu = false;
				showTestCases = true;
				scanDirectory(testCasesFolder, ".txt", files);
			}
			else if (event.key.keysym.sym == SDLK_TAB)
			{
				showMenu = !showMenu;
			}
			else if (event.key.keysym.sym == SDLK_SPACE)
			{
				if (sample)
					sample->handleToggle();
			}
			else if (event.key.keysym.sym == SDLK_1)
			{
				if (sample)
					sample->handleStep();
			}
			else if (event.key.keysym.sym == SDLK_9)
			{
				if (sample && geom)
				{
					string savePath = meshesFolder + "/";
					BuildSettings settings;
					memset(&settings, 0, sizeof(settings));

					rcVcopy(settings.navMeshBMin, geom->getNavMeshBoundsMin());
					rcVcopy(settings.navMeshBMax, geom->getNavMeshBoundsMax());

					sample->collectSettings(settings);

					geom->saveGeomSet(&settings);
				}
			}
			break;
		case SDL_MOUSEWHEEL:
			if (event.wheel.y < 0)
			{
				// wheel down
				if (mouseOverMenu)
				{
					mouseScroll++;
				}
				else
				{
					scrollZoom += 1.0f;
				}
			}
			else
			{
				if (mouseOverMenu)
				{
					mouseScroll--;
				}
				else
				{
					scrollZoom -= 1.0f;
				}
			}
			break;
		case SDL_MOUSEBUTTONDOWN:
			if (event.button.button == SDL_BUTTON_RIGHT)
			{
				if (!mouseOverMenu)
				{
					// Rotate view
					rotate = true;
					movedDuringRotate = false;
					origMousePos[0] = mousePos[0];
					origMousePos[1] = mousePos[1];
					origCameraEulers[0] = cameraEulers[0];
					origCameraEulers[1] = cameraEulers[1];
				}
			}
			break;
		case SDL_MOUSEBUTTONUP:
			// Handle mouse clicks here.
			if (event.button.button == SDL_BUTTON_RIGHT)
			{
				rotate = false;
				if (!mouseOverMenu)
				{
					if (!movedDuringRotate)
					{
						processHitTest = true;
						processHitTestShift = true;
					}
				}
			}
			else if (event.button.button == SDL_BUTTON_LEFT)
			{
				if (!mouseOverMenu)
				{
					processHitTest = true;
					processHitTestShift = (SDL_GetModState() & KMOD_SHIFT) ? true : false;
				}
			}
			break;
		case SDL_MOUSEMOTION:
			mousePos[0] = event.motion.x;
			mousePos[1] = height - 1 - event.motion.y;

			if (rotate)
			{
				int dx = mousePos[0] - origMousePos[0];
				int dy = mousePos[1] - origMousePos[1];
				cameraEulers[0] = origCameraEulers[0] - dy * 0.25f;
				cameraEulers[1] = origCameraEulers[1] + dx * 0.25f;
				if (dx * dx + dy * dy > 3 * 3)
				{
					movedDuringRotate = true;
				}
			}
			break;
		case SDL_QUIT:
			return true;
		default:
			break;
		}
	}

	// Update the mouse button mask.
	if (SDL_GetMouseState(nullptr, nullptr) & SDL_BUTTON_LMASK)
	{
		mouseButtonMask |= IMGUI_MBUT_LEFT;
	}
	if (SDL_GetMouseState(nullptr, nullptr) & SDL_BUTTON_RMASK)
	{
		mouseButtonMask |= IMGUI_MBUT_RIGHT;
	}

	return false;
}

void RecastDemo::HitTestMesh()
{
	// Hit test mesh.
	float hitTime;
	bool hit = geom->raycastMesh(rayStart, rayEnd, hitTime);

	if (hit)
	{
		if (SDL_GetModState() & KMOD_CTRL)
		{
			// Marker
			markerPositionSet = true;
			markerPosition[0] = rayStart[0] + (rayEnd[0] - rayStart[0]) * hitTime;
			markerPosition[1] = rayStart[1] + (rayEnd[1] - rayStart[1]) * hitTime;
			markerPosition[2] = rayStart[2] + (rayEnd[2] - rayStart[2]) * hitTime;
		}
		else
		{
			float pos[] = {
				rayStart[0] + (rayEnd[0] - rayStart[0]) * hitTime,
				rayStart[1] + (rayEnd[1] - rayStart[1]) * hitTime,
				rayStart[2] + (rayEnd[2] - rayStart[2]) * hitTime
			};
			sample->handleClick(rayStart, pos, processHitTestShift);
		}
	}
	else
	{
		if (SDL_GetModState() & KMOD_CTRL)
		{
			// Marker
			markerPositionSet = false;
		}
	}
}

void RecastDemo::HandleKeyboardMovement(float dt, GLdouble* modelviewMatrix)
{
	const Uint8* keystate = SDL_GetKeyboardState(nullptr);
	moveW = rcClamp(moveW + dt * 4 * (keystate[SDL_SCANCODE_W] ? 1 : -1), 0.0f, 1.0f);
	moveA = rcClamp(moveA + dt * 4 * (keystate[SDL_SCANCODE_A] ? 1 : -1), 0.0f, 1.0f);
	moveS = rcClamp(moveS + dt * 4 * (keystate[SDL_SCANCODE_S] ? 1 : -1), 0.0f, 1.0f);
	moveD = rcClamp(moveD + dt * 4 * (keystate[SDL_SCANCODE_D] ? 1 : -1), 0.0f, 1.0f);

	float keybSpeed = 22.0f;
	if (SDL_GetModState() & KMOD_SHIFT)
	{
		keybSpeed *= 4.0f;
	}

	float movex = (moveD - moveA) * keybSpeed * dt;
	float movey = (moveS - moveW) * keybSpeed * dt + scrollZoom * 2.0f;
	scrollZoom = 0;

	cameraPos[0] += movex * static_cast<float>(modelviewMatrix[0]);
	cameraPos[1] += movex * static_cast<float>(modelviewMatrix[4]);
	cameraPos[2] += movex * static_cast<float>(modelviewMatrix[8]);

	cameraPos[0] += movey * static_cast<float>(modelviewMatrix[2]);
	cameraPos[1] += movey * static_cast<float>(modelviewMatrix[6]);
	cameraPos[2] += movey * static_cast<float>(modelviewMatrix[10]);
}

void RecastDemo::RenderPropertiesMenu()
{
	mouseOverMenu |= imguiBeginScrollArea("Properties", width - 250 - 10, 10, 250, height - 20, &propScroll);

	if (imguiCheck("Show Log", showLog))
	{
		showLog = !showLog;
	}
	if (imguiCheck("Show Tools", showTools))
	{
		showTools = !showTools;
	}

	imguiSeparator();
	imguiLabel("Sample");
	if (imguiButton(sampleName.c_str()))
	{
		showSampleMenu = !showSampleMenu;
		if (showSampleMenu)
		{
			showLevels = false;
			showTestCases = false;
		}
	}

	imguiSeparator();
	imguiLabel("Input Mesh");
	if (imguiButton(meshName.c_str()))
	{
		if (showLevels)
		{
			showLevels = false;
		}
		else
		{
			showSampleMenu = false;
			showTestCases = false;
			showLevels = true;
			scanDirectory(meshesFolder, ".obj", files);
			scanDirectoryAppend(meshesFolder, ".gset", files);
		}
	}
	if (geom)
	{
		std::ostringstream text;
		text << "Verts: " << std::fixed << std::setprecision(1) << geom->getMesh()->getVertCount() / 1000.0f << "k";
		text << " Tris: " << std::fixed << std::setprecision(1) << geom->getMesh()->getTriCount() / 1000.0f << "k";
		imguiValue(text.str().c_str());
	}
	imguiSeparator();

	if (geom && sample)
	{
		imguiSeparatorLine();

		sample->handleSettings();

		if (imguiButton("Build"))
		{
			ctx.resetLog();
			if (!sample->handleBuild())
			{
				showLog = true;
				logScroll = 0;
			}
			ctx.dumpLog("Build log %s:", meshName.c_str());

			// Clear test.
			delete test;
			test = nullptr;
		}

		imguiSeparator();
	}

	if (sample)
	{
		imguiSeparatorLine();
		sample->handleDebugMode();
	}

	imguiEndScrollArea();
}

void RecastDemo::RenderSampleSelectionDialog()
{
	static int levelScroll = 0;
	mouseOverMenu |= imguiBeginScrollArea("Choose Sample", width - 10 - 250 - 10 - 200, height - 10 - 250, 200, 250, &levelScroll);

	Sample* newSample = nullptr;
	for (auto sampleItem : samples)
	{
		if (imguiItem(sampleItem.first.c_str()))
		{
			newSample = sampleItem.second();
			if (newSample)
			{
				sampleName = sampleItem.first;
			}
		}
	}
	if (newSample)
	{
		delete sample;
		sample = newSample;
		sample->setContext(&ctx);
		if (geom)
		{
			sample->handleMeshChanged(geom);
		}
		showSampleMenu = false;
	}

	ResetCameraAndFogToMeshBounds();

	imguiEndScrollArea();
}

string* RecastDemo::RenderSelectFileUI()
{
	string* selectedFile = nullptr;
	for (auto& filename : files)
	{
		if (imguiItem(filename.c_str()))
		{
			selectedFile = &filename;
		}
	}
	return selectedFile;
}

void RecastDemo::RenderLevelSelectionDialog()
{
	static int levelScroll = 0;
	mouseOverMenu |= imguiBeginScrollArea("Choose Level", width - 10 - 250 - 10 - 200, height - 10 - 450, 200, 450, &levelScroll);

	string* levelToLoad = RenderSelectFileUI();
	if (levelToLoad)
	{
		meshName = *levelToLoad;
		showLevels = false;

		delete geom;
		geom = nullptr;

		geom = new InputGeom();
		string path = meshesFolder + "/" + meshName;
		if (!geom->load(&ctx, path))
		{
			delete geom;
			geom = nullptr;

			// Destroy the sample if it already had geometry loaded, as we've just deleted it!
			if (sample && sample->getInputGeom())
			{
				delete sample;
				sample = nullptr;
			}

			showLog = true;
			logScroll = 0;
			ctx.dumpLog("Geom load log %s:", meshName.c_str());
		}
		if (sample && geom)
		{
			sample->handleMeshChanged(geom);
		}

		ResetCameraAndFogToMeshBounds();
	}

	imguiEndScrollArea();
}

void RecastDemo::RenderTestCaseSelectionDialog()
{
	static int testScroll = 0;
	mouseOverMenu |= imguiBeginScrollArea("Choose Test To Run", width - 10 - 250 - 10 - 200, height - 10 - 450, 200, 450, &testScroll);

	string* testToLoad = RenderSelectFileUI();
	if (testToLoad)
	{
		string path = testCasesFolder + "/" + *testToLoad;
		test = new TestCase();
		if (test)
		{
			// Load the test.
			if (!test->load(path))
			{
				delete test;
				test = nullptr;
			}

			// Create sample
			Sample* newSample = nullptr;
			for (auto sampleItem : samples)
			{
				if (sampleItem.first == test->getSampleName())
				{
					newSample = sampleItem.second();
					if (newSample)
					{
						sampleName = sampleItem.first;
					}
				}
			}

			delete sample;
			sample = newSample;

			if (sample)
			{
				sample->setContext(&ctx);
				showSampleMenu = false;
			}

			// Load geom.
			meshName = test->getGeomFileName();

			path = meshesFolder + "/" + meshName;

			delete geom;
			geom = new InputGeom();
			if (!geom || !geom->load(&ctx, path))
			{
				delete geom;
				geom = nullptr;
				delete sample;
				sample = nullptr;
				showLog = true;
				logScroll = 0;
				ctx.dumpLog("Geom load log %s:", meshName.c_str());
			}
			if (sample && geom)
			{
				sample->handleMeshChanged(geom);
			}

			// This will ensure that tile & poly bits are updated in tiled sample.
			if (sample)
				sample->handleSettings();

			ctx.resetLog();
			if (sample && !sample->handleBuild())
			{
				ctx.dumpLog("Build log %s:", meshName.c_str());
			}

			ResetCameraAndFogToMeshBounds();

			// Do the tests.
			if (sample)
			{
				test->doTests(sample->getNavMesh(), sample->getNavMeshQuery());
			}
		}
	}

	imguiEndScrollArea();
}

void RecastDemo::ResetCameraAndFogToMeshBounds()
{
	if (geom || sample)
	{
		const float* bmin = nullptr;
		const float* bmax = nullptr;
		if (geom)
		{
			bmin = geom->getNavMeshBoundsMin();
			bmax = geom->getNavMeshBoundsMax();
		}
		if (bmin && bmax)
		{
			camr = sqrtf(
					rcSqr(bmax[0] - bmin[0]) +
					rcSqr(bmax[1] - bmin[1]) +
					rcSqr(bmax[2] - bmin[2])) / 2;
			cameraPos[0] = (bmax[0] + bmin[0]) / 2 + camr;
			cameraPos[1] = (bmax[1] + bmin[1]) / 2 + camr;
			cameraPos[2] = (bmax[2] + bmin[2]) / 2 + camr;
			camr *= 3;
		}
		cameraEulers[0] = 45;
		cameraEulers[1] = -45;
		glFogf(GL_FOG_START, camr * 0.2f);
		glFogf(GL_FOG_END, camr * 1.25f);
	}
}

bool RecastDemo::run()
{
	// Reset per-frame state.
	mouseScroll = 0;
	mouseButtonMask = 0;
	processHitTest = false;
	processHitTestShift = false;

	// Handle input events and quit if escape was pressed.
	if (ReadUserInput()) {
		return true;
	}

	uint32_t time = SDL_GetTicks();
	float dt = (time - prevFrameTime) / 1000.0f;
	prevFrameTime = time;
	t += dt;

	if (processHitTest && geom && sample)
	{
		HitTestMesh();
	}

	// Update sample simulation.
	constexpr float SIM_RATE = 20;
	constexpr float DELTA_TIME = 1.0f / SIM_RATE;
	timeAcc = rcClamp(timeAcc + dt, -1.0f, 1.0f);
	for (int simIter = 0; timeAcc > DELTA_TIME; ++simIter)
	{
		timeAcc -= DELTA_TIME;
		if (simIter < 5 && sample)
		{
			sample->handleUpdate(DELTA_TIME);
		}
	}

	// Clamp the framerate so that we do not hog all the CPU.
	constexpr float MIN_FRAME_TIME = 1.0f / 60.0f;
	if (dt < MIN_FRAME_TIME)
	{
		int ms = static_cast<int>((MIN_FRAME_TIME - dt) * 1000.0f);
		if (ms > 10) ms = 10;
		if (ms >= 0) SDL_Delay(ms);
	}

	// Clear the screen
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDisable(GL_TEXTURE_2D);
	glEnable(GL_DEPTH_TEST);

	// Compute the projection matrix.
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	gluPerspective(50.0f, static_cast<float>(width) / static_cast<float>(height), 1.0f, camr);
	GLdouble projectionMatrix[16];
	glGetDoublev(GL_PROJECTION_MATRIX, projectionMatrix);

	// Compute the modelview matrix.
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	glRotatef(cameraEulers[0], 1, 0, 0);
	glRotatef(cameraEulers[1], 0, 1, 0);
	glTranslatef(-cameraPos[0], -cameraPos[1], -cameraPos[2]);
	GLdouble modelviewMatrix[16];
	glGetDoublev(GL_MODELVIEW_MATRIX, modelviewMatrix);

	// Get mouse direction ray.
	GLdouble x, y, z;
	GLint projectionResult = gluUnProject(mousePos[0], mousePos[1], 0.0f, modelviewMatrix, projectionMatrix, viewport, &x, &y, &z);
	dtAssert(projectionResult);
	rayStart[0] = static_cast<float>(x);
	rayStart[1] = static_cast<float>(y);
	rayStart[2] = static_cast<float>(z);

	projectionResult = gluUnProject(mousePos[0], mousePos[1], 1.0f, modelviewMatrix, projectionMatrix, viewport, &x, &y, &z);
	dtAssert(projectionResult);
	rayEnd[0] = static_cast<float>(x);
	rayEnd[1] = static_cast<float>(y);
	rayEnd[2] = static_cast<float>(z);

	HandleKeyboardMovement(dt, modelviewMatrix);

	glEnable(GL_FOG);

	if (sample)
		sample->handleRender();
	if (test)
		test->handleRender();

	glDisable(GL_FOG);

	// Render GUI
	glDisable(GL_DEPTH_TEST);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	gluOrtho2D(0, width, 0, height);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	mouseOverMenu = false;

	imguiBeginFrame(mousePos[0], mousePos[1], mouseButtonMask, mouseScroll);

	if (sample)
	{
		sample->handleRenderOverlay(
			static_cast<double*>(projectionMatrix),
			static_cast<double*>(modelviewMatrix),
			static_cast<int*>(viewport));
	}

	if (test)
	{
		mouseOverMenu |= test->handleRenderOverlay(
			static_cast<double*>(projectionMatrix),
			static_cast<double*>(modelviewMatrix),
			static_cast<int*>(viewport));
	}

	// Help text.
	if (showMenu)
	{
		imguiDrawText(280, height - 20, IMGUI_ALIGN_LEFT, "W/S/A/D: Move  RMB: Rotate", imguiRGBA(255, 255, 255, 128));
	}

	if (showMenu)
	{
		RenderPropertiesMenu();
	}

	// Sample selection dialog.
	if (showSampleMenu)
	{
		RenderSampleSelectionDialog();
	}

	// Level selection dialog.
	if (showLevels)
	{
		RenderLevelSelectionDialog();
	}

	// Test cases
	if (showTestCases)
	{
		RenderTestCaseSelectionDialog();
	}

	// Log
	if (showLog && showMenu)
	{
		mouseOverMenu |= imguiBeginScrollArea("Log", 250 + 20, 10, width - 300 - 250, 200, &logScroll);
		for (int i = 0; i < ctx.getLogCount(); ++i)
		{
			imguiLabel(ctx.getLogText(i));
		}
		imguiEndScrollArea();
	}

	// Left column tools menu
	if (!showTestCases && showTools && showMenu)
	{
		mouseOverMenu |= imguiBeginScrollArea("Tools", 10, 10, 250, height - 20, &toolsScroll);

		if (sample)
		{
			sample->handleTools();
		}

		imguiEndScrollArea();
	}

	// Marker
	if (markerPositionSet && gluProject(markerPosition[0], markerPosition[1], markerPosition[2], modelviewMatrix, projectionMatrix, viewport, &x, &y, &z))
	{
		// Draw marker circle
		glLineWidth(5.0f);
		glColor4ub(240, 220, 0, 196);
		glBegin(GL_LINE_LOOP);
		const float r = 25.0f;
		for (int i = 0; i < 20; ++i)
		{
			const float a = static_cast<float>(i) / 20.0f * RC_PI * 2;
			const float fx = static_cast<float>(x) + cosf(a)*r;
			const float fy = static_cast<float>(y) + sinf(a)*r;
			glVertex2f(fx, fy);
		}
		glEnd();
		glLineWidth(1.0f);
	}

	imguiEndFrame();
	imguiRenderGLDraw();

	glEnable(GL_DEPTH_TEST);
	SDL_GL_SwapWindow(window);
	return false;
}
