RWTexture2D<float4> gOutput : register(u0);

struct Payload
{
    float4 color;
};

[shader("raygeneration")]
void RayGen()
{
    uint2 idx = DispatchRaysIndex().xy;
    float2 dims = float2(DispatchRaysDimensions().xy);
    float2 uv = float2(idx) / dims; // 0..1 across the screen

    // top-left: red    top-right: green
    // bot-left: blue   bot-right: yellow
    float3 color = lerp(
        lerp(float3(1.0, 0.0, 0.0), float3(0.0, 1.0, 0.0), uv.x), // top row
        lerp(float3(0.0, 0.0, 1.0), float3(1.0, 1.0, 0.0), uv.x), // bottom row
        uv.y
    );

    gOutput[idx] = float4(color, 1.0);
}

[shader("miss")]
void Miss(inout Payload payload)
{
    payload.color = float4(0.0, 0.0, 0.0, 1.0);
}