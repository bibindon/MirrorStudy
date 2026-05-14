float4x4 g_matWorldViewProj;
float4x4 g_matWorld;
float4x4 g_matMirrorViewProj;
float4 g_lightNormal = { 0.3f, 1.0f, 0.5f, 0.0f };
float3 g_ambient = { 0.3f, 0.3f, 0.3f };

bool g_bUseTexture = true;
bool g_bUseLighting = true;
bool g_bMirrorSurface = false;

texture texture1;
sampler textureSampler = sampler_state {
    Texture = (texture1);
    MipFilter = LINEAR;
    MinFilter = LINEAR;
    MagFilter = LINEAR;
};

void VertexShader1(in  float4 inPosition  : POSITION,
                   in  float4 inNormal    : NORMAL0,
                   in  float4 inTexCood   : TEXCOORD0,

                   out float4 outPosition : POSITION,
                   out float4 outDiffuse  : COLOR0,
                   out float4 outTexCood  : TEXCOORD0,
                   out float4 outMirrorProj : TEXCOORD1)
{
    // é¡é¢ã ã‘ã¯ãƒ¯ãƒ¼ãƒ«ãƒ‰åº§æ¨™ã‚’é¡ã‚«ãƒ¡ãƒ©ã¸å°„å½±ã—ã€ãã®çµæžœã‹ã‚‰åå°„RTã®UVã‚’ä½œã‚‹ã€‚
    float4 worldPos = mul(inPosition, g_matWorld);
    outPosition = mul(inPosition, g_matWorldViewProj);
    outMirrorProj = mul(worldPos, g_matMirrorViewProj);

    if (g_bUseLighting)
    {
        float lightIntensity = dot(inNormal, g_lightNormal);
        outDiffuse.rgb = max(0, lightIntensity) + g_ambient;
        outDiffuse.a = 1.0f;
    }
    else
    {
        outDiffuse = 1.0f;
    }

    outTexCood = inTexCood;
}

void PixelShader1(in float4 inScreenColor : COLOR0,
                  in float2 inTexCood     : TEXCOORD0,
                  in float4 inMirrorProj  : TEXCOORD1,

                  out float4 outColor     : COLOR)
{
    float4 workColor = (float4)0;

    // Ã©ÂÂ¡Ã©ÂÂ¢Ã£ÂÂ Ã£Ââ€˜Ã£ÂÂ¯Ã©ÂÂ¡Ã£â€šÂ«Ã£Æ’Â¡Ã£Æ’Â©Ã£ÂÂ®Ã¥Â°â€žÃ¥Â½Â±Ã¥ÂºÂ§Ã¦Â¨â„¢Ã£Ââ€¹Ã£â€šâ€°UVÃ£â€šâ€™Ã¤Â½Å“Ã£ÂÂ£Ã£ÂÂ¦Ã¥ÂÂÃ¥Â°â€žRTÃ£â€šâ€™Ã¨ÂªÂ­Ã£â€šâ‚¬Ã£â‚¬â€š
    if (g_bMirrorSurface)
    {
        float2 uv;
        uv.x = inMirrorProj.x / inMirrorProj.w * 0.5f + 0.5f;
        uv.y = -inMirrorProj.y / inMirrorProj.w * 0.5f + 0.5f;
        workColor = tex2D(textureSampler, uv);
    }
    else
    {
        workColor = tex2D(textureSampler, inTexCood);
    }

    if (g_bUseTexture)
    {
        outColor = inScreenColor * workColor;
    }
    else
    {
        outColor = inScreenColor;
    }
}

technique Technique1
{
    pass Pass1
    {
        CullMode = NONE;

        VertexShader = compile vs_3_0 VertexShader1();
        PixelShader = compile ps_3_0 PixelShader1();
   }
}
