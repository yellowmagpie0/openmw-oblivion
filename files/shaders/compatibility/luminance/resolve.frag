#version 120

#include "lib/luminance/constants.glsl"

varying vec2 uv;
uniform sampler2D luminanceSceneTex;
uniform sampler2D prevLuminanceSceneTex;

uniform float osg_DeltaFrameTime;
uniform vec4 exposure;

void main()
{
    vec4 previous = texture2D(prevLuminanceSceneTex, vec2(0.5));
    vec4 current = texture2D(luminanceSceneTex, vec2(0.5));
    float prevLum = previous.r;
    float currLum = current.r;

    // TES4 tracks the average scene RGB independently of OpenMW's log luminance in red.
    if (exposure.x > 0.0)
    {
        float retention = pow(clamp(exposure.z, 0.0001, 1.0), max(0.0, osg_DeltaFrameTime));
        vec3 adapted = mix(current.gba, previous.gba, retention);
        gl_FragColor.gba = adapted * min(1.0, max(exposure.y, 0.0) / max(0.01, length(adapted)));
    }

    float avgLum = exp2((currLum * logLumRange) + minLog);
    gl_FragColor.r = prevLum + (avgLum - prevLum) * (1.0 - exp(-osg_DeltaFrameTime * hdrExposureTime));
}
