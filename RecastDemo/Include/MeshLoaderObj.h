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

#ifndef MESHLOADER_OBJ
#define MESHLOADER_OBJ

#include <string>
#include <vector>

class rcMeshLoaderObj
{
public:
	rcMeshLoaderObj();
	
	bool load(const std::string& fileName);

	const float* getVerts() const { return m_verts.data(); }
	const float* getNormals() const { return m_normals.data(); }
	const int* getTris() const { return m_tris.data(); }
	int getVertCount() const { return m_verts.size() / 3; }
	int getTriCount() const { return m_tris.size() / 3; }
	const std::string& getFileName() const { return m_filename; }

private:
	std::string m_filename;
	std::vector<float> m_verts;
	std::vector<int> m_tris;
	std::vector<float> m_normals;
	float m_scale;
};

#endif // MESHLOADER_OBJ
