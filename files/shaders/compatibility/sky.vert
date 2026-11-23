#version 120

#include "lib/core/vertex.h.glsl"

#include "lib/sky/passes.glsl"

uniform int pass;
#if @oblivionSky
uniform mat4 osg_ViewMatrixInverse;
varying float skyElevation;
#endif

varying vec4 passColor;
varying vec2 diffuseMapUV;

void main()
{
    gl_Position = modelToClip(gl_Vertex);
    passColor = gl_Color;
#if @oblivionSky
    skyElevation = normalize(mat3(osg_ViewMatrixInverse) * modelToView(gl_Vertex).xyz).z;
#endif

    if (pass == PASS_CLOUDS)
        diffuseMapUV = (gl_TextureMatrix[0] * gl_MultiTexCoord0).xy;
    else
        diffuseMapUV = gl_MultiTexCoord0.xy;
}
