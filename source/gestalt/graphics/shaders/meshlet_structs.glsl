// Workgroup size for task shader; each task shader thread produces up to one meshlet
#define TASK_WGSIZE 64

// Workgroup size for mesh shader; mesh shader workgroup processes the entire meshlet in parallel
#define MESH_WGSIZE 64

// Should we do meshlet frustum, occlusion and backface culling in task shader?
#define TASK_CULL 1

// Should we do triangle frustum and backface culling in mesh shader?
#define MESH_CULL 1

// Maximum number of vertices and triangles in a meshlet
#define MESH_MAXVTX 64
#define MESH_MAXTRI 64

// Maximum number of total task shader workgroups; 4M workgroups ~= 256M meshlets ~= 16B triangles if TASK_WGSIZE=64 and MESH_MAXTRI=64
#define TASK_WGLIMIT (1 << 22)

struct MeshTaskPayload
{
	uint drawId;
	uint meshletIndices[TASK_WGSIZE];
};

struct VertexPosition {
	vec3 position;
	float pad;
}; 

struct VertexData {
    uint8_t nx, ny, nz, nw; // normal
    uint8_t tx, ty, tz, tw; // tangent
    float16_t tu, tv;       // tex coords
	float pad;
};

struct Meshlet
{
    vec3 center;
    float radius;
    int8_t cone_axis[3];
    int8_t cone_cutoff;

    uint vertexOffset;
    uint indexOffset;
    uint mesh_index;
    uint8_t vertexCount;
    uint8_t triangleCount;
};

struct MeshDraw
{
	vec3 position;
	float scale;
	vec4 orientation;

	vec3 boundMin;
	uint meshletOffset;
	vec3 boundMax;
	uint meshletCount;

	uint vertexCount;
	uint indexCount;
	uint firstIndex;
	uint vertexOffset;

	uint materialIndex;
	uint pad[3];
};

struct MeshTaskCommand
{
	uint meshDrawId;  // references a MeshDraw needed to draw one mesh
	uint taskOffset; // index into the global meshlet array
	uint taskCount; // [0...63], number of meshlets to process, might be less for the last task
	uint pad;
};