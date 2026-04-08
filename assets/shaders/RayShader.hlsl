RWTexture2D<float4> gOutput : register(u0);
RaytracingAccelerationStructure scene : register(t0); // Add this!

struct Payload
{
    float4 color;
};

[shader("raygeneration")]
void RayGeneration()
{
    uint2 idx = DispatchRaysIndex().xy;
    float2 dims = float2(DispatchRaysDimensions().xy);
    float2 uv = idx / dims;
    
    // Shoot a ray into the scene!
    RayDesc ray;
    ray.Origin = float3(0, 2, -5); // Camera position
    ray.Direction = normalize(float3((uv.x * 2 - 1), (1 - uv.y * 2), 1));
    ray.TMin = 0.001;
    ray.TMax = 1000;
    
    Payload payload;
    payload.color = float4(0, 0, 0, 1);
    
    TraceRay(scene, RAY_FLAG_NONE, 0xFF, 0, 1, 0, ray, payload);
    
    gOutput[idx] = payload.color;
}

[shader("miss")]
void Miss(inout Payload payload)
{
    payload.color = float4(0.5, 0.7, 1.0, 1); // Sky blue
}

[shader("closesthit")]
void ClosestHit(inout Payload payload, BuiltInTriangleIntersectionAttributes attribs)
{
    payload.color = float4(1, 0, 0, 1); // Red cube!
}