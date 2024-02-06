/**/
#version 460

layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 outColor;

layout( push_constant ) uniform constants
{
    float exposure;
	float maxWhite; 
	float bloomStrength; 
	float adaptationSpeed;
	vec4 lift; 
	vec4 gamma; 
	vec4 gain;
	bool showBloom;
    int toneMappingOption;
} params;

layout(binding = 10) uniform sampler2D texScene;
layout(binding = 11) uniform sampler2D texBloom;
//layout(binding = 12) uniform sampler2D texLuminance; TODO

// Extended Reinhard tone mapping operator
vec3 Reinhard2(vec3 x)
{
	return (x * (1.0 + x / (params.maxWhite * params.maxWhite))) / (1.0 + x);
}

vec3 Uncharted2Tonemap(vec3 x) {
    float A = 0.15;
    float B = 0.50;
    float C = 0.10;
    float D = 0.20;
    float E = 0.02;
    float F = 0.30;
    return ((x*(A*x+C*B)+D*E)/(x*(A*x+B)+D*F))-E/F;
}

vec3 RRTAndODTFit(vec3 v) {
    vec3 a = v * (v + 0.0245786) - 0.000090537;
    vec3 b = v * (0.983729 * v + 0.4329510) + 0.238081;
    return a / b;
}

vec3 ACESFilm(vec3 x) {
    float a = 2.51;
    float b = 0.03;
    float c = 2.43;
    float d = 0.59;
    float e = 0.14;
    return clamp((x*(a*x+b))/(x*(c*x+d)+e), 0.0, 1.0);
}


vec3 ApplyLiftGammaGain(vec3 color, vec3 lift, vec3 gamma, vec3 gain) {
    // Apply lift (shadows)
    color += lift;

    // Apply gamma (midtones)
    color = pow(color, 1.0 / gamma);

    // Apply gain (highlights)
    color *= gain;

    return color;
}

void main() {
    vec3 sceneColor = texture(texScene, uv).rgb;
    vec3 bloomColor = texture(texBloom, uv).rgb * params.bloomStrength;

    if (params.showBloom) {
        outColor = vec4(bloomColor, 1.0);
        return;
    }

    // Apply exposure to scene color
    sceneColor *= params.exposure;

    // Tone mapping
    if (params.toneMappingOption == 0) {
        sceneColor = Reinhard2(sceneColor);
    } else if (params.toneMappingOption == 1) {
        sceneColor = Uncharted2Tonemap(sceneColor);
    } else if (params.toneMappingOption == 2) {
        sceneColor = ACESFilm(sceneColor); // Or RRTAndODTFit for a different look
    }

    // Color grading
    sceneColor = ApplyLiftGammaGain(sceneColor, params.lift.rgb, params.gamma.rgb, params.gain.rgb);

    // Combine scene color with bloom
    vec3 finalColor = sceneColor + bloomColor;

    outColor = vec4(finalColor, 1.0);
}
