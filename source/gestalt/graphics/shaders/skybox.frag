#version 450

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_shader_16bit_storage : require
#include "per_frame_structs.glsl"

layout(location = 0) in vec3 TexCoords;
layout(location = 0) out vec4 FragColor;

struct ViewProjData {
	mat4 view;
    mat4 proj;
}; 

struct DirectionalLight {
	vec3 color;
	float intensity;
	vec3 direction;
	int viewProjIndex;
}; 

struct PointLight {
	vec3 color;
	float intensity;
	vec3 position;
	float range;
}; 

layout(set = 1, binding = 0) readonly buffer LightViewProj
{
	ViewProjData viewProjData[];
};

layout(set = 1, binding = 1) readonly buffer DirectionalLightBuffer
{
	DirectionalLight dirLight[];
};

layout(set = 1, binding = 2) readonly buffer PointLightBuffer
{
	PointLight pointLight[];
};

layout(set = 2, binding = 0) uniform sampler2D scene_lit;
layout(set = 2, binding = 1) uniform samplerCube texEnvMap;

layout( push_constant ) uniform constants
{
    //float earthRadius;
    //float atmosphereRadius;
    vec3 betaR; uint showEnviromentMap;
    vec3 betaA; float pad2;
    vec3 betaM; float pad3;
    //float Hr;
    //float Hm;
} params;

// based on https://www.scratchapixel.com/lessons/procedural-generation-virtual-worlds/simulating-sky/simulating-colors-of-the-sky.html

const float PI = 3.141592653589793;

float rayleighPhase(float cosTheta) {
    return 3.0 / (16.0 * PI) * (1.0 + cosTheta * cosTheta);
}

float miePhase(float cosTheta, float g) {
    float g2 = g * g;
    return 3.0 / (8.0 * PI) * ((1.0 - g2) * (1.0 + cosTheta * cosTheta)) / (2.0 + g2) / pow(1.0 + g2 - 2.0 * g * cosTheta, 1.5);
}

const float EARTH_RADIUS = 6360e3;     // Earth's radius in meters
const float ATMOSPHERE_RADIUS = 6460e3; // Atmosphere's top radius in meters
const float Hr = 3e3;                 // Rayleigh scale height (meters)
const float Hm = 1.2e3;               // Mie scale height (meters)

// Density function
float density(float height, float scaleHeight) {
    return exp(-height / scaleHeight);
}

// Calculate optical depth
float opticalDepth(float height, float scaleHeight, vec3 dir) {
    float H = max(0.0, height);
    return density(H, scaleHeight) * length(dir);
}

// Compute scattering contributions
vec3 computeScattering(vec3 dir, vec3 lightDir, vec3 betaR, vec3 betaM, vec3 sunIntensity) {
    float height = length(dir) - EARTH_RADIUS;
    float rayleighDepth = opticalDepth(height, Hr, dir);
    float mieDepth = opticalDepth(height, Hm, dir);

    float cosTheta = dot(dir, lightDir);
    vec3 rayleigh = betaR * rayleighPhase(cosTheta) * rayleighDepth;
    vec3 mie = betaM * miePhase(cosTheta, 0.76) * mieDepth;

    return sunIntensity * (rayleigh + mie);
}

void main() {

    vec3 dir = normalize(TexCoords);

    if (params.showEnviromentMap == 0) {

        vec3 lightDir = normalize(dirLight[0].direction);
        vec3 sunIntensity = dirLight[0].color * dirLight[0].intensity * 100000.0;

        vec3 scattering = computeScattering(dir, lightDir, params.betaR, params.betaM, sunIntensity);

        // Absorption
        scattering -= params.betaA * 0.002;

        FragColor = vec4(scattering, 1.0);

    } else {

        // Sample the environment texture using the normalized direction
        vec4 envColor = texture(texEnvMap, dir);

        FragColor = envColor;
    }
}
