
#include "RaytracingHlslCompat.h"

float3 computeDiffuse(GltfShadeMaterial mat, float3 lightDir, float3 normal)
{
    // Lambertian
    float dotNL = max(dot(normal, lightDir), 0.0);
    return mat.pbrBaseColorFactor.xyz * dotNL;
}

float3 computeSpecular(GltfShadeMaterial mat, float3 viewDir, float3 lightDir, float3 normal)
{
    // Compute specular only if not in shadow
    const float kPi = 3.14159265;
    const float kShininess = 60.0;

    // Specular
    const float kEnergyConservation = (2.0 + kShininess) / (2.0 * kPi);
    float3        V = normalize(-viewDir);
    float3        R = reflect(-lightDir, normal);
    float       specular = kEnergyConservation * pow(max(dot(V, R), 0.0), kShininess);

    return float3(specular, specular, specular);
}
