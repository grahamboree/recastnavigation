#include "AppState.h"

#include "InputGeom.h"
#include "SDL_opengl.h"
#include "Sample.h"
#include "Sample_SoloMesh.h"
#include "Sample_TempObstacles.h"
#include "Sample_TileMesh.h"
#include "TestCase.h"
#include "imguiHelpers.h"

#include <functional>

#ifdef __APPLE__
#	include <OpenGL/glu.h>
#else
#	include <GL/glu.h>
#endif

#include <imgui.h>

namespace
{
constexpr ImGuiWindowFlags staticWindowFlags = ImGuiWindowFlags_NoMove
	| ImGuiWindowFlags_NoResize
	| ImGuiWindowFlags_NoSavedSettings
	| ImGuiWindowFlags_NoCollapse;

struct SampleItem
{
	std::string name;
	std::function<std::unique_ptr<Sample>()> create;
};
SampleItem g_samples[] = {
	{.name = "Solo Mesh",      .create = []() { return std::make_unique<Sample_SoloMesh>(); }     },
	{.name = "Tile Mesh",      .create = []() { return std::make_unique<Sample_TileMesh>(); }     },
	{.name = "Temp Obstacles", .create = []() { return std::make_unique<Sample_TempObstacles>(); }},
};
}

void AppState::resetCamera()
{
	// Reset camera and fog to match the mesh bounds.
	if (inputGeometry)
	{
		const float* boundsMin = inputGeometry->getNavMeshBoundsMin();
		const float* boundsMax = inputGeometry->getNavMeshBoundsMax();

		camr = rcSqr(boundsMax[0] - boundsMin[0]);
		camr += rcSqr(boundsMax[1] - boundsMin[1]);
		camr += rcSqr(boundsMax[2] - boundsMin[2]);
		camr = sqrtf(camr) / 2.0f;

		cameraPos[0] = (boundsMax[0] + boundsMin[0]) / 2.0f + camr;
		cameraPos[1] = (boundsMax[1] + boundsMin[1]) / 2.0f + camr;
		cameraPos[2] = (boundsMax[2] + boundsMin[2]) / 2.0f + camr;

		camr *= 3.0f;
	}
	cameraEulers[0] = 45;
	cameraEulers[1] = -45;
	glFogf(GL_FOG_START, camr * 0.1f);
	glFogf(GL_FOG_END, camr * 1.25f);
}

void AppState::updateWindowSize()
{
	SDL_GetWindowSize(window, &width, &height);
	SDL_GL_GetDrawableSize(window, &drawableWidth, &drawableHeight);

	glViewport(0, 0, drawableWidth, drawableHeight);
	glGetIntegerv(GL_VIEWPORT, viewport);

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	gluPerspective(50.0f, static_cast<float>(width) / static_cast<float>(height), 1.0f, camr);
	glGetDoublev(GL_PROJECTION_MATRIX, projectionMatrix);
}

void AppState::updateUIScale() const
{
	ImGuiIO& io = ImGui::GetIO();
	io.DisplaySize = ImVec2(static_cast<float>(width), static_cast<float>(height));
	io.DisplayFramebufferScale = ImVec2(
		static_cast<float>(drawableWidth) / static_cast<float>(width),
		static_cast<float>(drawableHeight) / static_cast<float>(height));
}

void AppState::worldToScreen(float x, float y, float z, float* screenX, float* screenY) const
{
	GLdouble modelviewMatrix[16];
	glGetDoublev(GL_MODELVIEW_MATRIX, modelviewMatrix);

	GLdouble winX, winY, winZ;
	gluProject(x, y, z, modelviewMatrix, projectionMatrix, viewport, &winX, &winY, &winZ);

	const float dpiScaleX = static_cast<float>(drawableWidth) / static_cast<float>(width);
	const float dpiScaleY = static_cast<float>(drawableHeight) / static_cast<float>(height);

	*screenX = static_cast<float>(winX) / dpiScaleX;
	*screenY = static_cast<float>(height) - static_cast<float>(winY / dpiScaleY);
}

void AppState::drawUI()
{
	if (!showMenu)
	{
		return;
	}

	// Help text.
	DrawScreenspaceText(280.0f, 20.0f, IM_COL32(255, 255, 255, 128), "W/A/S/D: Move  RMB: Rotate");

	constexpr int uiColumnWidth = 250;
	constexpr int uiWindowPadding = 10;
	// Properties window
	{
		ImGui::SetNextWindowPos(
			ImVec2(static_cast<float>(width - uiColumnWidth - uiWindowPadding), uiWindowPadding),
			ImGuiCond_Always);
		ImGui::SetNextWindowSize(ImVec2(uiColumnWidth, static_cast<float>(height - uiWindowPadding * 2)), ImGuiCond_Always);
		ImGui::Begin("Properties", nullptr, staticWindowFlags);

		ImGui::Text("Show");
		ImGui::Checkbox("Build Log", &showLog);
		ImGui::Checkbox("Tools Panel", &showTools);

		ImGui::SeparatorText("Sample");

		if (ImGui::BeginCombo(
				"##sampleCombo",
				sampleIndex >= 0 ? g_samples[sampleIndex].name.c_str() : "Choose Sample...",
				0))
		{
			for (int s = 0; s < IM_ARRAYSIZE(g_samples); ++s)
			{
				const bool selected = (sampleIndex == s);
				if (ImGui::Selectable(g_samples[s].name.c_str(), selected) && !selected)
				{
					sampleIndex = s;
					sample = g_samples[sampleIndex].create();
					sample->buildContext = &buildContext;
					if (inputGeometry)
					{
						sample->onMeshChanged(inputGeometry.get());
						resetCamera();
					}
				}

				// Set the initial focus when opening the combo (scrolling + keyboard navigation focus)
				if (selected)
				{
					ImGui::SetItemDefaultFocus();
				}
			}
			ImGui::EndCombo();
		}

		ImGui::SeparatorText("Input Mesh");

		if (ImGui::BeginCombo("##inputMesh", meshName.c_str(), 0))
		{
			files.clear();
			FileIO::scanDirectory(meshesFolder, ".obj", files);
			FileIO::scanDirectory(meshesFolder, ".gset", files);

			for (const auto& file : files)
			{
				const bool selected = (meshName == file);
				if (ImGui::Selectable(file.c_str(), selected) && !selected)
				{
					meshName = file;
					std::string path = meshesFolder + "/" + meshName;

					inputGeometry = std::make_unique<InputGeom>();
					if (!inputGeometry->load(&buildContext, path))
					{
						inputGeometry.reset();

						// Destroy the sample if it already had geometry loaded, as we've just deleted it!
						if (sample && sample->inputGeometry)
						{
							sample.reset();
						}

						showLog = true;
						logScroll = 0;
						buildContext.dumpLog("geom load log %s:", meshName.c_str());
					}
					resetCamera();
					if (sample)
					{
						sample->onMeshChanged(inputGeometry.get());
					}
				}

				// Set the initial focus when opening the combo (scrolling + keyboard navigation focus)
				if (selected)
				{
					ImGui::SetItemDefaultFocus();
				}
			}
			ImGui::EndCombo();
		}

		if (inputGeometry)
		{
			DrawRightAlignedText(
				"Verts: %.1fk  Tris: %.1fk",
				static_cast<float>(inputGeometry->mesh.getVertCount()) / 1000.0f,
				static_cast<float>(inputGeometry->mesh.getTriCount()) / 1000.0f);
		}

		if (sample)
		{
			if (inputGeometry)
			{
				sample->drawSettingsUI();

				if (ImGui::Button("Build"))
				{
					buildContext.resetLog();
					if (!sample->build())
					{
						showLog = true;
						logScroll = 0;
					}
					buildContext.dumpLog("Build log %s:", meshName.c_str());

					// Clear test.
					testCase.reset();
				}
			}

			ImGui::SeparatorText("Debug Settings");
			sample->drawDebugUI();
		}

		ImGui::End();
	}

	// Log window
	if (showLog)
	{
		constexpr int logWindowHeight = 200;
		ImGui::SetNextWindowPos(
			ImVec2(uiColumnWidth + 2 * uiWindowPadding, static_cast<float>(height - logWindowHeight - uiWindowPadding)),
			ImGuiCond_FirstUseEver);  // Position in screen space
		ImGui::SetNextWindowSize(
			ImVec2(static_cast<float>(width - 2 * uiColumnWidth - 4 * uiWindowPadding), logWindowHeight),
			ImGuiCond_FirstUseEver);  // Size of the window
		ImGui::Begin("Log", nullptr, ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoCollapse);

		for (int i = 0; i < buildContext.getLogCount(); ++i)
		{
			ImGui::TextUnformatted(buildContext.getLogText(i));
		}

		ImGui::End();
	}

	// Tools window
	if (!showTestCases && showTools)
	{
		ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_Always);  // Position in screen space
		ImGui::SetNextWindowSize(ImVec2(250, static_cast<float>(height - 20)), ImGuiCond_Always);  // Size of the window
		ImGui::Begin("Tools", nullptr, staticWindowFlags);

		if (sample)
		{
			sample->drawToolsUI();
		}

		ImGui::End();
	}

	// Test cases
	if (showTestCases)
	{
		ImGui::SetNextWindowPos(
			ImVec2(static_cast<float>(width - 10 - 250 - 10 - 200), static_cast<float>(height - 10 - 450)),
			ImGuiCond_Always);  // Position in screen space
		ImGui::SetNextWindowSize(ImVec2(200, 450), ImGuiCond_Always);
		ImGui::Begin("Test Cases", nullptr, staticWindowFlags);

		static int currentTest = 0;
		int newTest = currentTest;
		if (ImGui::BeginCombo("Choose Test", files[0].c_str()))
		{
			for (int i = 0; i < static_cast<int>(files.size()); ++i)
			{
				if (ImGui::Selectable(files[i].c_str(), currentTest == i))
				{
					newTest = i;
				}

				if (currentTest == i)
				{
					ImGui::SetItemDefaultFocus();  // Sets keyboard focus
				}
			}
			ImGui::EndCombo();
		}

		if (newTest != currentTest)
		{
			currentTest = newTest;

			std::string path = testCasesFolder + "/" + files[currentTest];

			// Load the test.
			testCase = std::make_unique<TestCase>();
			if (!testCase->load(path))
			{
				testCase.reset();
			}

			// Create sample
			for (int s = 0; s < IM_ARRAYSIZE(g_samples); ++s)
			{
				if (g_samples[s].name == testCase->sampleName)
				{
					sample = g_samples[s].create();
					sampleIndex = s;
				}
			}

			if (sample)
			{
				sample->buildContext = &buildContext;
			}

			// Load geom.
			meshName = testCase->geomFileName;

			path = meshesFolder + "/" + meshName;

			inputGeometry = std::make_unique<InputGeom>();
			if (!inputGeometry->load(&buildContext, path))
			{
				inputGeometry.reset();
				sample.reset();

				showLog = true;
				logScroll = 0;
				buildContext.dumpLog("geom load log %s:", meshName.c_str());
			}

			if (sample)
			{
				if (inputGeometry)
				{
					sample->onMeshChanged(inputGeometry.get());
				}

				// This will ensure that tile & poly bits are updated in tiled sample.
				sample->drawSettingsUI();

				buildContext.resetLog();
				if (!sample->build())
				{
					buildContext.dumpLog("Build log %s:", meshName.c_str());
				}
			}

			if (inputGeometry || sample)
			{
				resetCamera();
			}

			// Do the tests.
			if (sample)
			{
				testCase->doTests(sample->navMesh, sample->navQuery);
			}
		}

		ImGui::End();
	}
}