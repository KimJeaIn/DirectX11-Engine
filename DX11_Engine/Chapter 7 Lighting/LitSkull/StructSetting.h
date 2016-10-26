#include "d3dUtil.h"

#pragma comment (lib, "dwrite.lib")
#pragma comment (lib, "dinput8.lib")
#pragma comment (lib, "dxguid.lib")

// 추가 종속성과 같음
#pragma comment(lib, "lua5.1.lib")
#pragma comment(lib, "lua51.lib")

#include <dwrite.h>
#include <dinput.h>

#include <map>
#include <list>

extern "C"{
#include "lua\src\lua.h"
#include "lua\src\lualib.h"
#include "lua\src\lauxlib.h"
}

struct Vertex	//Overloaded Vertex Structure
{
	Vertex(){}
	Vertex(float x, float y, float z,
		float u, float v,
		float nx, float ny, float nz)
		: pos(x, y, z), texCoord(u, v), normal(nx, ny, nz){}

	XMFLOAT3 pos;
	XMFLOAT2 texCoord;
	XMFLOAT3 normal;
	XMFLOAT3 tangent;
	XMFLOAT3 biTangent;

	int StartWeight;
	int WeightCount;
};

struct GridVertex	//Overloaded Vertex Structure
{
	GridVertex(){}
	GridVertex(float x, float y, float z,
		float cr, float cg, float cb, float ca)
		: pos(x, y, z), color(cr, cg, cb, ca){}

	XMFLOAT3 pos;
	XMFLOAT4 color;
};

struct SurfaceMaterial
{
	std::wstring matName;
	XMFLOAT4 difColor;
	int texArrayIndex;
	///////////////**************new**************////////////////////
	int normMapTexArrayIndex;
	bool hasNormMap;
	///////////////**************new**************////////////////////
	bool hasTexture;
	bool transparent;
};

struct Joint
{
	std::wstring name;
	int parentID;

	XMFLOAT3 pos;
	XMFLOAT4 orientation;
};

struct Weight
{
	int jointID;
	float bias;
	XMFLOAT3 pos;
	///////////////**************new**************////////////////////
	XMFLOAT3 normal;
	///////////////**************new**************////////////////////
};

///////////////**************new**************////////////////////
struct BoundingBox
{
	XMFLOAT3 min;
	XMFLOAT3 max;
};

struct FrameData
{
	int frameID;
	std::vector<float> frameData;
};
struct AnimJointInfo
{
	std::wstring name;
	int parentID;

	int flags;
	int startIndex;
};

struct ModelAnimation
{
	int numFrames;
	int numJoints;
	int frameRate;
	int numAnimatedComponents;

	float frameTime;
	float totalAnimTime;
	float currAnimTime;

	std::vector<AnimJointInfo> jointInfo;
	std::vector<BoundingBox> frameBounds;
	std::vector<Joint>	baseFrameJoints;
	std::vector<FrameData>	frameData;
	std::vector<std::vector<Joint>> frameSkeleton;

	std::wstring AniName;
};
///////////////**************new**************////////////////////

struct ModelSubset
{
	int texArrayIndex;
	int numTriangles;

	std::vector<Vertex> vertices;
	std::vector<DWORD> indices;
	std::vector<Weight> weights;

	std::vector<XMFLOAT3> positions;

	ID3D11Buffer* vertBuff;
	ID3D11Buffer* indexBuff;
};

struct Model3D
{
	int numSubsets;
	int numJoints;

	std::vector<Joint> joints;
	std::vector<ModelSubset> subsets;
	///////////////**************new**************////////////////////
	std::vector<ModelAnimation> animations;
	///////////////**************new**************////////////////////
};

struct BoundingSphere
{
	BoundingSphere() : Center(0.0f, 0.0f, 0.0f), Radius(0.0f) {}
	XMFLOAT3 Center;
	float Radius;
};