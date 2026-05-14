float4x4 g_matWorldViewProj;
float4 g_lightNormal = { 0.3f, 1.0f, 0.5f, 0.0f };
float3 g_ambient = { 0.3f, 0.3f, 0.3f };

bool g_bUseTexture = true;

texture texture1;
sampler textureSampler = sampler_state {
    Texture = (texture1);
    MipFilter = NONE;
    MinFilter = POINT;
    MagFilter = POINT;
};

void VertexShader1(in  float4 inPosition  : POSITION,
                   in  float2 inTexCood   : TEXCOORD0,

                   out float4 outPosition : POSITION,
                   out float2 outTexCood  : TEXCOORD0)
{
    outPosition = inPosition;
    outTexCood = inTexCood;
}

void PixelShader1(in float4 inPosition    : POSITION,
                  in float2 inTexCood     : TEXCOORD0,

                  out float4 outColor     : COLOR)
{
    float4 workColor = (float4)0;
    workColor = tex2D(textureSampler, inTexCood);

    float average = (workColor.r + workColor.g + workColor.b) / 3;

    // 試しに彩度を上げたり下げたりしてみる
    if (true)
    {
        workColor.r += (workColor.r - average);
        workColor.g += (workColor.g - average);
        workColor.b += (workColor.b - average);
    }
    else
    {
        workColor.r -= (workColor.r - average) / 2.f;
        workColor.g -= (workColor.g - average) / 2.f;
        workColor.b -= (workColor.b - average) / 2.f;
    }

    workColor = saturate(workColor);

    outColor = workColor;
    
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
