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

vec3 uncharted2_tonemap_partial(vec3 x)
{
    float A = 0.15f;
    float B = 0.50f;
    float C = 0.10f;
    float D = 0.20f;
    float E = 0.02f;
    float F = 0.30f;
    return ((x*(A*x+C*B)+D*E)/(x*(A*x+B)+D*F))-E/F;
}

vec3 Uncharted2Filmic(vec3 v)
{
    float exposure_bias = 2.0f;
    vec3 curr = uncharted2_tonemap_partial(v * exposure_bias);

    vec3 W = vec3(11.2f);
    vec3 white_scale = vec3(1.0f) / uncharted2_tonemap_partial(W);
    return curr * white_scale;
}


const mat3 acesInputMatrix = mat3(
    vec3(0.59719, 0.07600, 0.02840), // First column becomes first row
    vec3(0.35458, 0.90834, 0.13383), // Second column becomes second row
    vec3(0.04823, 0.01566, 0.83777)  // Third column becomes third row
);

const mat3 acesOutputMatrix =  mat3(
    vec3(1.60475, -0.10208, -0.00327), // First column becomes first row
    vec3(-0.53108, 1.10813, -0.07276), // Second column becomes second row
    vec3(-0.07367, -0.00605, 1.07602)  // Third column becomes third row
);

vec3 rtt_and_odt_fit(vec3 v) {
    vec3 a = v * (v + 0.0245786) - 0.000090537;
    vec3 b = v * (0.983729 * v + 0.4329510) + 0.238081;
    return a / b;
}

vec3 ACES_fitted(vec3 v) {
    v = acesInputMatrix * v;
    v = rtt_and_odt_fit(v);
    return acesOutputMatrix * v;
}

vec3 ACESAproximation(vec3 x) {
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
        sceneColor = Uncharted2Filmic(sceneColor);
    } else if (params.toneMappingOption == 2) {
        sceneColor = ACESAproximation(sceneColor); 
    } else if (params.toneMappingOption == 3) {
        sceneColor = ACES_fitted(sceneColor); 
    }

    // Color grading
    sceneColor = ApplyLiftGammaGain(sceneColor, params.lift.rgb, params.gamma.rgb, params.gain.rgb);

    // Combine scene color with bloom
    vec3 finalColor = sceneColor + bloomColor;

    outColor = vec4(finalColor, 1.0);
}
