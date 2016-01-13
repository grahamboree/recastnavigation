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

#include "MeshLoaderObj.h"

#include "Recast.h"

#include <stdio.h>
#include <stdlib.h>
#include <cstring>
#define _USE_MATH_DEFINES
#include <math.h>

#include <iostream>
#include <fstream>
#include <sstream>

using std::string;
using std::vector;

rcMeshLoaderObj::rcMeshLoaderObj() :
	m_scale(1.0f)
{
}

bool rcMeshLoaderObj::load(const std::string& filename)
{
	std::ifstream meshFile;
	meshFile.open(filename);

 	string type;
	string skip;

	// This helps mitigate lots of vector resizes.
	// We don't actually know how many verts or tris
	// there are, but 10k is a reasonable guess.
	m_verts.reserve(10000);
	m_tris.reserve(10000);

	string v1, v2, v3;

	// Read mesh
	while (!meshFile.eof())
	{
		// Get the line type.
		meshFile >> type;

		if (type == "v")
		{
			// Vertex position
			float x, y, z;
			meshFile >> x >> y >> z;
			m_verts.push_back(x * m_scale);
			m_verts.push_back(y * m_scale);
			m_verts.push_back(z * m_scale);
		}
		else if (type == "f")
		{
			// Face
			meshFile >> v1 >> v2 >> v3;

			// Only grab the vertex position index, not the normal or texture coordinate indices.
			v1 = v1.substr(0, v1.find('/'));
			v2 = v2.substr(0, v2.find('/'));
			v3 = v3.substr(0, v3.find('/'));

			int a = atoi(v1.c_str());
			int b = atoi(v2.c_str());
			int c = atoi(v3.c_str());

			// Only add the face if the vert indices are valid.
			if (a > 0 && a <= m_verts.size() && b > 0 && b <= m_verts.size() && c > 0 && c <= m_verts.size())
			{
				m_tris.push_back(a - 1);
				m_tris.push_back(b - 1);
				m_tris.push_back(c - 1);
			}
		}
		else
		{
			// Skip every other kind of line.  This line reads from the stream up to the next newline.
			std::getline(meshFile, skip);
		}
	}

	// Calculate face normals.
	m_normals.resize(m_tris.size());
	for (int i = 0; i < m_tris.size(); i += 3)
	{
		// Get pointers to the tri's vertex data.
		const float* v0 = &m_verts[m_tris[i] * 3];
		const float* v1 = &m_verts[m_tris[i + 1] * 3];
		const float* v2 = &m_verts[m_tris[i + 2] * 3];
		
		// Calculate 2 edges of the triangle.
		float edge0[3], edge1[3];
		rcVsub(edge0, v1, v0);
		rcVsub(edge1, v2, v0);

		// Compute and normalize the dot product of the two edges.
		float* normal = &m_normals[i];
		rcVcross(normal, edge0, edge1);
		rcVnormalize(normal);
	}
	
	m_filename = filename;
	return true;
}
