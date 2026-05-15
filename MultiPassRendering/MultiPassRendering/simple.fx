float4x4 g_matWorldViewProj;
float4x4 g_matWorld;
float4x4 g_matMirrorViewProj;
float4 g_lightNormal = { 0.3f, 1.0f, 0.5f, 0.0f };
float3 g_ambient = { 0.1f, 0.1f, 0.1f };

texture texture1;
sampler textureSampler = sampler_state {
    Texture = (texture1);
    MipFilter = LINEAR;
    MinFilter = LINEAR;
    MagFilter = LINEAR;
};

// 通常メッシュの頂点を変換し、ライティング結果とUVを出力する。
void VertexShaderNormal(in  float4 inPosition  : POSITION,
                        in  float4 inNormal    : NORMAL0,
                        in  float4 inTexCood   : TEXCOORD0,

                        out float4 outPosition : POSITION,
                        out float4 outDiffuse  : COLOR0,
                        out float4 outTexCood  : TEXCOORD0)
{
    outPosition = mul(inPosition, g_matWorldViewProj);

    float lightIntensity = dot(inNormal, g_lightNormal);
    outDiffuse.rgb = saturate(max(0, lightIntensity) + g_ambient);
    outDiffuse.a = 1.0f;

    outTexCood = inTexCood;
}

// 通常メッシュのテクスチャ色にライティング結果を掛ける。
void PixelShaderNormal(in float4 inScreenColor : COLOR0,
                       in float2 inTexCood     : TEXCOORD0,

                       out float4 outColor     : COLOR)
{
    float4 textureColor = tex2D(textureSampler, inTexCood);
    outColor = inScreenColor * textureColor;
}

// 鏡面メッシュの頂点を変換し、反射テクスチャ参照用の射影座標を出力する。
void VertexShaderMirror(in  float4 inPosition   : POSITION,
                        in  float4 inNormal     : NORMAL0,
                        in  float4 inTexCood    : TEXCOORD0,

                        out float4 outPosition  : POSITION,
                        out float4 outMirrorProj : TEXCOORD0)
{
    float4 worldPos = mul(inPosition, g_matWorld);
    outPosition = mul(inPosition, g_matWorldViewProj);
    outMirrorProj = mul(worldPos, g_matMirrorViewProj);
}

// 鏡カメラの射影座標からUVを作り、反射テクスチャを読む。
void PixelShaderMirror(in float4 inMirrorProj : TEXCOORD0,

                       out float4 outColor    : COLOR)
{
    float2 uv;
    uv.x = inMirrorProj.x / inMirrorProj.w * 0.5f + 0.5f;
    uv.y = -inMirrorProj.y / inMirrorProj.w * 0.5f + 0.5f;
    outColor = tex2D(textureSampler, uv);
}

technique TechniqueNormal
{
    pass Pass1
    {
        CullMode = NONE;

        VertexShader = compile vs_3_0 VertexShaderNormal();
        PixelShader = compile ps_3_0 PixelShaderNormal();
   }
}

technique TechniqueMirror
{
    pass Pass1
    {
        CullMode = NONE;

        VertexShader = compile vs_3_0 VertexShaderMirror();
        PixelShader = compile ps_3_0 PixelShaderMirror();
   }
}
