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

#include "Recast.h"
#include "RecastAlloc.h"
#include "RecastAssert.h"

#include <math.h>

/// Check whether two bounding boxes overlap
///
/// @param[in]	aMin	Min axis extents of bounding box A
/// @param[in]	aMax	Max axis extents of bounding box A
/// @param[in]	bMin	Min axis extents of bounding box B
/// @param[in]	bMax	Max axis extents of bounding box B
/// @returns true if the two bounding boxes overlap.  False otherwise.
static bool overlapBounds(const rcVec3f aMin, const rcVec3f aMax, const rcVec3f bMin, const rcVec3f bMax)
{
	return
		aMin.x <= bMax.x && aMax.x >= bMin.x &&
		aMin.y <= bMax.y && aMax.y >= bMin.y &&
		aMin.z <= bMax.z && aMax.z >= bMin.z;
}

/// Allocates a new span in the heightfield.
/// Use a memory pool and free list to minimize actual allocations.
/// 
/// @param[in]	hf		The heightfield
/// @returns A pointer to the allocated or re-used span memory. 
static rcSpan* allocSpan(rcHeightfield& hf)
{
	// If necessary, allocate new page and update the freelist.
	if (hf.freelist == NULL || hf.freelist->next == NULL)
	{
		// Create new page.
		// Allocate memory for the new pool.
		rcSpanPool* spanPool = (rcSpanPool*)rcAlloc(sizeof(rcSpanPool), RC_ALLOC_PERM);
		if (spanPool == NULL)
		{
			return NULL;
		}

		// Add the pool into the list of pools.
		spanPool->next = hf.pools;
		hf.pools = spanPool;
		
		// Add new spans to the free list.
		rcSpan* freeList = hf.freelist;
		rcSpan* head = &spanPool->items[0];
		rcSpan* it = &spanPool->items[RC_SPANS_PER_POOL];
		do
		{
			--it;
			it->next = freeList;
			freeList = it;
		}
		while (it != head);
		hf.freelist = it;
	}

	// Pop item from the front of the free list.
	rcSpan* newSpan = hf.freelist;
	hf.freelist = hf.freelist->next;
	return newSpan;
}

/// Releases the memory used by the span back to the heightfield, so it can be re-used for new spans.
/// @param[in]	hf		The heightfield.
/// @param[in]	span	A pointer to the span to free
static void freeSpan(rcHeightfield& hf, rcSpan* span)
{
	if (span == NULL)
	{
		return;
	}
	// Add the span to the front of the free list.
	span->next = hf.freelist;
	hf.freelist = span;
}

/// Adds a span to the heightfield.  If the new span overlaps existing spans,
/// it will merge the new span with the existing ones.
///
/// @param[in]	hf					Heightfield to add spans to
/// @param[in]	x					The new span's column cell x index
/// @param[in]	z					The new span's column cell z index
/// @param[in]	min					The new span's minimum cell index
/// @param[in]	max					The new span's maximum cell index
/// @param[in]	areaID				The new span's area type ID
/// @param[in]	flagMergeThreshold	How close two spans maximum extents need to be to merge area type IDs
static bool addSpan(rcHeightfield& hf,
                    const int x, const int z,
                    const unsigned short min, const unsigned short max,
                    const unsigned char areaID, const int flagMergeThreshold)
{
	// Create the new span.
	rcSpan* newSpan = allocSpan(hf);
	if (newSpan == NULL)
	{
		return false;
	}
	newSpan->smin = min;
	newSpan->smax = max;
	newSpan->area = areaID;
	newSpan->next = NULL;
	
	const int columnIndex = x + z * hf.width;
	rcSpan* previousSpan = NULL;
	rcSpan* currentSpan = hf.spans[columnIndex];
	
	// Insert the new span, possibly merging it with existing spans.
	while (currentSpan != NULL)
	{
		if (currentSpan->smin > newSpan->smax)
		{
			// Current span is completely after the new span, break.
			break;
		}
		
		if (currentSpan->smax < newSpan->smin)
		{
			// Current span is completely before the new span.  Keep going.
			previousSpan = currentSpan;
			currentSpan = currentSpan->next;
		}
		else
		{
			// The new span overlaps with an existing span.  Merge them.
			if (currentSpan->smin < newSpan->smin)
			{
				newSpan->smin = currentSpan->smin;
			}
			if (currentSpan->smax > newSpan->smax)
			{
				newSpan->smax = currentSpan->smax;
			}
			
			// Merge flags.
			if (rcAbs((int)newSpan->smax - (int)currentSpan->smax) <= flagMergeThreshold)
			{
				// Higher area ID numbers indicate higher resolution priority.
				newSpan->area = rcMax(newSpan->area, currentSpan->area);
			}
			
			// Remove the current span since it's now merged with newSpan.
			// Keep going because there might be other overlapping spans that also need to be merged.
			rcSpan* next = currentSpan->next;
			freeSpan(hf, currentSpan);
			if (previousSpan)
			{
				previousSpan->next = next;
			}
			else
			{
				hf.spans[columnIndex] = next;
			}
			currentSpan = next;
		}
	}
	
	// Insert new span after prev
	if (previousSpan != NULL)
	{
		newSpan->next = previousSpan->next;
		previousSpan->next = newSpan;
	}
	else
	{
		// This span should go before the others in the list
		newSpan->next = hf.spans[columnIndex];
		hf.spans[columnIndex] = newSpan;
	}

	return true;
}

bool rcAddSpan(rcContext* context, rcHeightfield& heightfield,
               const int x, const int z,
               const unsigned short spanMin, const unsigned short spanMax,
               const unsigned char areaID, const int flagMergeThreshold)
{
	rcAssert(context);

	if (!addSpan(heightfield, x, z, spanMin, spanMax, areaID, flagMergeThreshold))
	{
		context->log(RC_LOG_ERROR, "rcAddSpan: Out of memory.");
		return false;
	}

	return true;
}

enum rcAxis
{
	RC_AXIS_X = 0,
	RC_AXIS_Y = 1,
	RC_AXIS_Z = 2
};

/// Divides a convex polygon of max 12 vertices into two convex polygons
/// across a separating axis.
/// 
/// @param[in]	inVerts			The input polygon vertices
/// @param[in]	inVertsCount	The number of input polygon vertices
/// @param[out]	outVerts1		Resulting polygon 1's vertices
/// @param[out]	outVerts1Count	The number of resulting polygon 1 vertices
/// @param[out]	outVerts2		Resulting polygon 2's vertices
/// @param[out]	outVerts2Count	The number of resulting polygon 2 vertices
/// @param[in]	axisOffset		THe offset along the specified axis
/// @param[in]	axis			The separating axis
static void dividePoly(const rcVec3f inVerts[7], int inVertsCount,
                       rcVec3f outVerts1[7], int* outVerts1Count,
                       rcVec3f outVerts2[7], int* outVerts2Count,
                       float axisOffset, rcAxis axis)
{
	rcAssert(inVertsCount <= 12);
	
	// How far positive or negative away from the separating axis is each vertex.
	float vertAxisDeltas[12];
	for (int inVert = 0; inVert < inVertsCount; ++inVert)
	{
		vertAxisDeltas[inVert] = axisOffset - inVerts[inVert][axis];
	}

	int poly1Vert = 0;
	int poly2Vert = 0;
	for (int inVertA = 0, inVertB = inVertsCount - 1; inVertA < inVertsCount; inVertB = inVertA, ++inVertA)
	{
		// If the two vertices are on the same side of the separating axis
		const bool sameSide = (vertAxisDeltas[inVertA] >= 0) == (vertAxisDeltas[inVertB] >= 0);

		if (!sameSide)
		{
			float scalar = vertAxisDeltas[inVertB] / (vertAxisDeltas[inVertB] - vertAxisDeltas[inVertA]);
			outVerts1[poly1Vert] = inVerts[inVertB] + (inVerts[inVertA] - inVerts[inVertB]) * scalar;
			outVerts2[poly2Vert] = outVerts1[poly1Vert];
			poly1Vert++;
			poly2Vert++;
			
			// add the inVertA point to the right polygon. Do NOT add points that are on the dividing line
			// since these were already added above
			if (vertAxisDeltas[inVertA] > 0)
			{
				outVerts1[poly1Vert] = inVerts[inVertA];
				poly1Vert++;
			}
			else if (vertAxisDeltas[inVertA] < 0)
			{
				outVerts2[poly2Vert] = inVerts[inVertA]; 
				poly2Vert++;
			}
		}
		else
		{
			// add the inVertA point to the right polygon. Addition is done even for points on the dividing line
			if (vertAxisDeltas[inVertA] >= 0)
			{
				outVerts1[poly1Vert] = inVerts[inVertA]; 
				poly1Vert++;
				if (vertAxisDeltas[inVertA] != 0)
				{
					continue;
				}
			}
			outVerts2[poly2Vert] = inVerts[inVertA];
			poly2Vert++;
		}
	}

	*outVerts1Count = poly1Vert;
	*outVerts2Count = poly2Vert;
}

///	Rasterize a single triangle to the heightfield.
///
///	This code is extremely hot, so much care should be given to maintaining maximum perf here.
/// 
/// @param[in] 	v0					Triangle vertex 0
/// @param[in] 	v1					Triangle vertex 1
/// @param[in] 	v2					Triangle vertex 2
/// @param[in] 	areaID				The area ID to assign to the rasterized spans
/// @param[in] 	hf					Heightfield to rasterize into
/// @param[in] 	hfBBMin				The min extents of the heightfield bounding box
/// @param[in] 	hfBBMax				The max extents of the heightfield bounding box
/// @param[in] 	cellSize			The x and z axis size of a voxel in the heightfield
/// @param[in] 	inverseCellSize		1 / cellSize
/// @param[in] 	inverseCellHeight	1 / cellHeight
/// @param[in] 	flagMergeThreshold	The threshold in which area flags will be merged 
/// @returns true if the operation completes successfully.  false if there was an error adding spans to the heightfield.
static bool rasterizeTri(const rcVec3f v0, const rcVec3f v1, const rcVec3f v2,
                         const unsigned char areaID, rcHeightfield& hf,
                         const rcVec3f hfBBMin, const rcVec3f hfBBMax,
                         const float cellSize, const float inverseCellSize, const float inverseCellHeight,
                         const int flagMergeThreshold)
{
	// Calculate the bounding box of the triangle.
	rcVec3f triBBMin = v0.min(v1).min(v2);
	rcVec3f triBBMax = v0.max(v1).max(v2);

	// If the triangle does not touch the bounding box of the heightfield, skip the triangle.
	if (!overlapBounds(triBBMin, triBBMax, hfBBMin, hfBBMax))
	{
		return true;
	}

	const int w = hf.width;
	const int h = hf.height;
	const float by = hfBBMax.y - hfBBMin.y;

	// Calculate the footprint of the triangle on the grid's z-axis
	int z0 = (int)((triBBMin.z - hfBBMin.z) * inverseCellSize);
	int z1 = (int)((triBBMax.z - hfBBMin.z) * inverseCellSize);

	// use -1 rather than 0 to cut the polygon properly at the start of the tile
	z0 = rcClamp(z0, -1, h - 1);
	z1 = rcClamp(z1, 0, h - 1);

	// Clip the triangle into all grid cells it touches.
	rcVec3f buff[7 * 4];
	rcVec3f* in = buff;
	rcVec3f* inRow = buff + 7;
	rcVec3f* p1 = inRow + 7;
	rcVec3f* p2 = p1 + 7;
	int nvRow;
	int nvIn = 3;
	
	in[0] = v0;
	in[1] = v1;
	in[2] = v2;

	for (int z = z0; z <= z1; ++z)
	{
		// Clip polygon to row. Store the remaining polygon as well
		const float cellZ = hfBBMin.z + (float)z * cellSize;
		dividePoly(in, nvIn, inRow, &nvRow, p1, &nvIn, cellZ + cellSize, RC_AXIS_Z);
		rcSwap(in, p1);
		
		if (nvRow < 3)
		{
			continue;
		}
		if (z < 0)
		{
			continue;
		}
		
		// find X-axis bounds of the row
		float minX = inRow[0].x;
		float maxX = inRow[0].x;
		for (int vert = 1; vert < nvRow; ++vert)
		{
			if (minX > inRow[vert].x)
			{
				minX = inRow[vert].x;
			}
			if (maxX < inRow[vert].x)
			{
				maxX = inRow[vert].x;
			}
		}
		int x0 = (int)((minX - hfBBMin.x) * inverseCellSize);
		int x1 = (int)((maxX - hfBBMin.x) * inverseCellSize);
		if (x1 < 0 || x0 >= w)
		{
			continue;
		}
		x0 = rcClamp(x0, -1, w - 1);
		x1 = rcClamp(x1, 0, w - 1);

		int nv;
		int nv2 = nvRow;

		for (int x = x0; x <= x1; ++x)
		{
			// Clip polygon to column. store the remaining polygon as well
			const float cx = hfBBMin.x + (float)x * cellSize;
			dividePoly(inRow, nv2, p1, &nv, p2, &nv2, cx + cellSize, RC_AXIS_X);
			rcSwap(inRow, p2);
			
			if (nv < 3)
			{
				continue;
			}
			if (x < 0)
			{
				continue;
			}
			
			// Calculate min and max of the span.
			float spanMin = p1[0].y;
			float spanMax = p1[0].y;
			for (int vert = 1; vert < nv; ++vert)
			{
				spanMin = rcMin(spanMin, p1[vert].y);
				spanMax = rcMax(spanMax, p1[vert].y);
			}
			spanMin -= hfBBMin.y;
			spanMax -= hfBBMin.y;
			
			// Skip the span if it's completely outside the heightfield bounding box
			if (spanMax < 0.0f)
			{
				continue;
			}
			if (spanMin > by)
			{
				continue;
			}
			
			// Clamp the span to the heightfield bounding box.
			if (spanMin < 0.0f)
			{
				spanMin = 0;
			}
			if (spanMax > by)
			{
				spanMax = by;
			}

			// Snap the span to the heightfield height grid.
			unsigned short spanMinCellIndex = (unsigned short)rcClamp((int)floorf(spanMin * inverseCellHeight), 0, RC_SPAN_MAX_HEIGHT);
			unsigned short spanMaxCellIndex = (unsigned short)rcClamp((int)ceilf(spanMax * inverseCellHeight), (int)spanMinCellIndex + 1, RC_SPAN_MAX_HEIGHT);

			if (!addSpan(hf, x, z, spanMinCellIndex, spanMaxCellIndex, areaID, flagMergeThreshold))
			{
				return false;
			}
		}
	}

	return true;
}

bool rcRasterizeTriangle(rcContext* context,
                         const float* v0, const float* v1, const float* v2,
                         const unsigned char areaID, rcHeightfield& heightfield, const int flagMergeThreshold)
{
	rcAssert(context != NULL);

	rcScopedTimer timer(context, RC_TIMER_RASTERIZE_TRIANGLES);

	// Rasterize the single triangle.
	const float inverseCellSize = 1.0f / heightfield.cs;
	const float inverseCellHeight = 1.0f / heightfield.ch;
	if (!rasterizeTri(*(rcVec3f*)v0, *(rcVec3f*)v1, *(rcVec3f*)v2, areaID, heightfield, *(rcVec3f*)heightfield.bmin, *(rcVec3f*)heightfield.bmax, heightfield.cs, inverseCellSize, inverseCellHeight, flagMergeThreshold))
	{
		context->log(RC_LOG_ERROR, "rcRasterizeTriangle: Out of memory.");
		return false;
	}

	return true;
}

bool rcRasterizeTriangles(rcContext* context,
                          const float* verts, const int /*nv*/,
                          const int* tris, const unsigned char* triAreaIDs, const int numTris,
                          rcHeightfield& heightfield, const int flagMergeThreshold)
{
	rcAssert(context != NULL);

	rcScopedTimer timer(context, RC_TIMER_RASTERIZE_TRIANGLES);

	const rcVec3f* vec3Verts = (rcVec3f*)verts;

	const rcVec3f vecHFMin = *(rcVec3f*)heightfield.bmin;
	const rcVec3f vecHFMax = *(rcVec3f*)heightfield.bmax;
	
	// Rasterize the triangles.
	const float inverseCellSize = 1.0f / heightfield.cs;
	const float inverseCellHeight = 1.0f / heightfield.ch;
	for (int triIndex = 0; triIndex < numTris; ++triIndex)
	{
		const rcVec3f& v0 = vec3Verts[tris[triIndex * 3 + 0]];
		const rcVec3f& v1 = vec3Verts[tris[triIndex * 3 + 1]];
		const rcVec3f& v2 = vec3Verts[tris[triIndex * 3 + 2]];
		if (!rasterizeTri(v0, v1, v2, triAreaIDs[triIndex], heightfield, vecHFMin, vecHFMax, heightfield.cs, inverseCellSize, inverseCellHeight, flagMergeThreshold))
		{
			context->log(RC_LOG_ERROR, "rcRasterizeTriangles: Out of memory.");
			return false;
		}
	}

	return true;
}

bool rcRasterizeTriangles(rcContext* context,
                          const float* verts, const int /*nv*/,
                          const unsigned short* tris, const unsigned char* triAreaIDs, const int numTris,
                          rcHeightfield& heightfield, const int flagMergeThreshold)
{
	rcAssert(context != NULL);

	const rcVec3f* vec3Verts = (rcVec3f*)verts;

	const rcVec3f vecHFMin = *(rcVec3f*)heightfield.bmin;
	const rcVec3f vecHFMax = *(rcVec3f*)heightfield.bmax;

	rcScopedTimer timer(context, RC_TIMER_RASTERIZE_TRIANGLES);

	// Rasterize the triangles.
	const float inverseCellSize = 1.0f / heightfield.cs;
	const float inverseCellHeight = 1.0f / heightfield.ch;
	for (int triIndex = 0; triIndex < numTris; ++triIndex)
	{
		const rcVec3f& v0 = vec3Verts[tris[triIndex * 3 + 0]];
		const rcVec3f& v1 = vec3Verts[tris[triIndex * 3 + 1]];
		const rcVec3f& v2 = vec3Verts[tris[triIndex * 3 + 2]];
		if (!rasterizeTri(v0, v1, v2, triAreaIDs[triIndex], heightfield, vecHFMin, vecHFMax, heightfield.cs, inverseCellSize, inverseCellHeight, flagMergeThreshold))
		{
			context->log(RC_LOG_ERROR, "rcRasterizeTriangles: Out of memory.");
			return false;
		}
	}

	return true;
}

bool rcRasterizeTriangles(rcContext* context,
                          const float* verts, const unsigned char* triAreaIDs, const int numTris,
                          rcHeightfield& heightfield, const int flagMergeThreshold)
{
	rcAssert(context != NULL);

	rcVec3f* vec3Verts = (rcVec3f*)verts;

	const rcVec3f vecHFMin = *(rcVec3f*)heightfield.bmin;
	const rcVec3f vecHFMax = *(rcVec3f*)heightfield.bmax;

	rcScopedTimer timer(context, RC_TIMER_RASTERIZE_TRIANGLES);
	
	// Rasterize the triangles.
	const float inverseCellSize = 1.0f / heightfield.cs;
	const float inverseCellHeight = 1.0f / heightfield.ch;
	for (int triIndex = 0; triIndex < numTris; ++triIndex)
	{
		const rcVec3f& v0 = vec3Verts[(triIndex * 3 + 0)];
		const rcVec3f& v1 = vec3Verts[(triIndex * 3 + 1)];
		const rcVec3f& v2 = vec3Verts[(triIndex * 3 + 2)];
		if (!rasterizeTri(v0, v1, v2, triAreaIDs[triIndex], heightfield, vecHFMin, vecHFMax, heightfield.cs, inverseCellSize, inverseCellHeight, flagMergeThreshold))
		{
			context->log(RC_LOG_ERROR, "rcRasterizeTriangles: Out of memory.");
			return false;
		}
	}

	return true;
}
