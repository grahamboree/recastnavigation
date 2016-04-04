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

#include <cstdio>
#define _USE_MATH_DEFINES
#include <cmath>

#include "SDL.h"
#include "SDL_opengl.h"
#ifdef __APPLE__
#	include <OpenGL/glu.h>
#else
#	include <GL/glu.h>
#endif

#include <vector>
#include <string>

#include "imgui.h"
#include "imgui_impl_sdl.h"

#include "Recast.h"
#include "InputGeom.h"
#include "TestCase.h"
#include "Sample_SoloMesh.h"
#include "Sample_TileMesh.h"
#include "Sample_TempObstacles.h"
#include "Sample_Debug.h"
#include "noc_file_dialog.h"
#include "imgui_Extensions.h"

#ifdef WIN32
#	define snprintf _snprintf
#	define putenv _putenv
#endif

using std::string;
using std::vector;

struct SampleItem
{
	Sample* (*create)();
	const string name;
};
Sample* createSolo() { return new Sample_SoloMesh(); }
Sample* createTile() { return new Sample_TileMesh(); }
Sample* createTempObstacle() { return new Sample_TempObstacles(); }
Sample* createDebug() { return new Sample_Debug(); }
static SampleItem g_samples[] =
{
	{ createSolo, "Solo Mesh" },
	{ createTile, "Tile Mesh" },
	{ createTempObstacle, "Temp Obstacles" },
};
static const int g_nsamples = sizeof(g_samples) / sizeof(SampleItem);

void resetCameraToMeshBounds(InputGeom* geom, Sample* sample, float& camr, float* cameraPos, float* cameraEulers) {
	if (geom || sample)
	{
		// Compute mesh bounds.
		const float* bmin = 0;
		const float* bmax = 0;
		if (geom)
		{
			bmin = geom->getNavMeshBoundsMin();
			bmax = geom->getNavMeshBoundsMax();
		}
		
		// Reset camera and fog to match the mesh bounds.
		if (bmin && bmax)
		{
			camr = sqrtf(rcSqr(bmax[0] - bmin[0]) +
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

int main(int /*argc*/, char** /*argv*/)
{
	// Init SDL
	if (SDL_Init(SDL_INIT_EVERYTHING) != 0)
	{
		printf("Could not initialise SDL.\nError: %s\n", SDL_GetError());
		return -1;
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

	SDL_DisplayMode displayMode;
	SDL_GetCurrentDisplayMode(0, &displayMode);

	bool presentationMode = false;
	Uint32 flags = SDL_WINDOW_OPENGL;
	int width;
	int height;
	if (presentationMode)
	{
		// Create a fullscreen window at the native resolution.
		width = displayMode.w;
		height = displayMode.h;
		flags |= SDL_WINDOW_FULLSCREEN;
	}
	else
	{
		float aspect = 16.0f / 9.0f;
		width = rcMin(displayMode.w, static_cast<int>(displayMode.h * aspect)) - 80;
		height = displayMode.h - 80;
	}
	
	SDL_Window* window;
	SDL_Renderer* renderer;
	int errorCode = SDL_CreateWindowAndRenderer(width, height, flags, &window, &renderer);

	if (errorCode != 0 || !window || !renderer)
	{
		printf("Could not initialise SDL opengl\nError: %s\n", SDL_GetError());
		return -1;
	}

	SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
	SDL_GLContext glcontext = SDL_GL_CreateContext(window);
	
	// Setup ImGui binding
	ImGui_ImplSdl_Init(window);
	ImGuiIO& io = ImGui::GetIO();
	io.Fonts->AddFontFromFileTTF("DroidSans.ttf", 15);
	
	float t = 0.0f;
	float timeAcc = 0.0f;
	Uint32 prevFrameTime = SDL_GetTicks();
	int mousePos[] = {0, 0};
	int origMousePos[] = {0, 0}; // Used to compute mouse movement totals across frames.
	
	float cameraEulers[] = {45, -45};
	float cameraPos[] = {0, 0, 0};
	float camr = 1000;
	float origCameraEulers[] = {0, 0}; // Used to compute rotational changes across frames.
	
	float moveW = 0;
	float moveA = 0;
	float moveS = 0;
	float moveD = 0;
	
	float scrollZoom = 0;
	bool rotate = false;
	bool movedDuringRotate = false;
	float rayStart[] = {0, 0, 0};
	float rayEnd[] = {0, 0, 0};
	
	bool showMenu = !presentationMode;
	bool showLog = false;
	bool showTools = true;
	bool showSample = false;
	bool showTestCases = false;

	string sampleName = "Choose Sample...";
	Sample* sample = 0;

	const char* meshesFolder = "Meshes";
	string meshName = "Choose Mesh...";
	InputGeom* geom = 0;
	
	TestCase* test = 0;

	BuildContext ctx;

	float markerPosition[3] = {0, 0, 0};
	bool markerPositionSet = false;
	
	// Fog.
	float fogColor[] = {0.32f, 0.31f, 0.30f, 1.0f};
	glEnable(GL_FOG);
	glFogi(GL_FOG_MODE, GL_LINEAR);
	glFogf(GL_FOG_START, camr * 0.1f);
	glFogf(GL_FOG_END, camr * 1.25f);
	glFogfv(GL_FOG_COLOR, fogColor);
	
	glEnable(GL_CULL_FACE);
	glDepthFunc(GL_LEQUAL);
	
	bool done = false;
	while (!done)
	{
		// Handle input events.
		bool processHitTest = false;
		bool processHitTestShift = false;
		SDL_Event event;
		
		while (SDL_PollEvent(&event))
		{
			ImGui_ImplSdl_ProcessEvent(&event);
			switch (event.type)
			{
				case SDL_KEYDOWN:
				{
					// Handle any key presses here.
					if (event.key.keysym.sym == SDLK_ESCAPE)
					{
						done = true;
					}
					else if (event.key.keysym.sym == SDLK_t)
					{
						showSample = false;
						showTestCases = true;
					}
					else if (event.key.keysym.sym == SDLK_TAB)
					{
						showMenu = !showMenu;
					}
					else if (event.key.keysym.sym == SDLK_SPACE)
					{
						if (sample)
						{
							sample->handleToggle();
						}
					}
					else if (event.key.keysym.sym == SDLK_1)
					{
						if (sample)
						{
							sample->handleStep();
						}
					}
					else if (event.key.keysym.sym == SDLK_9)
					{
						if (sample && geom)
						{
							string savePath = string(meshesFolder) + "/";
							BuildSettings settings;
							memset(&settings, 0, sizeof(settings));

							rcVcopy(settings.navMeshBMin, geom->getNavMeshBoundsMin());
							rcVcopy(settings.navMeshBMax, geom->getNavMeshBoundsMax());

							sample->collectSettings(settings);

							geom->saveGeomSet(&settings);
						}
					}
				} break;
				case SDL_MOUSEWHEEL:
				{
					if (!ImGui::IsMouseHoveringAnyWindow())
					{
						scrollZoom += (event.wheel.y < 0) ? 1.0f : -1.0f;
					}
				} break;
				case SDL_MOUSEBUTTONDOWN:
				{
					if (event.button.button == SDL_BUTTON_RIGHT && !ImGui::IsMouseHoveringAnyWindow())
					{
						// Rotate view
						rotate = true;
						movedDuringRotate = false;
						origMousePos[0] = mousePos[0];
						origMousePos[1] = mousePos[1];
						origCameraEulers[0] = cameraEulers[0];
						origCameraEulers[1] = cameraEulers[1];
					}
				} break;
				case SDL_MOUSEBUTTONUP:
				{
					if (event.button.button == SDL_BUTTON_RIGHT)
					{
						rotate = false;
						if (!ImGui::IsMouseHoveringAnyWindow() && !movedDuringRotate)
						{
							processHitTest = true;
							processHitTestShift = true;
						}
					}
					else if (event.button.button == SDL_BUTTON_LEFT)
					{
						if (!ImGui::IsMouseHoveringAnyWindow())
						{
							processHitTest = true;
							processHitTestShift = (SDL_GetModState() & KMOD_SHIFT) ? true : false;
						}
					}
				} break;
				case SDL_MOUSEMOTION:
				{
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
				} break;
				case SDL_QUIT:
				{
					done = true;
				} break;
			}
		}
		
		ImGui_ImplSdl_NewFrame(window);
		
		Uint32 time = SDL_GetTicks();
		float deltaMilliseconds = (time - prevFrameTime) / 1000.0f;
		prevFrameTime = time;
		
		t += deltaMilliseconds;

		// Hit test mesh.
		if (processHitTest && geom && sample)
		{
			float hitTime;
			if (geom->raycastMesh(rayStart, rayEnd, hitTime))
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
		
		// Update sample simulation.
		const float SIM_RATE = 20;
		const float DELTA_TIME = 1.0f / SIM_RATE;
		timeAcc = rcClamp(timeAcc + deltaMilliseconds, -1.0f, 1.0f);
		int simIter = 0;
		while (timeAcc > DELTA_TIME)
		{
			timeAcc -= DELTA_TIME;
			if (simIter < 5 && sample)
			{
				sample->handleUpdate(DELTA_TIME);
			}
			simIter++;
		}

		// Clamp the framerate so that we do not hog all the CPU.
		const float MIN_FRAME_TIME = 1.0f / 40.0f;
		if (deltaMilliseconds < MIN_FRAME_TIME)
		{
			int ms = static_cast<int>((MIN_FRAME_TIME - deltaMilliseconds) * 1000.0f);
			if (ms > 10) ms = 10;
			if (ms >= 0) SDL_Delay(ms);
		}
		
		// Set the viewport.
		GLint viewport[4];
		glViewport(0, 0, width, height);
		glGetIntegerv(GL_VIEWPORT, viewport);
		
		// Clear the screen
		glClearColor(0.3f, 0.3f, 0.32f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glDisable(GL_TEXTURE_2D);
		glEnable(GL_DEPTH_TEST);
		
		// Compute the projection matrix.
		GLdouble projectionMatrix[16];
		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();
		gluPerspective(50.0f, static_cast<float>(width) / static_cast<float>(height), 1.0f, camr);
		glGetDoublev(GL_PROJECTION_MATRIX, projectionMatrix);
		
		// Compute the modelview matrix.
		GLdouble modelviewMatrix[16];
		glMatrixMode(GL_MODELVIEW);
		glLoadIdentity();
		glRotatef(cameraEulers[0], 1, 0, 0);
		glRotatef(cameraEulers[1], 0, 1, 0);
		glTranslatef(-cameraPos[0], -cameraPos[1], -cameraPos[2]);
		glGetDoublev(GL_MODELVIEW_MATRIX, modelviewMatrix);
		
		// Get hit ray position and direction.
		GLdouble x, y, z;
		gluUnProject(mousePos[0], mousePos[1], 0.0f, modelviewMatrix, projectionMatrix, viewport, &x, &y, &z);
		rayStart[0] = static_cast<float>(x);
		rayStart[1] = static_cast<float>(y);
		rayStart[2] = static_cast<float>(z);
		gluUnProject(mousePos[0], mousePos[1], 1.0f, modelviewMatrix, projectionMatrix, viewport, &x, &y, &z);
		rayEnd[0] = static_cast<float>(x);
		rayEnd[1] = static_cast<float>(y);
		rayEnd[2] = static_cast<float>(z);
		
		// Handle keyboard movement.
		const Uint8* keystate = SDL_GetKeyboardState(NULL);
		moveW = rcClamp(moveW + deltaMilliseconds * 4 * (keystate[SDL_SCANCODE_W] ? 1 : -1), 0.0f, 1.0f);
		moveA = rcClamp(moveA + deltaMilliseconds * 4 * (keystate[SDL_SCANCODE_A] ? 1 : -1), 0.0f, 1.0f);
		moveS = rcClamp(moveS + deltaMilliseconds * 4 * (keystate[SDL_SCANCODE_S] ? 1 : -1), 0.0f, 1.0f);
		moveD = rcClamp(moveD + deltaMilliseconds * 4 * (keystate[SDL_SCANCODE_D] ? 1 : -1), 0.0f, 1.0f);
		
		float keybSpeed = 22.0f;
		if (SDL_GetModState() & KMOD_SHIFT)
		{
			keybSpeed *= 4.0f;
		}
		
		float movex = (moveD - moveA) * keybSpeed * deltaMilliseconds;
		float movey = (moveS - moveW) * keybSpeed * deltaMilliseconds + scrollZoom * 2.0f;
		scrollZoom = 0;
		
		cameraPos[0] += movex * static_cast<float>(modelviewMatrix[0]);
		cameraPos[1] += movex * static_cast<float>(modelviewMatrix[4]);
		cameraPos[2] += movex * static_cast<float>(modelviewMatrix[8]);
		
		cameraPos[0] += movey * static_cast<float>(modelviewMatrix[2]);
		cameraPos[1] += movey * static_cast<float>(modelviewMatrix[6]);
		cameraPos[2] += movey * static_cast<float>(modelviewMatrix[10]);

		glEnable(GL_FOG);

		if (sample)
		{
			sample->handleRender();
		}
		if (test)
		{
			test->handleRender();
		}
		
		glDisable(GL_FOG);
		
		// Render GUI
		glDisable(GL_DEPTH_TEST);
		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();
		gluOrtho2D(0, width, 0, height);
		glMatrixMode(GL_MODELVIEW);
		glLoadIdentity();
		
		if (sample)
		{
			sample->handleRenderOverlay(static_cast<double*>(projectionMatrix), static_cast<double*>(modelviewMatrix), static_cast<int*>(viewport));
		}
		if (test)
		{
			test->handleRenderOverlay(static_cast<double*>(projectionMatrix), static_cast<double*>(modelviewMatrix), static_cast<int*>(viewport));
		}

		if (showMenu)
		{
			// Help text.
			ImGui::ScreenspaceText("W/S/A/D: Move  RMB: Rotate", ImVec4(255, 255, 255, 128), ImVec2(280, 0));
			
			ImGui::ShowTestWindow();
		
			ImGui::SetNextWindowPos(ImVec2(width - 250 - 10, 10), ImGuiSetCond_Once);
			ImGui::Begin("Properties", 0, ImVec2(250, height - 20), -1, ImGuiWindowFlags_NoSavedSettings);
			ImGui::PushItemWidth(-130);
			
			ImGui::Checkbox("Show Log", &showLog);
			ImGui::Checkbox("Show Tools", &showTools);
			
			ImGui::Spacing();
			ImGui::Text("Sample");
			if (ImGui::Button(sampleName.c_str()))
			{
				showSample = !showSample;
				if (showSample)
				{
					showTestCases = false;
				}
			}
			
			ImGui::Spacing();
			
			// Level selection dialog.
			ImGui::Text("Input Mesh");
			if (ImGui::Button(meshName.c_str()))
			{
				showSample = false;
				showTestCases = false;
				
				const char* filename = noc_file_dialog_open(NOC_FILE_DIALOG_OPEN, "obj\0*.obj", meshesFolder, 0);
				if (filename)
				{
					string path = filename;
					meshName = path.substr(path.find_last_of("\\/") + 1);
					
					delete geom;
					geom = new InputGeom;
					if (!geom->load(&ctx, path))
					{
						delete geom;
						geom = 0;
						
						// Destroy the sample if it already had geometry loaded, as we've just deleted it!
						if (sample && sample->getInputGeom())
						{
							delete sample;
							sample = 0;
						}
						
						showLog = true;
						//logScroll = 0;
						ctx.dumpLog("Geom load log %s:", path.c_str());
					}
					if (sample && geom)
					{
						sample->handleMeshChanged(geom);
					}
					
					resetCameraToMeshBounds(geom, sample, camr, cameraPos, cameraEulers);
				}
			}
			
			if (geom)
			{
				ImGui::Text("Verts: %.1fk  Tris: %.1fk",
					geom->getMesh()->getVertCount() / 1000.0f,
					geom->getMesh()->getTriCount() / 1000.0f);
			}
			
			ImGui::Spacing();

			if (geom && sample)
			{
				ImGui::Separator();
				
				sample->handleSettings();

				if (ImGui::Button("Build"))
				{
					ctx.resetLog();
					if (!sample->handleBuild())
					{
						showLog = true;
						//logScroll = 0;
					}
					ctx.dumpLog("Build log %s:", meshName.c_str());
					
					// Clear test.
					delete test;
					test = 0;
				}

				ImGui::Spacing();
			}
			
			if (sample)
			{
				ImGui::Separator();
				sample->handleDebugMode();
			}
			
			ImGui::End();
		}

		// Sample selection dialog.
		if (showSample)
		{
			ImGui::SetNextWindowPos(ImVec2(width - 10 - 250 - 10 - 200, 10));
			ImGui::SetNextWindowSize(ImVec2(200, 250));
			ImGui::Begin("Choose Sample", 0,
						 ImGuiWindowFlags_NoTitleBar |
						 ImGuiWindowFlags_NoResize |
						 ImGuiWindowFlags_NoMove |
						 ImGuiWindowFlags_NoSavedSettings);
			
			Sample* newSample = 0;
			for (int i = 0; i < g_nsamples; ++i)
			{
				if (ImGui::Selectable(g_samples[i].name.c_str()))
				{
					newSample = g_samples[i].create();
					if (newSample)
					{
						sampleName = g_samples[i].name;
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
				showSample = false;
			}
			
			if (geom || sample)
			{
				const float* bmin = 0;
				const float* bmax = 0;
				if (geom)
				{
					bmin = geom->getNavMeshBoundsMin();
					bmax = geom->getNavMeshBoundsMax();
				}
				// Reset camera and fog to match the mesh bounds.
				if (bmin && bmax)
				{
					camr = sqrtf(rcSqr(bmax[0]-bmin[0]) +
								 rcSqr(bmax[1]-bmin[1]) +
								 rcSqr(bmax[2]-bmin[2])) / 2;
					cameraPos[0] = (bmax[0] + bmin[0]) / 2 + camr;
					cameraPos[1] = (bmax[1] + bmin[1]) / 2 + camr;
					cameraPos[2] = (bmax[2] + bmin[2]) / 2 + camr;
					camr *= 3;
				}
				cameraEulers[0] = 45;
				cameraEulers[1] = -45;
				glFogf(GL_FOG_START, camr*0.1f);
				glFogf(GL_FOG_END, camr*1.25f);
			}
			
			ImGui::End();
		}
		
		// Test cases
		if (showTestCases)
		{
			const char* filename = noc_file_dialog_open(NOC_FILE_DIALOG_OPEN, "txt\0*.txt", "TestCases", 0);
			
			showTestCases = false;
			if (filename) {
				test = new TestCase;
				// Load the test.
				if (!test->load(filename))
				{
					delete test;
					test = 0;
				}

				// Create sample
				delete sample;
				sample = 0;
				for (int i = 0; i < g_nsamples; ++i)
				{
					if (g_samples[i].name == test->getSampleName())
					{
						sampleName = g_samples[i].name;
						sample = g_samples[i].create();
					}
				}

				if (sample)
				{
					sample->setContext(&ctx);
					showSample = false;
				}

				// Load geom.
				meshName = test->getGeomFileName();
				
				string path = string(meshesFolder) + "/" + meshName;
				
				delete geom;
				geom = new InputGeom;
				if (!geom->load(&ctx, path))
				{
					delete geom;
					geom = 0;
					delete sample;
					sample = 0;
					showLog = true;
					//logScroll = 0;
					ctx.dumpLog("Geom load log %s:", meshName.c_str());
				}

				if (sample)
				{
					if (geom)
					{
						sample->handleMeshChanged(geom);
					}

					// This will ensure that tile & poly bits are updated in tiled sample.
					sample->handleSettings();
				}

				ctx.resetLog();
				if (sample && !sample->handleBuild())
				{
					ctx.dumpLog("Build log %s:", meshName.c_str());
				}
				
				resetCameraToMeshBounds(geom, sample, camr, cameraPos, cameraEulers);
				
				// Do the tests.
				if (sample)
				{
					test->doTests(sample->getNavMesh(), sample->getNavMeshQuery());
				}
			}
		}
		
		// Log
		if (showLog && showMenu)
		{
			ImGui::SetNextWindowSize(ImVec2(width - 250 - 250 - 20 - 20, 200));
			ImGui::SetNextWindowPos(ImVec2(250 + 20, height - 200 - 10));
			ImGui::Begin("Log", 0, ImGuiWindowFlags_NoSavedSettings);
			for (int i = 0; i < ctx.getLogCount(); ++i)
			{
				//imguiLabel(ctx.getLogText(i));
				ImGui::Text("%s", ctx.getLogText(i));
			}
			
			ImGui::End();
		}
		
		// Left column tools menu
		if (!showTestCases && showTools && showMenu)
		{
			ImGui::SetNextWindowSize(ImVec2(250, height - 20), ImGuiSetCond_Once);
			ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiSetCond_Once);
			ImGui::Begin("Tools", 0, ImGuiWindowFlags_NoSavedSettings);
			ImGui::PushItemWidth(-130);

			if (sample)
			{
				sample->handleTools();
			}

			ImGui::PopItemWidth();
			ImGui::End();
		}
	
		// Marker
		if (markerPositionSet && gluProject(static_cast<GLdouble>(markerPosition[0]), static_cast<GLdouble>(markerPosition[1]), 
			static_cast<GLdouble>(markerPosition[2]), modelviewMatrix, projectionMatrix, viewport, &x, &y, &z))
		{
			// Draw marker circle
			glLineWidth(5.0f);
			glColor4ub(240, 220, 0, 196);
			glBegin(GL_LINE_LOOP);
			const float r = 25.0f;
			for (int i = 0; i < 20; ++i)
			{
				const float a = static_cast<float>(i) / 20.0f * RC_PI * 2;
				const float fx = static_cast<float>(x) + cosf(a) * r;
				const float fy = static_cast<float>(y) + sinf(a) * r;
				glVertex2f(fx, fy);
			}
			glEnd();
			glLineWidth(1.0f);
		}
		
		ImGui::Render();
		
		glEnable(GL_DEPTH_TEST);
		SDL_GL_SwapWindow(window);
	}
	
	ImGui_ImplSdl_Shutdown();
	SDL_GL_DeleteContext(glcontext);
	SDL_DestroyWindow(window);
	SDL_Quit();
	
	delete sample;
	delete geom;
	
	return 0;
}
