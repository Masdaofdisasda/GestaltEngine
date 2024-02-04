layout(set = 0, binding = 0) uniform  SceneData{   

	mat4 view;
	mat4 proj;
	mat4 viewproj;
	vec4 ambientColor;
	vec4 sunlightDirection; //w for sun power
	vec4 sunlightColor;
} sceneData;

layout(set = 1, binding = 1) uniform sampler2D textures[];

layout(set = 2, binding = 2) uniform GLTFMaterialData{   

	vec4 colorFactors;
	vec2 metal_rough_factors;
	
} materialData[];

layout(set = 3, binding = 3) uniform samplerCube texEnvMap;
layout(set = 3, binding = 4) uniform samplerCube texEnvMapIrradiance;