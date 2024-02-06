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
} params;

layout(binding = 10) uniform sampler2D texScene;
layout(binding = 11) uniform sampler2D texBloom;
//layout(binding = 12) uniform sampler2D texLuminance; TODO

// Extended Reinhard tone mapping operator
vec3 Reinhard2(vec3 x)
{
	return (x * (1.0 + x / (params.maxWhite * params.maxWhite))) / (1.0 + x);
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
    sceneColor = Reinhard2(sceneColor);

    // Color grading
    sceneColor = ApplyLiftGammaGain(sceneColor, params.lift.rgb, params.gamma.rgb, params.gain.rgb);

    // Combine scene color with bloom
    vec3 finalColor = sceneColor + bloomColor;

    outColor = vec4(finalColor, 1.0);
}
