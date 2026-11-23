#version 120

#include "lib/luminance/constants.glsl"

varying vec2 uv;
uniform sampler2D sceneTex;
uniform bool colorExposure;

void main()
{
    vec3 color = texture2D(sceneTex, uv).rgb;
    float lum = dot(color, vec3(0.2126, 0.7152, 0.0722));
    lum = max(lum, epsilon);

    if (colorExposure)
        gl_FragColor.gba = pow(max(color, vec3(0.0)), vec3(2.2));
    gl_FragColor.r = clamp((log2(lum) - minLog) * invLogLumRange, 0.0, 1.0);
}
