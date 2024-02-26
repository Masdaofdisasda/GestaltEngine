/**/
#version 460

layout(location = 0) in  vec2 texCoord;
layout(location = 0) out vec4 outColor;

layout(binding = 10) uniform sampler2D texColor;
layout(binding = 11) uniform sampler2D texRotationPattern;

layout( push_constant ) uniform constants
{
    float intensity;
    float attenuation;
	int streakSamples;
	int numStreaks;
} params;

void main()
{
	const float texOffset = 1.0 / 512.0;

	int iteration = 1;

	vec4 color = vec4(texture( texColor, texCoord ).rgb, 1.0f);
	float b = pow( float(params.streakSamples), float(iteration) );
	vec2 streakDirection = texture( texRotationPattern, texCoord ).xy;

	for ( int k = 0; k < params.numStreaks; k++ )
	{
		vec4 cOut = vec4( 0.0 );

		for ( int s = 0; s < params.streakSamples; s++)
		{
			float weight = pow(params.attenuation, b * float(s));

			// Streak direction is a 2D vector in image space
			vec2 texCoordSample = texCoord + (streakDirection * b * float(s) * texOffset);

			// Scale and accumulate
			cOut += clamp(weight, 0.0, 1.0) * texture( texColor, texCoordSample ) / 4.0;
		}

		color = max( color, cOut );

		// rotate streak 90 degrees
		streakDirection = vec2( -streakDirection.y, streakDirection.x );
	}

	outColor = vec4(color.rgb * params.intensity, 1.0);
}
