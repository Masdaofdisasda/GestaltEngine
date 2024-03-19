//
#version 460 core

layout (location=0) in vec4 uv;
layout (location=0) out vec4 out_FragColor;


layout( push_constant ) uniform constants
{
    float majorLineWidth;
    float minorLineWidth;
    float axisLineWidth;
    float axisDashScale;
    float majorGridDiv;
    float minorGridMult;
} params;

vec4 majorLineColor = vec4(1, 1, 1, 1);
vec4 minorLineColor = vec4(0.6, 0.6, 0.6, 1);
vec4 baseColor = vec4(0.1, 0.1, 0.1, 1);
vec4 xAxisColor = vec4(1, 0, 0, 1);
vec4 yAxisColor = vec4(0, 1, 0, 1);
vec4 zAxisColor = vec4(0, 0, 1, 1);

void main() {

    // Calculating derivative for anti-aliasing
    vec4 uvDDXY = vec4(dFdx(uv.xy), dFdy(uv.xy));
    vec2 uvDeriv = vec2(length(uvDDXY.xz), length(uvDDXY.yw));

    // Adjusting line width based on axis and major line width
    float axisLineWidth  = max(params.majorLineWidth, params.axisLineWidth);
    vec2 axisDrawWidth = max(vec2(axisLineWidth), uvDeriv);
    vec2 axisLineAA = uvDeriv * 1.5;
    vec2 axisLines = smoothstep(axisDrawWidth + axisLineAA, axisDrawWidth - axisLineAA, abs(uv.zw * 2.0 - 1.0));
    axisLines *= clamp(axisLineWidth / axisDrawWidth.x, 0.0, 1.0);

    // Major grid lines
    float div = max(2.0, params.majorGridDiv);
    vec2 majorUVDeriv = uvDeriv / div;
    float majorLineWidth = params.majorLineWidth / div;
    vec2 majorDrawWidth = clamp(vec2(majorLineWidth), majorUVDeriv, vec2(0.5));
    vec2 majorLineAA = majorUVDeriv * 1.5;
    vec2 majorGridUV = 1.0 - abs(fract(uv.xy / div) * 2.0 - 1.0);
    vec2 majorAxisOffset =  (1.0 - clamp(abs(uv.zw / div * 2.0), vec2(0.0), vec2(1.0))) * 2.0;
    majorGridUV += majorAxisOffset; // adjust UVs so center axis line is skipped
    vec2 majorGrid = smoothstep(majorDrawWidth + majorLineAA, majorDrawWidth - majorLineAA, majorGridUV);
    majorGrid *= clamp(majorLineWidth / majorDrawWidth, vec2(0.0), vec2(1.0));
    majorGrid = clamp(majorGrid - axisLines, vec2(0.0), vec2(1.0)); // hack
    majorGrid = mix(majorGrid, vec2(majorLineWidth), clamp(majorUVDeriv * 2.0 - 1.0, vec2(0.0), vec2(1.0)));

    // Combining the grid lines
    vec3 color = mix(vec3(0.1, 0.1, 0.1), vec3(1.0), max(majorGrid.x, majorGrid.y));
    out_FragColor = vec4(color, 1.0);

    /*
    // Minor Grid Lines
    float minorDiv = params.majorGridDiv * params.minorGridMult;
    vec2 minorUVDeriv = uvDeriv / minorDiv;
    vec2 minorGridUV = abs(fract(uv * minorDiv) * 2.0 - 1.0);
    vec2 minorDrawWidth = clamp(vec2(params.minorLineWidth), minorUVDeriv, vec2(0.5));
    vec2 minorLineAA = minorUVDeriv * 1.5;
    vec2 minorLines = smoothstep(minorDrawWidth + minorLineAA, minorDrawWidth - minorLineAA, minorGridUV);
    float minorGrid = max(minorLines.x, minorLines.y);
    
    // Axis Dash Effect
    vec2 axisDashUV = abs(fract((uv + params.axisLineWidth * 0.5) * params.axisDashScale) * 2.0 - 1.0) - 0.5;
    vec2 axisDash = smoothstep(-minorLineAA, minorLineAA, axisDashUV);
    axisDash = length(uv) < 0.0 ? axisDash : vec2(1.0);
    
    // Axis Lines
    vec3 axisLinesColor = mix(xAxisColor.rgb, yAxisColor.rgb, step(abs(uv.x), params.axisLineWidth * uvDeriv.x));
    axisLinesColor = mix(axisLinesColor, baseColor.rgb, step(abs(uv.y), params.axisLineWidth * uvDeriv.y));
    
    // Combine the grid lines and colors
    vec3 color = mix(baseColor.rgb, minorLineColor.rgb, minorGrid * minorLineColor.a);
    color = mix(color, axisLinesColor, min(axisDash.x, axisDash.y));
    
    out_FragColor = vec4(color, 1.0);*/

    
    /*
    // Axis coloring (assuming the shader applies to a specific plane)
    if (abs(uv.x) < params.axisLineWidth * gridFade.x) {
        color = mix(color, xAxisColor, smoothstep(0.0, gridFade.x * params.axisLineWidth, 0.5 - abs(uv.x)));
    }
    if (abs(uv.y) < params.axisLineWidth * gridFade.y) {
        color = mix(color, zAxisColor, smoothstep(0.0, gridFade.y * params.axisLineWidth, 0.5 - abs(uv.y)));
    }

    out_FragColor = color;*/
}
