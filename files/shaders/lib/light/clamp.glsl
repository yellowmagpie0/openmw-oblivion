void clampLighting(inout vec3 lighting)
{
#if @clamp && @oblivionLighting
    // Keep the weather or lamp hue when its brightest channel reaches the diffuse material limit.
    lighting = max(lighting, 0.0);
    lighting /= max(1.0, max(lighting.r, max(lighting.g, lighting.b)));
#elif @clamp
    lighting = clamp(lighting, vec3(0.0), vec3(1.0));
#else
    lighting = max(lighting, 0.0);
#endif
}
