#pragma comment( lib, "d3d9.lib" )
#if defined(DEBUG) || defined(_DEBUG)
#pragma comment( lib, "d3dx9d.lib" )
#else
#pragma comment( lib, "d3dx9.lib" )
#endif

#include <d3d9.h>
#include <d3dx9.h>
#include <string>
#include <tchar.h>
#include <cassert>
#include <crtdbg.h>
#include <vector>

#define SAFE_RELEASE(p) { if (p) { (p)->Release(); (p) = NULL; } }

LPDIRECT3D9 g_pD3D = NULL;
LPDIRECT3DDEVICE9 g_pd3dDevice = NULL;
LPD3DXFONT g_pFont = NULL;
LPD3DXMESH g_pMesh = NULL;

LPD3DXMESH g_pMeshSphere = NULL;

std::vector<D3DMATERIAL9> g_pMaterials;
std::vector<LPDIRECT3DTEXTURE9> g_pTextures;
DWORD g_dwNumMaterials = 0;
LPD3DXEFFECT g_pEffect1 = NULL;
LPD3DXEFFECT g_pEffect2 = NULL;

bool g_bClose = false;

LPDIRECT3DTEXTURE9 g_pRenderTarget = NULL;

LPDIRECT3DVERTEXDECLARATION9 g_pQuadDecl = NULL;

struct QuadVertex
{
    // クリップ空間用（-1..1, w=1）
    float x;
    float y;
    float z;
    float w;

    // テクスチャ座標（今回は未使用でも可）
    float u;
    float v;
};

static void TextDraw(LPD3DXFONT pFont, TCHAR* text, int X, int Y);
static void InitD3D(HWND hWnd);
static void Cleanup();

static void RenderPass1();
static void RenderPass2();
static void DrawFullscreenQuad();

LRESULT WINAPI MsgProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

extern int WINAPI _tWinMain(_In_ HINSTANCE hInstance,
                            _In_opt_ HINSTANCE hPrevInstance,
                            _In_ LPTSTR lpCmdLine,
                            _In_ int nCmdShow);

int WINAPI _tWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPTSTR lpCmdLine,
                     _In_ int nCmdShow)
{
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);

    WNDCLASSEX wc { };
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.style = CS_CLASSDC;
    wc.lpfnWndProc = MsgProc;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 0;
    wc.hInstance = GetModuleHandle(NULL);
    wc.hIcon = NULL;
    wc.hCursor = NULL;
    wc.hbrBackground = NULL;
    wc.lpszMenuName = NULL;
    wc.lpszClassName = _T("Window1");
    wc.hIconSm = NULL;

    ATOM atom = RegisterClassEx(&wc);
    assert(atom != 0);

    RECT rect;
    SetRect(&rect, 0, 0, 640, 480);
    AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);
    rect.right = rect.right - rect.left;
    rect.bottom = rect.bottom - rect.top;
    rect.top = 0;
    rect.left = 0;

    HWND hWnd = CreateWindow(_T("Window1"),
                             _T("Hello DirectX9 World !!"),
                             WS_OVERLAPPEDWINDOW,
                             CW_USEDEFAULT,
                             CW_USEDEFAULT,
                             rect.right,
                             rect.bottom,
                             NULL,
                             NULL,
                             wc.hInstance,
                             NULL);

    InitD3D(hWnd);
    ShowWindow(hWnd, SW_SHOWDEFAULT);
    UpdateWindow(hWnd);

    MSG msg;

    while (true)
    {
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
            DispatchMessage(&msg);
        }
        else
        {
            Sleep(16);

            RenderPass1();
            RenderPass2();
        }

        if (g_bClose)
        {
            break;
        }
    }

    Cleanup();

    UnregisterClass(_T("Window1"), wc.hInstance);
    return 0;
}

void TextDraw(LPD3DXFONT pFont, TCHAR* text, int X, int Y)
{
    RECT rect = { X, Y, 0, 0 };

    // DrawTextの戻り値は文字数である。
    // そのため、hResultの中身が整数でもエラーが起きているわけではない。
    HRESULT hResult = pFont->DrawText(NULL,
                                      text,
                                      -1,
                                      &rect,
                                      DT_LEFT | DT_NOCLIP,
                                      D3DCOLOR_ARGB(255, 0, 0, 0));

    assert((int)hResult >= 0);
}

void InitD3D(HWND hWnd)
{
    HRESULT hResult = E_FAIL;

    g_pD3D = Direct3DCreate9(D3D_SDK_VERSION);
    assert(g_pD3D != NULL);

    D3DPRESENT_PARAMETERS d3dpp;
    ZeroMemory(&d3dpp, sizeof(d3dpp));
    d3dpp.Windowed = TRUE;
    d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    d3dpp.BackBufferFormat = D3DFMT_UNKNOWN;
    d3dpp.BackBufferCount = 1;
    d3dpp.MultiSampleType = D3DMULTISAMPLE_NONE;
    d3dpp.MultiSampleQuality = 0;
    d3dpp.EnableAutoDepthStencil = TRUE;
    d3dpp.AutoDepthStencilFormat = D3DFMT_D16;
    d3dpp.hDeviceWindow = hWnd;
    d3dpp.Flags = 0;
    d3dpp.FullScreen_RefreshRateInHz = D3DPRESENT_RATE_DEFAULT;
    d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_DEFAULT;

    hResult = g_pD3D->CreateDevice(D3DADAPTER_DEFAULT,
                                   D3DDEVTYPE_HAL,
                                   hWnd,
                                   D3DCREATE_HARDWARE_VERTEXPROCESSING,
                                   &d3dpp,
                                   &g_pd3dDevice);

    if (FAILED(hResult))
    {
        hResult = g_pD3D->CreateDevice(D3DADAPTER_DEFAULT,
                                       D3DDEVTYPE_HAL,
                                       hWnd,
                                       D3DCREATE_SOFTWARE_VERTEXPROCESSING,
                                       &d3dpp,
                                       &g_pd3dDevice);

        assert(hResult == S_OK);
    }

    hResult = D3DXCreateFont(g_pd3dDevice,
                             20,
                             0,
                             FW_HEAVY,
                             1,
                             FALSE,
                             SHIFTJIS_CHARSET,
                             OUT_TT_ONLY_PRECIS,
                             CLEARTYPE_NATURAL_QUALITY,
                             FF_DONTCARE,
                             _T("ＭＳ ゴシック"),
                             &g_pFont);

    assert(hResult == S_OK);

    LPD3DXBUFFER pD3DXMtrlBuffer = NULL;

    hResult = D3DXLoadMeshFromX(_T("cube.x"),
                                D3DXMESH_SYSTEMMEM,
                                g_pd3dDevice,
                                NULL,
                                &pD3DXMtrlBuffer,
                                NULL,
                                &g_dwNumMaterials,
                                &g_pMesh);

    assert(hResult == S_OK);

    D3DXMATERIAL* d3dxMaterials = (D3DXMATERIAL*)pD3DXMtrlBuffer->GetBufferPointer();
    g_pMaterials.resize(g_dwNumMaterials);
    g_pTextures.resize(g_dwNumMaterials);

    for (DWORD i = 0; i < g_dwNumMaterials; i++)
    {
        g_pMaterials[i] = d3dxMaterials[i].MatD3D;
        g_pMaterials[i].Ambient = g_pMaterials[i].Diffuse;
        g_pTextures[i] = NULL;
        
        //--------------------------------------------------------------
        // Unicode文字セットでもマルチバイト文字セットでも
        // "d3dxMaterials[i].pTextureFilename"はマルチバイト文字セットになる。
        // 
        // 一方で、D3DXCreateTextureFromFileはプロジェクト設定で
        // Unicode文字セットかマルチバイト文字セットか変わる。
        //--------------------------------------------------------------

        std::string pTexPath(d3dxMaterials[i].pTextureFilename);

        if (!pTexPath.empty())
        {
            bool bUnicode = false;

#ifdef UNICODE
            bUnicode = true;
#endif

            if (!bUnicode)
            {
                hResult = D3DXCreateTextureFromFileA(g_pd3dDevice, pTexPath.c_str(), &g_pTextures[i]);
                assert(hResult == S_OK);
            }
            else
            {
                int len = MultiByteToWideChar(CP_ACP, 0, pTexPath.c_str(), -1, nullptr, 0);
                std::wstring pTexPathW(len, 0);
                MultiByteToWideChar(CP_ACP, 0, pTexPath.c_str(), -1, &pTexPathW[0], len);

                hResult = D3DXCreateTextureFromFileW(g_pd3dDevice, pTexPathW.c_str(), &g_pTextures[i]);
                assert(hResult == S_OK);
            }
        }
    }

    hResult = pD3DXMtrlBuffer->Release();
    assert(hResult == S_OK);

    hResult = D3DXCreateEffectFromFile(g_pd3dDevice,
                                       _T("simple.fx"),
                                       NULL,
                                       NULL,
                                       D3DXSHADER_DEBUG,
                                       NULL,
                                       &g_pEffect1,
                                       NULL);

    assert(hResult == S_OK);

    hResult = D3DXCreateEffectFromFile(g_pd3dDevice,
                                       _T("simple2.fx"),
                                       NULL,
                                       NULL,
                                       D3DXSHADER_DEBUG,
                                       NULL,
                                       &g_pEffect2,
                                       NULL);

    assert(hResult == S_OK);

    hResult = D3DXCreateSphere(g_pd3dDevice,
                               20.f,
                               32,
                               32,
                               &g_pMeshSphere,
                               NULL);

    assert(hResult == S_OK);

    hResult = D3DXCreateTexture(g_pd3dDevice,
                                640,
                                480,
                                1,
                                D3DUSAGE_RENDERTARGET,
                                D3DFMT_A8R8G8B8,
                                D3DPOOL_DEFAULT,
                                &g_pRenderTarget);
    assert(hResult == S_OK);

    D3DVERTEXELEMENT9 elems[] =
    {
        { 0,  0, D3DDECLTYPE_FLOAT4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
        { 0, 16, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },
        D3DDECL_END()
    };

    HRESULT hr = g_pd3dDevice->CreateVertexDeclaration(elems, &g_pQuadDecl);
    assert(hr == S_OK);
}

void Cleanup()
{
    for (auto& texture : g_pTextures)
    {
        SAFE_RELEASE(texture);
    }

    SAFE_RELEASE(g_pMesh);
    SAFE_RELEASE(g_pMeshSphere);
    SAFE_RELEASE(g_pEffect1);
    SAFE_RELEASE(g_pEffect2);
    SAFE_RELEASE(g_pFont);
    SAFE_RELEASE(g_pd3dDevice);
    SAFE_RELEASE(g_pD3D);
}

void RenderPass1()
{
    HRESULT hResult = E_FAIL;

    LPDIRECT3DSURFACE9 pOldRenderTarget = nullptr;
    hResult = g_pd3dDevice->GetRenderTarget(0, &pOldRenderTarget);
    assert(hResult == S_OK);

    LPDIRECT3DSURFACE9 pRenderTarget;
    hResult = g_pRenderTarget->GetSurfaceLevel(0, &pRenderTarget);
    assert(hResult == S_OK);

    hResult = g_pd3dDevice->SetRenderTarget(0, pRenderTarget);
    assert(hResult == S_OK);

    static float f = 0.0f;
    f += 0.025f;

    D3DXMATRIX mat;
    D3DXMATRIX View, Proj;

    D3DXMatrixPerspectiveFovLH(&Proj,
                               D3DXToRadian(45),
                               640.0f / 480.0f,
                               1.0f,
                               10000.0f);

    D3DXVECTOR3 vec1(10 * sinf(f), 5, -10 * cosf(f));
    D3DXVECTOR3 vec2(0, 0, 0);
    D3DXVECTOR3 vec3(0, 1, 0);
    D3DXMatrixLookAtLH(&View, &vec1, &vec2, &vec3);
    D3DXMatrixIdentity(&mat);
    mat = mat * View * Proj;

    hResult = g_pEffect1->SetMatrix("g_matWorldViewProj", &mat);
    assert(hResult == S_OK);

    hResult = g_pd3dDevice->Clear(0,
                                  NULL,
                                  D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER,
                                  D3DCOLOR_XRGB(100, 100, 100),
                                  1.0f,
                                  0);

    assert(hResult == S_OK);

    hResult = g_pd3dDevice->BeginScene();
    assert(hResult == S_OK);

    TCHAR msg[100];
    _tcscpy_s(msg, 100, _T("SSAOに挑戦"));
    TextDraw(g_pFont, msg, 0, 0);

    hResult = g_pEffect1->SetTechnique("Technique1");
    assert(hResult == S_OK);

    UINT numPass;
    hResult = g_pEffect1->Begin(&numPass, 0);
    assert(hResult == S_OK);

    hResult = g_pEffect1->BeginPass(0);
    assert(hResult == S_OK);

    hResult = g_pEffect1->SetBool("g_bUseTexture", TRUE);
    assert(hResult == S_OK);

    for (DWORD i = 0; i < g_dwNumMaterials; i++)
    {
        hResult = g_pEffect1->SetTexture("texture1", g_pTextures[i]);
        assert(hResult == S_OK);

        hResult = g_pEffect1->CommitChanges();
        assert(hResult == S_OK);

        hResult = g_pMesh->DrawSubset(i);
        assert(hResult == S_OK);
    }

    {
        hResult = g_pEffect1->SetBool("g_bUseTexture", FALSE);
        assert(hResult == S_OK);

        hResult = g_pEffect1->CommitChanges();
        assert(hResult == S_OK);

        hResult = g_pMeshSphere->DrawSubset(0);
        assert(hResult == S_OK);
    }

    hResult = g_pEffect1->EndPass();
    assert(hResult == S_OK);

    hResult = g_pEffect1->End();
    assert(hResult == S_OK);

    hResult = g_pd3dDevice->EndScene();
    assert(hResult == S_OK);

    hResult = g_pd3dDevice->SetRenderTarget(0, pOldRenderTarget);
    assert(hResult == S_OK);
}

void RenderPass2()
{
    HRESULT hResult = E_FAIL;

    hResult = g_pd3dDevice->Clear(0,
                                  NULL,
                                  D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER,
                                  D3DCOLOR_XRGB(0, 0, 0),
                                  1.0f,
                                  0);
    assert(hResult == S_OK);

    // 2Dフルスクリーン描画なのでZは不要
    hResult = g_pd3dDevice->SetRenderState(D3DRS_ZENABLE, FALSE);
    assert(hResult == S_OK);

    hResult = g_pd3dDevice->BeginScene();
    assert(hResult == S_OK);

    hResult = g_pEffect2->SetTechnique("Technique1");
    assert(hResult == S_OK);

    UINT numPass = 0;
    hResult = g_pEffect2->Begin(&numPass, 0);
    assert(hResult == S_OK);

    hResult = g_pEffect2->BeginPass(0);
    assert(hResult == S_OK);

    hResult = g_pEffect2->SetTexture("texture1", g_pRenderTarget);
    assert(hResult == S_OK);

    hResult = g_pEffect2->CommitChanges();
    assert(hResult == S_OK);

    DrawFullscreenQuad();

    hResult = g_pEffect2->EndPass();
    assert(hResult == S_OK);

    hResult = g_pEffect2->End();
    assert(hResult == S_OK);

    hResult = g_pd3dDevice->EndScene();
    assert(hResult == S_OK);

    hResult = g_pd3dDevice->Present(NULL, NULL, NULL, NULL);
    assert(hResult == S_OK);

    hResult = g_pd3dDevice->SetRenderState(D3DRS_ZENABLE, TRUE);
    assert(hResult == S_OK);
}

void DrawFullscreenQuad()
{
    QuadVertex v[4] { };

    // クリップ空間の矩形（TriangleStrip）
    float du = 0.5f / 640.f;
    float dv = 0.5f / 480.f;

    v[0].x = -1.0f;
    v[0].y = -1.0f;
    v[0].z = 0.0f;
    v[0].w = 1.0f;
    v[0].u = 0.0f + du;
    v[0].v = 1.0f - dv;

    v[1].x = -1.0f;
    v[1].y = 1.0f;
    v[1].z = 0.0f;
    v[1].w = 1.0f;
    v[1].u = 0.0f + du;
    v[1].v = 0.0f + dv;

    v[2].x = 1.0f;
    v[2].y = -1.0f;
    v[2].z = 0.0f;
    v[2].w = 1.0f;
    v[2].u = 1.0f - du;
    v[2].v = 1.0f - dv;

    v[3].x = 1.0f;
    v[3].y = 1.0f;
    v[3].z = 0.0f;
    v[3].w = 1.0f;
    v[3].u = 1.0f - du;
    v[3].v = 0.0f + dv;

    g_pd3dDevice->SetVertexDeclaration(g_pQuadDecl);
    g_pd3dDevice->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, v, sizeof(QuadVertex));
}

LRESULT WINAPI MsgProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_DESTROY:
    {
        PostQuitMessage(0);
        g_bClose = true;
        return 0;
    }
    }

    return DefWindowProc(hWnd, msg, wParam, lParam);
}

