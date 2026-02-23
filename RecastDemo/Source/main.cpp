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

#include "AppState.h"
#include "InputGeom.h"
#include "SDL_opengl.h"
#include "Sample.h"
#include "TestCase.h"
//#include "imguiHelpers.h"

#include <imgui_impl_opengl2.h>
#include <imgui_impl_sdl2.h>
#include <implot.h>

#ifdef __APPLE__
#	include <OpenGL/glu.h>
#else
#	include <GL/glu.h>
#endif


namespace
{
// Constants
constexpr float UPDATE_TIME = 1.0f / 60.0f;  // update at 60Hz
constexpr float FOG_COLOR[4] = {0.32f, 0.31f, 0.30f, 1.0f};

constexpr float CAM_MOVE_SPEED = 4.0f;
constexpr float CAM_FAST_MOVE_SPEED = 22.0f;
}

AppState app;

int main(int /*argc*/, char** /*argv*/)
{
	// Init SDL
	if (SDL_Init(SDL_INIT_EVERYTHING) != 0)
	{
		printf("Could not initialize SDL.\nError: %s\n", SDL_GetError());
		return -1;
	}

	// Use OpenGL render driver.
	SDL_SetHint(SDL_HINT_RENDER_DRIVER, "opengl");

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

	// Create the SDL window with OpenGL support
	SDL_DisplayMode displayMode;
	SDL_GetCurrentDisplayMode(0, &displayMode);
	app.window = SDL_CreateWindow(
		"Recast Demo",
		SDL_WINDOWPOS_CENTERED,
		SDL_WINDOWPOS_CENTERED,
		displayMode.w - 80,
		displayMode.h - 80,
		SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);

	// Create the OpenGL context
	app.glContext = SDL_GL_CreateContext(app.window);
	SDL_GL_MakeCurrent(app.window, app.glContext);

	if (!app.window || !app.glContext)
	{
		printf("Could not initialize SDL opengl\nError: %s\n", SDL_GetError());
		return -1;
	}

	SDL_GL_SetSwapInterval(1);  // Enable vsync
	app.updateWindowSize();

	// Setup Dear ImGui context
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImPlot::CreateContext();
	ImGui_ImplSDL2_InitForOpenGL(app.window, app.glContext);
	ImGui_ImplOpenGL2_Init();
	ImGui::StyleColorsDark();

	ImGuiIO& io = ImGui::GetIO();
	io.Fonts->AddFontFromFileTTF("DroidSans.ttf", 16.0f);
	ImGui::PushFont(io.Fonts->Fonts[0]);

	app.updateUIScale();

	app.prevFrameTime = SDL_GetTicks();

	// Set up fog.
	glEnable(GL_FOG);
	glFogi(GL_FOG_MODE, GL_LINEAR);
	glFogf(GL_FOG_START, app.camr * 0.1f);
	glFogf(GL_FOG_END, app.camr * 1.25f);
	glFogfv(GL_FOG_COLOR, FOG_COLOR);

	// Enable backface culling
	glEnable(GL_CULL_FACE);
	glDepthFunc(GL_LEQUAL);

	bool done = false;
	while (!done)
	{
		// Handle input events.
		bool processHitTest = false;
		bool processHitTestShift = false;

		// Per frame input
		app.mouseOverMenu = ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow);
		SDL_Event event;
		while (SDL_PollEvent(&event))
		{
			ImGui_ImplSDL2_ProcessEvent(&event);
			switch (event.type)
			{
			case SDL_KEYDOWN:
				// Handle any key presses here.
				switch (event.key.keysym.sym)
				{
				case SDLK_ESCAPE:
					done = true;
					break;
				case SDLK_t:
					app.showTestCases = true;
					app.files.clear();
					FileIO::scanDirectory(app.testCasesFolder, ".txt", app.files);
					break;
				case SDLK_TAB:
					app.showMenu = !app.showMenu;
					break;
				case SDLK_SPACE:
					if (app.sample)
					{
						app.sample->onToggle();
					}
					break;
				case SDLK_1:
					if (app.sample)
					{
						app.sample->singleStep();
					}
					break;
				case SDLK_9:
					if (app.sample && app.inputGeometry)
					{
						BuildSettings settings;
						rcVcopy(settings.navMeshBMin, app.inputGeometry->getNavMeshBoundsMin());
						rcVcopy(settings.navMeshBMax, app.inputGeometry->getNavMeshBoundsMax());
						app.sample->collectSettings(settings);
						app.inputGeometry->saveGeomSet(&settings);
					}
					break;
				}
				break;

			case SDL_MOUSEWHEEL:
				if (!app.mouseOverMenu)
				{
					app.scrollZoom += static_cast<float>(event.wheel.y);
				}
				break;
			case SDL_MOUSEBUTTONDOWN:
				if (event.button.button == SDL_BUTTON_RIGHT && !app.mouseOverMenu)
				{
					// Rotate view
					app.isRotatingCamera = true;
					app.movedDuringRotate = false;
					app.origMousePos[0] = app.mousePos[0];
					app.origMousePos[1] = app.mousePos[1];
					app.origCameraEulers[0] = app.cameraEulers[0];
					app.origCameraEulers[1] = app.cameraEulers[1];
				}
				break;

			case SDL_MOUSEBUTTONUP:
				// Handle mouse clicks here.
				if (event.button.button == SDL_BUTTON_RIGHT)
				{
					app.isRotatingCamera = false;
					if (!app.mouseOverMenu && !app.movedDuringRotate)
					{
						processHitTest = true;
						processHitTestShift = true;
					}
				}
				else if (event.button.button == SDL_BUTTON_LEFT)
				{
					if (!app.mouseOverMenu)
					{
						processHitTest = true;
						processHitTestShift = (SDL_GetModState() & KMOD_SHIFT) ? true : false;
					}
				}
				break;

			case SDL_MOUSEMOTION:
				app.mousePos[0] = event.motion.x;
				app.mousePos[1] = app.height - 1 - event.motion.y;

				if (app.isRotatingCamera)
				{
					int dx = app.mousePos[0] - app.origMousePos[0];
					int dy = app.mousePos[1] - app.origMousePos[1];
					app.cameraEulers[0] = app.origCameraEulers[0] - static_cast<float>(dy) * 0.25f;
					app.cameraEulers[1] = app.origCameraEulers[1] + static_cast<float>(dx) * 0.25f;
					if (dx * dx + dy * dy > 3 * 3)
					{
						app.movedDuringRotate = true;
					}
				}
				break;

			case SDL_WINDOWEVENT:
			{
				if (event.window.event == SDL_WINDOWEVENT_RESIZED)
				{
					app.updateWindowSize();
					app.updateUIScale();
				}
			}
			break;

			case SDL_QUIT:
				done = true;
				break;

			default:
				break;
			}
		}

		Uint32 time = SDL_GetTicks();
		float dt = static_cast<float>(time - app.prevFrameTime) / 1000.0f;
		app.prevFrameTime = time;

		// Hit test mesh.
		if (processHitTest && app.inputGeometry && app.sample)
		{
			float hitTime;
			if (app.inputGeometry->raycastMesh(app.rayStart, app.rayEnd, hitTime))
			{
				float hitPos[3];
				hitPos[0] = app.rayStart[0] + (app.rayEnd[0] - app.rayStart[0]) * hitTime;
				hitPos[1] = app.rayStart[1] + (app.rayEnd[1] - app.rayStart[1]) * hitTime;
				hitPos[2] = app.rayStart[2] + (app.rayEnd[2] - app.rayStart[2]) * hitTime;
				app.sample->onClick(app.rayStart, hitPos, processHitTestShift);
			}
		}

		if (app.sample)
		{
			// Update sample simulation.
			app.timeAcc = rcClamp(app.timeAcc + dt, -1.0f, 1.0f);
			while (app.timeAcc > UPDATE_TIME)
			{
				app.timeAcc -= UPDATE_TIME;
				app.sample->update(UPDATE_TIME);
			}
		}
		else
		{
			app.timeAcc = 0;
		}

		// Clear the screen
		glClearColor(0.3f, 0.3f, 0.32f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glDisable(GL_TEXTURE_2D);
		glEnable(GL_DEPTH_TEST);

		// Set the modelview matrix.
		glMatrixMode(GL_MODELVIEW);
		glLoadIdentity();
		glRotatef(app.cameraEulers[0], 1, 0, 0);
		glRotatef(app.cameraEulers[1], 0, 1, 0);
		glTranslatef(-app.cameraPos[0], -app.cameraPos[1], -app.cameraPos[2]);
		GLdouble modelviewMatrix[16];
		glGetDoublev(GL_MODELVIEW_MATRIX, modelviewMatrix);

		// Get hit ray position and direction.
		int mouseLogicalX;
		int mouseLogicalY;
		SDL_GetMouseState(&mouseLogicalX, &mouseLogicalY);

		// Scale mouse coordinates in accordance with high-dpi scale
		const float scaleX = static_cast<float>(app.drawableWidth) / app.width;
		const float scaleY = static_cast<float>(app.drawableHeight) / app.height;
		const float mouseX = mouseLogicalX * scaleX;
		const float mouseY = app.drawableHeight - mouseLogicalY * scaleY;  // Flip Y (OpenGL origin is bottom-left)

		GLdouble x, y, z;
		gluUnProject(mouseX, mouseY, 0.0, modelviewMatrix, app.projectionMatrix, app.viewport, &x, &y, &z);
		app.rayStart[0] = static_cast<float>(x);
		app.rayStart[1] = static_cast<float>(y);
		app.rayStart[2] = static_cast<float>(z);

		gluUnProject(mouseX, mouseY, 1.0, modelviewMatrix, app.projectionMatrix, app.viewport, &x, &y, &z);
		app.rayEnd[0] = static_cast<float>(x);
		app.rayEnd[1] = static_cast<float>(y);
		app.rayEnd[2] = static_cast<float>(z);

		// Keyboard movement.
		const Uint8* keystate = SDL_GetKeyboardState(NULL);
		app.moveFront = rcClamp(app.moveFront + dt * 4 * ((keystate[SDL_SCANCODE_W] || keystate[SDL_SCANCODE_UP]) ? 1.0f : -1.0f), 0.0f, 1.0f);
		app.moveLeft = rcClamp(app.moveLeft + dt * 4 * ((keystate[SDL_SCANCODE_A] || keystate[SDL_SCANCODE_LEFT]) ? 1.0f : -1.0f), 0.0f, 1.0f);
		app.moveBack = rcClamp(app.moveBack + dt * 4 * ((keystate[SDL_SCANCODE_S] || keystate[SDL_SCANCODE_DOWN]) ? 1.0f : -1.0f), 0.0f, 1.0f);
		app.moveRight = rcClamp(app.moveRight + dt * 4 * ((keystate[SDL_SCANCODE_D] || keystate[SDL_SCANCODE_RIGHT]) ? 1.0f : -1.0f), 0.0f, 1.0f);
		app.moveUp = rcClamp(app.moveUp + dt * 4 * ((keystate[SDL_SCANCODE_Q] || keystate[SDL_SCANCODE_PAGEUP]) ? 1.0f : -1.0f), 0.0f, 1.0f);
		app.moveDown = rcClamp(app.moveDown + dt * 4 * ((keystate[SDL_SCANCODE_E] || keystate[SDL_SCANCODE_PAGEDOWN]) ? 1.0f : -1.0f), 0.0f, 1.0f);

		const float keybSpeed = (SDL_GetModState() & KMOD_SHIFT) ? CAM_MOVE_SPEED : CAM_FAST_MOVE_SPEED;
		float moveX = (app.moveRight - app.moveLeft) * keybSpeed * dt;
		float moveY = (app.moveBack - app.moveFront) * keybSpeed * dt + app.scrollZoom * 2.0f;
		app.scrollZoom = 0;

		app.cameraPos[0] += moveX * static_cast<float>(modelviewMatrix[0]);
		app.cameraPos[1] += moveX * static_cast<float>(modelviewMatrix[4]);
		app.cameraPos[2] += moveX * static_cast<float>(modelviewMatrix[8]);

		app.cameraPos[0] += moveY * static_cast<float>(modelviewMatrix[2]);
		app.cameraPos[1] += moveY * static_cast<float>(modelviewMatrix[6]);
		app.cameraPos[2] += moveY * static_cast<float>(modelviewMatrix[10]);

		app.cameraPos[1] += (app.moveUp - app.moveDown) * keybSpeed * dt;

		// Draw the mesh
		glEnable(GL_FOG);
		if (app.sample)
		{
			app.sample->render();
		}
		if (app.testCase)
		{
			app.testCase->render();
		}
		glDisable(GL_FOG);

		ImGui_ImplOpenGL2_NewFrame();
		ImGui_ImplSDL2_NewFrame();
		ImGui::NewFrame();
		// ImGui::ShowDemoWindow();
		// ImPlot::ShowDemoWindow();

		if (app.sample)
		{
			app.sample->renderOverlay();
		}
		if (app.testCase)
		{
			app.testCase->renderOverlay();
		}

		app.drawUI();

		ImGui::Render();
		ImGui_ImplOpenGL2_RenderDrawData(ImGui::GetDrawData());
		SDL_GL_SwapWindow(app.window);
	}

	ImGui_ImplOpenGL2_Shutdown();
	ImGui_ImplSDL2_Shutdown();
	ImPlot::DestroyContext();
	ImGui::DestroyContext();

	SDL_GL_DeleteContext(app.glContext);
	SDL_DestroyWindow(app.window);
	SDL_Quit();

	return 0;
}
