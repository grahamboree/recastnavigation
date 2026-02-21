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

#ifndef DEBUGDRAW_H
#define DEBUGDRAW_H

// Some math headers don't have PI defined.
static const float DU_PI = 3.14159265f;

enum duDebugDrawPrimitives
{
	DU_DRAW_POINTS,
	DU_DRAW_LINES,
	DU_DRAW_TRIS,
	DU_DRAW_QUADS
};

/// Abstract debug draw interface.
struct duDebugDraw
{
	virtual ~duDebugDraw() = 0;

	virtual void depthMask(bool state) = 0;

	virtual void texture(bool state) = 0;

	/// Begin drawing primitives.
	///  @param prim [in] primitive type to draw, one of rcDebugDrawPrimitives.
	///  @param size [in] size of a primitive, applies to point size and line width only.
	virtual void begin(duDebugDrawPrimitives prim, float size = 1.0f) = 0;

	/// Submit a vertex
	///  @param pos [in] position of the verts.
	///  @param color [in] color of the verts.
	virtual void vertex(const float* pos, unsigned int color) = 0;

	/// Submit a vertex
	///  @param x,y,z [in] position of the verts.
	///  @param color [in] color of the verts.
	virtual void vertex(float x, float y, float z, unsigned int color) = 0;

	/// Submit a vertex
	///  @param pos [in] position of the verts.
	///  @param color [in] color of the verts.
	///  @param uv [in] the uv coordinates of the verts.
	virtual void vertex(const float* pos, unsigned int color, const float* uv) = 0;

	/// Submit a vertex
	///  @param x,y,z [in] position of the verts.
	///  @param color [in] color of the verts.
	///  @param u,v [in] the uv coordinates of the verts.
	virtual void vertex(float x, float y, float z, unsigned int color, float u, float v) = 0;

	/// End drawing primitives.
	virtual void end() = 0;

	/// Compute a color for given area.
	virtual unsigned int areaToCol(unsigned int area);
};

inline unsigned int duRGBA(int r, int g, int b, int a)
{
	return ((unsigned int)r) | ((unsigned int)g << 8) | ((unsigned int)b << 16) | ((unsigned int)a << 24);
}

inline unsigned int duRGBAf(float r, float g, float b, float a)
{
	return duRGBA(
		(unsigned char)(r * 255.0f),
		(unsigned char)(g * 255.0f),
		(unsigned char)(b * 255.0f),
		(unsigned char)(a * 255.0f));
}

unsigned int duIntToCol(int i, int a);
void duIntToCol(int i, float* color);

inline unsigned int duMultCol(const unsigned int color, const unsigned int d)
{
	const unsigned int r = color & 0xff;
	const unsigned int g = (color >> 8) & 0xff;
	const unsigned int b = (color >> 16) & 0xff;
	const unsigned int a = (color >> 24) & 0xff;
	return duRGBA((r * d) >> 8, (g * d) >> 8, (b * d) >> 8, a);
}

inline unsigned int duDarkenCol(unsigned int color)
{
	return ((color >> 1) & 0x007f7f7f) | (color & 0xff000000);
}

inline unsigned int duLerpCol(unsigned int ca, unsigned int cb, unsigned int u)
{
	const unsigned int ra = ca & 0xff;
	const unsigned int ga = (ca >> 8) & 0xff;
	const unsigned int ba = (ca >> 16) & 0xff;
	const unsigned int aa = (ca >> 24) & 0xff;
	const unsigned int rb = cb & 0xff;
	const unsigned int gb = (cb >> 8) & 0xff;
	const unsigned int bb = (cb >> 16) & 0xff;
	const unsigned int ab = (cb >> 24) & 0xff;

	unsigned int r = (ra * (255 - u) + rb * u) / 255;
	unsigned int g = (ga * (255 - u) + gb * u) / 255;
	unsigned int b = (ba * (255 - u) + bb * u) / 255;
	unsigned int a = (aa * (255 - u) + ab * u) / 255;
	return duRGBA(r, g, b, a);
}

inline unsigned int duTransCol(unsigned int c, unsigned int a)
{
	return (a << 24) | (c & 0x00ffffff);
}

void duCalcBoxColors(unsigned int* colors, unsigned int colTop, unsigned int colSide);

void duDebugDrawArc(duDebugDraw* dd, float x0, float y0, float z0, float x1, float y1, float z1, float arcHeight, float arrowSize0, float arrowSize1, unsigned int color, float lineWidth);
void duDebugDrawArrow(duDebugDraw* dd, float x0, float y0, float z0, float x1, float y1, float z1, float arrowSize0, float arrowSize1, unsigned int color, float lineWidth);
void duDebugDrawBox(duDebugDraw* dd, float minx, float miny, float minz, float maxx, float maxy, float maxz, const unsigned int* cornerColors);
void duDebugDrawBoxWire(duDebugDraw* dd, float minx, float miny, float minz, float maxx, float maxy, float maxz, unsigned int color, float lineWidth);
void duDebugDrawCircle(duDebugDraw* dd, float x, float y, float z, float radius, unsigned int color, float lineWidth);
void duDebugDrawCross(duDebugDraw* dd, float x, float y, float z, float size, unsigned int color, float lineWidth);
void duDebugDrawCylinder(duDebugDraw* dd, float minx, float miny, float minz, float maxx, float maxy, float maxz, unsigned int color);
void duDebugDrawCylinderWire(duDebugDraw* dd, float minx, float miny, float minz, float maxx, float maxy, float maxz, unsigned int color, float lineWidth);
void duDebugDrawGridXZ(duDebugDraw* dd, float x, float y, float z, int width, int height, float cellSize, unsigned int color, float lineWidth);

// Versions without begin/end, can be used to draw multiple primitives in one go.
void duAppendArc(duDebugDraw* dd, float x0, float y0, float z0, float x1, float y1, float z1, float h, float arrowSize0, float arrowSize1, unsigned int color);
void duAppendArrow(duDebugDraw* dd, float x0, float y0, float z0, float x1, float y1, float z1, float arrowSize0, float arrowSize1, unsigned int color);
void duAppendBox(duDebugDraw* dd, float minx, float miny, float minz, float maxx, float maxy, float maxz, const unsigned int* cornerColors);
void duAppendBoxWire(duDebugDraw* dd, float minx, float miny, float minz, float maxx, float maxy, float maxz, unsigned int color);
void duAppendBoxPoints(duDebugDraw* dd, float minx, float miny, float minz, float maxx, float maxy, float maxz, unsigned int color);
void duAppendCircle(duDebugDraw* dd, float x, float y, float z, float r, unsigned int color);
void duAppendCross(duDebugDraw* dd, float x, float y, float z, float size, unsigned int color);
void duAppendCylinder(duDebugDraw* dd, float minx, float miny, float minz, float maxx, float maxy, float maxz, unsigned int color);
void duAppendCylinderWire(duDebugDraw* dd, float minx, float miny, float minz, float maxx, float maxy, float maxz, unsigned int color);

class duDisplayList : public duDebugDraw
{
	float* m_pos;
	unsigned int* m_color;
	int m_size;
	int m_cap;

	duDebugDrawPrimitives m_prim;
	float m_primSize;
	bool m_depthMask;

	void resize(int cap);

public:
	duDisplayList(int cap = 512);
	virtual ~duDisplayList();
	virtual void depthMask(bool state);
	virtual void begin(duDebugDrawPrimitives prim, float size = 1.0f);
	virtual void vertex(float x, float y, float z, unsigned int color);
	virtual void vertex(const float* pos, unsigned int color);
	virtual void end();
	void clear();
	void draw(duDebugDraw* dd);

private:
	// Explicitly disabled copy constructor and copy assignment operator.
	duDisplayList(const duDisplayList&);
	duDisplayList& operator=(const duDisplayList&);
};

#endif  // DEBUGDRAW_H
