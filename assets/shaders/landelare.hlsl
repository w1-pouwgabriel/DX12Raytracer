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

// -- Ray generation ------------------------------------------------------------

[shader("raygeneration")]
void RayGeneration()
{
    
    uint2 idx = DispatchRaysIndex().xy;
    float2 size = DispatchRaysDimensions().xy;
    float2 uv = idx / size;

// FORCE BRIGHT RED - ignore everything else
    output[idx] = float4(1, 0, 0, 1);  // Add this at the TOP
    return;  // Skip rest of shader

    float3 target = float3(
        (uv.x * 2 - 1) * 1.8 * (size.x / size.y),
        (1 - uv.y) * 4 - 2 + camera.y,
        0
    );

    RayDesc ray;
    ray.Origin = camera;
    ray.Direction = normalize(target - camera);
    ray.TMin = 0.001;
    ray.TMax = 1000;

    Payload payload;
    payload.color = float3(0, 0, 0);
    payload.allowReflection = true;
    payload.missed = false;

    TraceRay(scene, RAY_FLAG_NONE, 0xFF, 0, 1, 0, ray, payload);

    output[idx] = float4(payload.color, 1);
}

// -- Miss ----------------------------------------------------------------------

[shader("miss")]
void Miss(inout Payload payload)
{
    float slope = normalize(WorldRayDirection()).y;
    float t = saturate(slope * 5 + 0.5);
    payload.color = lerp(skyBottom, skyTop, t);
    payload.missed = true;
}

// -- Forward declarations ------------------------------------------------------

void HitCube(inout Payload payload, float2 uv);
void HitMirror(inout Payload payload, float2 uv);
void HitFloor(inout Payload payload, float2 uv);

// -- Closest hit dispatcher ----------------------------------------------------

[shader("closesthit")]
void ClosestHit(inout Payload payload,
                BuiltInTriangleIntersectionAttributes attribs)
{
    float2 uv = attribs.barycentrics;
    switch (InstanceID())
    {
        case 0:
            HitCube(payload, uv);
            break;
        case 1:
            HitMirror(payload, uv);
            break;
        case 2:
            HitFloor(payload, uv);
            break;
        default:
            payload.color = float3(1, 0, 1);
            break;
    }
}

// -- Hit shaders ---------------------------------------------------------------

void HitCube(inout Payload payload, float2 uv)
{
    uint tri = PrimitiveIndex() / 2; // 2 triangles per face → face index

    // Build face normal from face index (0-2 = negative axes, 3-5 = positive axes)
    // FIX: tri.xxx is invalid — uint scalars don't support swizzle.
    //      Use uint3(tri,tri,tri) instead.
    uint3 triVec = uint3(tri, tri, tri);
    float3 normal = float3(triVec % 3 == uint3(0, 1, 2)) * (tri < 3 ? -1.0 : 1.0);
    float3 worldNormal = normalize(mul(normal, (float3x3) ObjectToWorld4x3()));
    float3 color = abs(normal) / 3.0 + 0.5;

    // Dark edge lines at triangle seams
    if (uv.x < 0.03 || uv.y < 0.03)
        color = (0.25).rrr;

    color *= saturate(dot(worldNormal, normalize(light))) + 0.33;
    payload.color = color;
}

void HitMirror(inout Payload payload, float2 uv)
{
    if (!payload.allowReflection)
        return;

    float3 pos = WorldRayOrigin() + WorldRayDirection() * RayTCurrent();
    float3 normal = normalize(mul(float3(0, 1, 0), (float3x3) ObjectToWorld4x3()));
    float3 reflected = reflect(normalize(WorldRayDirection()), normal);

    RayDesc mirrorRay;
    mirrorRay.Origin = pos;
    mirrorRay.Direction = reflected;
    mirrorRay.TMin = 0.001;
    mirrorRay.TMax = 1000;

    payload.allowReflection = false;
    TraceRay(scene, RAY_FLAG_NONE, 0xFF, 0, 1, 0, mirrorRay, payload);
}

void HitFloor(inout Payload payload, float2 uv)
{
    float3 pos = WorldRayOrigin() + WorldRayDirection() * RayTCurrent();

    // Checkerboard pattern
    bool2 pattern = frac(pos.xz) > 0.5;
    float checker = (pattern.x ^ pattern.y) ? 0.6 : 0.4;
    payload.color = checker.rrr;

    // Shadow ray toward the light
    RayDesc shadowRay;
    shadowRay.Origin = pos;
    shadowRay.Direction = normalize(light - pos);
    shadowRay.TMin = 0.001;
    shadowRay.TMax = length(light - pos);

    Payload shadow;
    shadow.color = float3(0, 0, 0);
    shadow.allowReflection = false;
    shadow.missed = false;

    TraceRay(scene, RAY_FLAG_NONE, 0xFF, 0, 1, 0, shadowRay, shadow);

    if (!shadow.missed)
        payload.color /= 2;
}