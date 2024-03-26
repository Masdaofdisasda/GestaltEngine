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
} params;

vec4 umajorLineColor = vec4(1, 1, 1, 1);
vec4 uminorLineColor = vec4(0.7, 0.7, 0.7, 1);
vec4 ubaseColor = vec4(0.01, 0.01, 0.01, 1);
vec4 ucenterColor = vec4(1.0, 1.0, 1.0, 1);

vec4 uxAxisColor = vec4(1, 0, 0, 1);
vec4 uyAxisColor = vec4(0, 1, 0, 1);
vec4 uzAxisColor = vec4(0, 0, 1, 1);

vec4 uxAxisDashColor = vec4(0, 0, 0, 1);
vec4 uyAxisDashColor = vec4(0, 0, 0, 1);
vec4 uzAxisDashColor = vec4(0, 0, 0, 1);

vec3 sRGBToLinear(vec3 color) {
    return mix(color / 12.92, pow((color + vec3(0.055)) / vec3(1.055), vec3(2.4)), step(vec3(0.04045), color));
}

// based on https://bgolus.medium.com/the-best-darn-grid-shader-yet-727f9278b9d8
void main() {

    // Calculating derivative for anti-aliasing
    vec4 uvDDXY = vec4(dFdx(uv.xy), dFdy(uv.xy));
    vec2 uvDeriv = vec2(length(uvDDXY.xz), length(uvDDXY.yw));

    // Adjusting line width based on axis and major line width
    float axisLineWidth  = max(params.majorLineWidth, params.axisLineWidth);
    vec2 axisDrawWidth = max(vec2(axisLineWidth), uvDeriv);
    vec2 axisLineAA = uvDeriv * 1.5;
    vec2 axisLines = smoothstep(axisDrawWidth + axisLineAA, axisDrawWidth - axisLineAA, abs(uv.zw * 2.0));
    axisLines *= clamp(axisLineWidth / axisDrawWidth, vec2(0.0), vec2(1.0));

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

    // Minor grid lines
    float minorLineWidth = min(params.minorLineWidth, params.majorLineWidth);
    bool minorInvertLine = minorLineWidth > 0.5;
    float minorTargetWidth = minorInvertLine ? 1.0 - minorLineWidth : minorLineWidth;
    vec2 minorDrawWidth = clamp(vec2(minorTargetWidth), uvDeriv, vec2(0.5));
    vec2 minorLineAA = uvDeriv * 1.5;
    vec2 minorGridUV = abs(fract(uv.xy) * 2.0 - 1.0);
    minorGridUV = minorInvertLine ? minorGridUV : 1.0 - minorGridUV;

    vec2 minorMajorOffset = (1.0 - clamp((1.0 - abs(fract(uv.zw / div) * 2.0 - 1.0)) * div, vec2(0.0), vec2(1.0))) * 2.0;
    minorGridUV += minorMajorOffset; // adjust UVs so major division lines are skipped
    vec2 minorGrid = smoothstep(minorDrawWidth + minorLineAA, minorDrawWidth - minorLineAA, minorGridUV);
    minorGrid *= clamp(minorTargetWidth / minorDrawWidth, vec2(0.0), vec2(1.0));
    minorGrid = clamp(minorGrid - axisLines, vec2(0.0), vec2(1.0)); // hack
    minorGrid = mix(minorGrid, vec2(minorTargetWidth), clamp(uvDeriv * 2.0 - 1.0, vec2(0.0), vec2(1.0)));
    minorGrid = minorInvertLine ? vec2(1.0) - minorGrid : minorGrid;
    minorGrid = step(vec2(0.5), abs(uv.zw)) * minorGrid;

    float minorGridF = mix(minorGrid.x, 1.0, minorGrid.y);
    float majorGridF = mix(majorGrid.x, 1.0, majorGrid.y);

    vec2 axisDashUV = abs(fract((uv.zw + axisLineWidth * 0.5) * params.axisDashScale) * 2.0 - 1.0) - 0.5;
    vec2 axisDashDeriv = uvDeriv * params.axisDashScale * 1.5;
    vec2 axisDash = smoothstep(-axisDashDeriv, axisDashDeriv, axisDashUV);
    axisDash = mix(axisDash, vec2(1.0), step(vec2(0.0), -uv.zw));


    // colors
    vec4 xAxisColor = vec4(sRGBToLinear(uxAxisColor.rgb), uxAxisColor.a);
    vec4 yAxisColor = vec4(sRGBToLinear(uyAxisColor.rgb), uyAxisColor.a);
    vec4 zAxisColor = vec4(sRGBToLinear(uzAxisColor.rgb), uzAxisColor.a);

    vec4 xAxisDashColor = vec4(sRGBToLinear(uxAxisDashColor.rgb), uxAxisDashColor.a);
    vec4 yAxisDashColor = vec4(sRGBToLinear(uyAxisDashColor.rgb), uyAxisDashColor.a);
    vec4 zAxisDashColor = vec4(sRGBToLinear(uzAxisDashColor.rgb), uzAxisDashColor.a);

    vec4 centerColor = vec4(sRGBToLinear(ucenterColor.rgb), ucenterColor.a);
    vec4 majorLineColor = vec4(sRGBToLinear(umajorLineColor.rgb), umajorLineColor.a);
    vec4 minorLineColor = vec4(sRGBToLinear(uminorLineColor.rgb), uminorLineColor.a);
    vec4 baseColor = vec4(sRGBToLinear(ubaseColor.rgb), ubaseColor.a);

    // use Y as up
    vec4 aAxisColor = xAxisColor;
    vec4 bAxisColor = zAxisColor;
    vec4 aAxisDashColor = xAxisDashColor;
    vec4 bAxisDashColor = zAxisDashColor;

    aAxisColor = mix(aAxisDashColor, aAxisColor, axisDash.y);
    bAxisColor = mix(bAxisDashColor, bAxisColor, axisDash.x);
    aAxisColor = mix(aAxisColor, centerColor, axisLines.y);

    vec4 axisLines4 = mix(bAxisColor * axisLines.y, aAxisColor, axisLines.x);

    vec4 color = mix(baseColor, minorLineColor, minorGridF * minorLineColor.a);
    color = mix(color, majorLineColor, majorGridF * majorLineColor.a);
    color = color * (1.0 - axisLines4.a) + axisLines4;

    color.rgb = pow(color.rgb, vec3(1.0 / 2.2));

    out_FragColor = vec4(color.rgb, 1.0);
}
