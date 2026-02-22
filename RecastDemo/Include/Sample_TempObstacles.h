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

#pragma once

#include "Sample.h"

struct LinearAllocator;
struct FastLZCompressor;
struct MeshProcess;
class dtTileCache;

class Sample_TempObstacles : public Sample
{
protected:
	bool keepIntermediateResults = false;

	LinearAllocator* tAllocator;
	FastLZCompressor* tCompressor;
	MeshProcess* tMeshProcess;

	dtTileCache* tileCache = nullptr;

	float cacheBuildTimeMs = 0;
	int cacheCompressedSize = 0;
	int cacheRawSize = 0;
	int cacheLayerCount = 0;
	unsigned int cacheBuildMemUsage = 0;

	enum class DrawMode : uint8_t
	{
		NAVMESH,
		NAVMESH_TRANS,
		NAVMESH_BVTREE,
		NAVMESH_NODES,
		NAVMESH_PORTALS,
		NAVMESH_INVIS,
		MESH,
		CACHE_BOUNDS,

		MAX_DRAWMODE
	};
	DrawMode drawMode = DrawMode::NAVMESH;

	int maxTiles = 0;
	int maxPolysPerTile = 0;
	int tileSize = 48;

public:
	Sample_TempObstacles();
	~Sample_TempObstacles() override;
	Sample_TempObstacles(const Sample_TempObstacles&) = delete;
	Sample_TempObstacles& operator=(const Sample_TempObstacles&) = delete;
	Sample_TempObstacles(const Sample_TempObstacles&&) = delete;
	Sample_TempObstacles& operator=(const Sample_TempObstacles&&) = delete;

	bool supportsTool(SampleToolType toolType) const override;
	void drawSettingsUI() override;
	void drawDebugUI() override;
	void render() override;
	void renderOverlay() override;
	void onMeshChanged(InputGeom* geom) override;
	bool build() override;
	void update(float dt) override;

	void getTilePos(const float* pos, int& tx, int& ty);

	void renderCachedTile(int tx, int ty, int type);
	void renderCachedTileOverlay(int tx, int ty) const;

	void addTempObstacle(const float* pos) const;
	void removeTempObstacle(const float* sp, const float* sq) const;
	void clearAllTempObstacles() const;

	void saveAll(const char* path) const;
	void loadAll(const char* path);

private:
	int rasterizeTileLayers(int tx, int ty, const rcConfig& cfg, struct TileCacheData* tiles, int maxTiles) const;
};
