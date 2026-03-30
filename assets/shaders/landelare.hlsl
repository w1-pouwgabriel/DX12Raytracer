struct Payload
{
    float3 color;
    bool allowReflection;
    bool missed;
};

RaytracingAccelerationStructure scene : register(t0);
RWTexture2D<float4> output : register(u0);

static const float3 camera = float3(0, 1.5, -7);
static const float3 light = float3(0, 200, 0);
static const float3 skyTop = float3(0.24, 0.44, 0.72);
static const float3 skyBottom = float3(0.75, 0.86, 0.93);

[shader("raygeneration")]
void RayGeneration()
{
    uint2 idx = DispatchRaysIndex().xy;
    float2 size = DispatchRaysDimensions().xy;

    float2 uv = idx / size;
    float3 target = float3((uv.x * 2 - 1) * 1.8 * (size.x / size.y),
                           (1 - uv.y) * 4 - 2 + camera.y,
                           0);
}