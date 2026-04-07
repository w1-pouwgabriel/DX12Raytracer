// ── Uniforms ──────────────────────────────────────────────────────────────────
cbuffer ShaderParams : register(b0)
{
    float  iTime;
    float3 iResolution;
    float4 iMouse;
};

Texture2D    iChannel0 : register(t0);
TextureCube  iChannel1 : register(t1);
SamplerState sampler0  : register(s0);
SamplerState sampler1  : register(s1);

// ── Constants ─────────────────────────────────────────────────────────────────
static const float mouseRotateSpeed = 5.0;
static const float MATERIAL_BODY    = 0.0;
static const float MATERIAL_WING    = 1.0;
static const float OBJECT_SIZE      = 0.5;

#define OBJECTS 40
#define CACHED  5

// ── Types ─────────────────────────────────────────────────────────────────────
struct sdObject
{
    float3 pos;
    float  rad;
    int    index;
};

// Global state (static = per-invocation in HLSL, equivalent to GLSL globals)
static sdObject sdObjects[OBJECTS];
static sdObject cachedObjects[CACHED];
static int      maxCacheIndex = 0;

// ── Rotation matrices ─────────────────────────────────────────────────────────
// HLSL float3x3 is row-major; rows here are the mathematical rows of each
// rotation matrix (transpose of GLSL's column assignments).
// Use mul(M, v) where GLSL used M * v,
// and mul(v, M) where GLSL used v * M  (e.g. p *= M).

float3x3 rotx(float a)
{
    float3x3 rot;
    rot[0] = float3(1.0,     0.0,      0.0    );
    rot[1] = float3(0.0,     cos(a),   sin(a) );
    rot[2] = float3(0.0,    -sin(a),   cos(a) );
    return rot;
}

float3x3 roty(float a)
{
    float3x3 rot;
    rot[0] = float3( cos(a),  0.0,  -sin(a));
    rot[1] = float3( 0.0,     1.0,   0.0   );
    rot[2] = float3( sin(a),  0.0,   cos(a));
    return rot;
}

float3x3 rotz(float a)
{
    float3x3 rot;
    rot[0] = float3( cos(a),  sin(a),  0.0);
    rot[1] = float3(-sin(a),  cos(a),  0.0);
    rot[2] = float3( 0.0,     0.0,     1.0);
    return rot;
}

// ── Distance functions ────────────────────────────────────────────────────────
// from https://iquilezles.org/articles/distfunctions

float udBox(float3 p, float3 b)
{
    return length(max(abs(p) - b, 0.0));
}

float sdHexPrism(float3 p, float2 h)
{
    float3 q = abs(p);
    return max(q.z - h.y, max((q.x * 0.866025 + q.y * 0.5), q.y) - h.x);
}

// ── Object model ──────────────────────────────────────────────────────────────
float2 getModel(in float3 pos, int index)
{
    float phase = (float)index;
    float l     = length(pos);

    float bl   = (sin(pos.z * 12.0 - 5.0) * 0.5 + 0.5) + 0.3;
    float body = sdHexPrism(pos - float3(0.0, 0.0, 0.0),
                            float2(OBJECT_SIZE * 0.04 * bl, OBJECT_SIZE * 0.2));

    float wx = max(abs(l * 6.0 + 0.2) - 0.4, 0.0);
    float sl = 1.5 * abs(sin(wx)) + 0.05;

    float3 wing = float3(OBJECT_SIZE * 0.5,
                         OBJECT_SIZE * 0.01,
                         OBJECT_SIZE * 0.25 * sl);

    float w1 = udBox(mul(rotz( sin(iTime * 22.0 + phase)), pos) - float3(OBJECT_SIZE * 0.5, 0.0, 0.0), wing);
    float w2 = udBox(mul(rotz(-sin(iTime * 22.0 + phase)), pos) + float3(OBJECT_SIZE * 0.5, 0.0, 0.0), wing);

    float id = MATERIAL_BODY;
    if (w1 < body || w2 < body)
        id = MATERIAL_WING;

    float m = min(body, w1);
    m = min(m, w2);

    return float2(m, id);
}

// ── Map ───────────────────────────────────────────────────────────────────────
float2 map(in float3 rp, in sdObject objects[CACHED], inout float3 localPos, inout int index)
{
    float  m   = 9999.0;
    float2 ret = float2(m, 0.0);

    for (int i = 0; i < CACHED; ++i)
    {
        if (i <= maxCacheIndex)
        {
            float3 lp  = rp - objects[i].pos;
            float2 mat = getModel(lp, objects[i].index);

            float a = min(mat.x, m);
            if (a < m)
            {
                m        = a;
                ret      = mat;
                localPos = lp;
                index    = objects[i].index;
            }
        }
    }
    return ret;
}

// ── Pre-step ──────────────────────────────────────────────────────────────────
// Finds objects potentially hit by the ray and caches them.
float prestep(in float3 ro, in float3 rp, in float3 rd,
              in float3 rd90degXAxis, in float3 rd90degYAxis)
{
    maxCacheIndex = -1;
    float m = 99999.0;

    for (int i = 0; i < OBJECTS; ++i)
    {
        float3 sp = -ro + sdObjects[i].pos;

        float distToPlaneY = abs(dot(rd90degYAxis, sp));
        float distToPlaneX = abs(dot(rd90degXAxis, sp));

        float distanceToPlanes = max(distToPlaneY, distToPlaneX) - sdObjects[i].rad;

        float2 mat = getModel(rp - sdObjects[i].pos * (1.0 + distanceToPlanes), sdObjects[i].index);
        float  l   = mat.x;
        m = min(m, l);

        if (distanceToPlanes <= 0.0 && ++maxCacheIndex < CACHED)
        {
            if      (maxCacheIndex == 0) cachedObjects[0] = sdObjects[i];
            else if (maxCacheIndex == 1) cachedObjects[1] = sdObjects[i];
            else if (maxCacheIndex == 2) cachedObjects[2] = sdObjects[i];
            else if (maxCacheIndex == 3) cachedObjects[3] = sdObjects[i];
            else if (maxCacheIndex == 4) cachedObjects[4] = sdObjects[i];
            else return m;
        }
    }
    return m;
}

// ── Trace ─────────────────────────────────────────────────────────────────────
void trace(in float3 rp, in float3 rd, inout float4 color)
{
    float3 ro     = rp;
    float  travel = 0.0;
    const int STEPS = 50;

    // Orthonormal frame for distance-to-plane calculations
    float3 tmp   = normalize(cross(rd, float3(0.0, 1.0, 0.0)));
    float3 up    = normalize(cross(rd, tmp));
    float3 right = cross(rd, up);

    travel = prestep(ro, rp, rd, right, up);
    rp    += travel * rd;

    float3 local    = float3(0.0, 0.0, 0.0);
    int    hitindex = 0;

    for (int i = 0; i < STEPS; ++i)
    {
        float2 mat  = map(rp, cachedObjects, local, hitindex);
        float  dist = mat.x;

        if (dist <= 0.0)
        {
            float indx = (float)hitindex;
            float c1   = sin(indx * 0.1) * 0.5 + 0.5;
            float c2   = abs(cos(abs(local.z * 15.0)) + sin(abs(local.x) * 15.0));
            color      = float4(mat.y, c2 * mat.y, c1 * mat.y, 1.0) * abs(sin(indx * 0.1));
            color.a    = 1.0;
            return;
        }

        float dst = max(0.01, dist);
        travel   += dst;
        rp       += rd * dst;
        if (travel > 30.0) return;
    }
}

// ── Entry point ───────────────────────────────────────────────────────────────
float4 mainImage(float4 svPos : SV_Position) : SV_Target
{
    float2 fragCoord = svPos.xy;
    float4 fragColor = float4(0.0, 0.0, 0.0, 0.0);

    float2 uv    = fragCoord.xy / iResolution.xy;
    uv          -= 0.5;
    uv.y        /= iResolution.x / iResolution.y;

    float2 mouse  = iMouse.xy / iResolution.xy;
    mouse        -= 0.5;
    if (all(mouse == -0.5))
        mouse = 0.0;
    mouse *= mouseRotateSpeed;

    // Populate butterfly objects
    for (int i = 0; i < OBJECTS; ++i)
    {
        float2 uvSample = sin(iTime * 0.001) + 0.21 * float2((float)i, (float)i);
        float3 p        = (iChannel0.Sample(sampler0, uvSample) - 0.5).rgb;
        p               = mul(p, roty(iTime * 2.0));   // row-vec * mat (GLSL: p *= M)
        p.z += (sin(iTime) * 0.5 + 0.5) * 1.0;
        p.x *= 1.0 + (sin(iTime * 0.1) * 0.5 + 0.5) * 0.25;
        p.y *= 1.0 + (cos(iTime * 0.1) * 0.5 + 0.5) * 0.25;

        sdObjects[i].pos   = p * 10.0;
        sdObjects[i].rad   = OBJECT_SIZE * 1.0;
        sdObjects[i].index = i;
    }

    float3 rp = float3(0.0, 0.0, 1.0);
    float3 rd = normalize(float3(uv, 0.3));

    rd = mul(rd, rotx(mouse.y));   // row-vec * mat (GLSL: rd *= M)
    rd = mul(rd, roty(mouse.x));

    trace(rp, rd, fragColor);

    // Background (cubemap)
    float3 envDir = mul(rd, roty(3.14159 * 0.5));
    fragColor = lerp(fragColor, iChannel1.Sample(sampler1, envDir), 1.0 - fragColor.a);

    // Color grading
    float luma = (fragColor.r + fragColor.g + fragColor.b) * 0.33;
    fragColor -= luma * float4(0.9, 0.5, 0.0, 1.0) * clamp(rd.y - 0.05, 0.0, 1.0);
    fragColor += float4(0.2, 0.4, 0.0, 0.0) * abs(clamp(rd.y, -1.0, 0.0));

    // Vignette frame
    fragColor = lerp(fragColor, 0.0, 1.0 - smoothstep(0.5,  0.45, abs(uv.x)));
    fragColor = lerp(fragColor, 0.0, 1.0 - smoothstep(0.28, 0.2,  abs(uv.y)));

    return fragColor;
}