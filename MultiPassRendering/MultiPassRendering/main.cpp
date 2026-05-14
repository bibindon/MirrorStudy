#pragma comment( lib, "d3d9.lib" )
#if defined(DEBUG) || defined(_DEBUG)
#pragma comment( lib, "d3dx9d.lib" )
#else
#pragma comment( lib, "d3dx9.lib" )
#endif

#include <d3d9.h>
#include <d3dx9.h>
#include <commdlg.h>
#include <windowsx.h>
#include <shlwapi.h>
#include <string>
#include <tchar.h>
#include <cassert>
#include <crtdbg.h>
#include <vector>

#pragma comment( lib, "comdlg32.lib" )
#pragma comment( lib, "shlwapi.lib" )

#define SAFE_RELEASE(p) { if (p) { (p)->Release(); (p) = NULL; } }

namespace
{
    const int kWindowWidth = 1600;
    const int kWindowHeight = 900;
    const float kCameraMoveSpeed = 12.0f;
    const float kCameraRotationSpeed = 0.005f;
    const float kLoadedMeshDistance = 20.0f;
}

LPDIRECT3D9 g_pD3D = NULL;
LPDIRECT3DDEVICE9 g_pd3dDevice = NULL;
LPD3DXFONT g_pFont = NULL;
LPD3DXEFFECT g_pEffect1 = NULL;
LPD3DXEFFECT g_pEffect2 = NULL;

bool g_bClose = false;

LPDIRECT3DTEXTURE9 g_pRenderTarget = NULL;
LPDIRECT3DTEXTURE9 g_pMirrorRenderTarget = NULL;
// 鏡面の射影テクスチャ生成に使う鏡カメラのViewProjection。
D3DXMATRIX g_matMirrorViewProj;

LPDIRECT3DVERTEXDECLARATION9 g_pQuadDecl = NULL;
HWND g_hWnd = NULL;
bool g_bMouseInitialized = false;
POINT g_prevMousePos = { 0, 0 };
LARGE_INTEGER g_qpcFrequency = { };
LARGE_INTEGER g_prevFrameCounter = { };

D3DXVECTOR3 g_cameraPosition(0.0f, 5.0f, -15.0f);
float g_cameraYaw = 0.0f;
float g_cameraPitch = 0.0f;

struct MeshInstance
{
    LPD3DXMESH pMesh = NULL;
    std::vector<D3DMATERIAL9> materials;
    std::vector<LPDIRECT3DTEXTURE9> textures;
    DWORD numMaterials = 0;
    D3DXVECTOR3 center = D3DXVECTOR3(0.0f, 0.0f, 0.0f);
    float radius = 1.0f;
    D3DXVECTOR3 position = D3DXVECTOR3(0.0f, 0.0f, 0.0f);
    D3DXVECTOR3 mirrorPlanePoint = D3DXVECTOR3(0.0f, 0.0f, 0.0f);
    D3DXVECTOR3 mirrorPlaneNormal = D3DXVECTOR3(0.0f, 1.0f, 0.0f);
    bool isMirrorSurface = false;
    bool isGroundSurface = false;
};

struct TextureCacheEntry
{
    std::basic_string<TCHAR> path;
    LPDIRECT3DTEXTURE9 pTexture = NULL;
};

std::vector<MeshInstance> g_meshInstances;
std::vector<TextureCacheEntry> g_textureCache;

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
static bool AddMeshFromXFile(const TCHAR* pPath,
                             const D3DXVECTOR3& position,
                             bool centerAtPosition,
                             bool isMirrorSurface = false,
                             bool isGroundSurface = false);
static void ReleaseMeshResources();
static void UpdateCamera(float deltaTime);
static void UpdateFrame();
static D3DXVECTOR3 GetCameraForward();
static D3DXVECTOR3 GetCameraFocusPoint();
static bool OpenMeshFileDialog(HWND hWnd);
static std::basic_string<TCHAR> BuildTexturePath(const TCHAR* meshPath, const char* textureFileName);
static HRESULT GetOrCreateTexture(const std::basic_string<TCHAR>& texturePath, LPDIRECT3DTEXTURE9* ppTexture);
static void ReleaseTextureCache();
static bool ComputeMirrorPlaneFromMesh(LPD3DXMESH pMesh, D3DXVECTOR3* pPlanePoint, D3DXVECTOR3* pPlaneNormal);
static void RenderSceneToCurrentTarget(const D3DXMATRIX& view,
                                       const D3DXMATRIX& proj,
                                       bool skipMirrorSurface,
                                       bool skipGroundSurface,
                                       bool useMirrorTextureForMirrorSurface);
static void RenderMirrorTexture();
static MeshInstance* GetMirrorMeshInstance();

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
    SetRect(&rect, 0, 0, kWindowWidth, kWindowHeight);
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
    g_hWnd = hWnd;

    InitD3D(hWnd);
    ShowWindow(hWnd, SW_SHOWDEFAULT);
    UpdateWindow(hWnd);
    QueryPerformanceFrequency(&g_qpcFrequency);
    QueryPerformanceCounter(&g_prevFrameCounter);
    D3DXMatrixIdentity(&g_matMirrorViewProj);

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
            UpdateFrame();
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
    d3dpp.BackBufferWidth = kWindowWidth;
    d3dpp.BackBufferHeight = kWindowHeight;
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

    bool bPlateLoadResult = AddMeshFromXFile(_T("res\\plate.x"),
                                             D3DXVECTOR3(0.0f, 0.0f, 0.0f),
                                             false,
                                             false,
                                             true);
    assert(bPlateLoadResult);

    bool bMirrorLoadResult = AddMeshFromXFile(_T("res\\plate.mirror.x"),
                                              D3DXVECTOR3(0.0f, 0.1f, 0.0f),
                                              false,
                                              true,
                                              false);
    assert(bMirrorLoadResult);

    bool bCubeLoadResult = AddMeshFromXFile(_T("res\\cubeNormalInverse.x"),
                                            D3DXVECTOR3(0.0f, 0.0f, 0.0f),
                                            false);
    assert(bCubeLoadResult);

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

    hResult = D3DXCreateTexture(g_pd3dDevice,
                                kWindowWidth,
                                kWindowHeight,
                                1,
                                D3DUSAGE_RENDERTARGET,
                                D3DFMT_A8R8G8B8,
                                D3DPOOL_DEFAULT,
                                &g_pRenderTarget);
    assert(hResult == S_OK);

    hResult = D3DXCreateTexture(g_pd3dDevice,
                                kWindowWidth,
                                kWindowHeight,
                                1,
                                D3DUSAGE_RENDERTARGET,
                                D3DFMT_A8R8G8B8,
                                D3DPOOL_DEFAULT,
                                &g_pMirrorRenderTarget);
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
    ReleaseMeshResources();
    ReleaseTextureCache();
    SAFE_RELEASE(g_pEffect1);
    SAFE_RELEASE(g_pEffect2);
    SAFE_RELEASE(g_pFont);
    SAFE_RELEASE(g_pMirrorRenderTarget);
    SAFE_RELEASE(g_pRenderTarget);
    SAFE_RELEASE(g_pQuadDecl);
    SAFE_RELEASE(g_pd3dDevice);
    SAFE_RELEASE(g_pD3D);
}

bool AddMeshFromXFile(const TCHAR* pPath,
                     const D3DXVECTOR3& position,
                     bool centerAtPosition,
                     bool isMirrorSurface,
                     bool isGroundSurface)
{
    LPD3DXBUFFER pD3DXMtrlBuffer = NULL;
    LPD3DXMESH pMesh = NULL;
    DWORD dwNumMaterials = 0;
    MeshInstance instance;

    HRESULT hResult = D3DXLoadMeshFromX(pPath,
                                        D3DXMESH_SYSTEMMEM,
                                        g_pd3dDevice,
                                        NULL,
                                        &pD3DXMtrlBuffer,
                                        NULL,
                                        &dwNumMaterials,
                                        &pMesh);

    if (FAILED(hResult))
    {
        SAFE_RELEASE(pD3DXMtrlBuffer);
        SAFE_RELEASE(pMesh);
        return false;
    }

    std::vector<D3DMATERIAL9> materials(dwNumMaterials);
    std::vector<LPDIRECT3DTEXTURE9> textures(dwNumMaterials, NULL);

    D3DXMATERIAL* d3dxMaterials = (D3DXMATERIAL*)pD3DXMtrlBuffer->GetBufferPointer();

    for (DWORD i = 0; i < dwNumMaterials; i++)
    {
        materials[i] = d3dxMaterials[i].MatD3D;
        materials[i].Ambient = materials[i].Diffuse;

        if (d3dxMaterials[i].pTextureFilename == NULL)
        {
            continue;
        }

        std::string pTexPath(d3dxMaterials[i].pTextureFilename);

        if (pTexPath.empty())
        {
            continue;
        }

        std::basic_string<TCHAR> resolvedTexturePath = BuildTexturePath(pPath, pTexPath.c_str());
        hResult = GetOrCreateTexture(resolvedTexturePath, &textures[i]);

        if (FAILED(hResult))
        {
            for (auto& texture : textures)
            {
                SAFE_RELEASE(texture);
            }

            SAFE_RELEASE(pD3DXMtrlBuffer);
            SAFE_RELEASE(pMesh);
            return false;
        }
    }

    void* pVertices = NULL;
    hResult = pMesh->LockVertexBuffer(D3DLOCK_READONLY, &pVertices);
    if (SUCCEEDED(hResult))
    {
        D3DXComputeBoundingSphere(static_cast<const D3DXVECTOR3*>(pVertices),
                                  pMesh->GetNumVertices(),
                                  D3DXGetFVFVertexSize(pMesh->GetFVF()),
                                  &instance.center,
                                  &instance.radius);
        pMesh->UnlockVertexBuffer();
    }

    SAFE_RELEASE(pD3DXMtrlBuffer);

    instance.pMesh = pMesh;
    instance.materials.swap(materials);
    instance.textures.swap(textures);
    instance.numMaterials = dwNumMaterials;
    instance.position = centerAtPosition ? (position - instance.center) : position;
    instance.isMirrorSurface = isMirrorSurface;
    instance.isGroundSurface = isGroundSurface;

    if (isMirrorSurface)
    {
        bool bPlaneComputed = ComputeMirrorPlaneFromMesh(instance.pMesh,
                                                         &instance.mirrorPlanePoint,
                                                         &instance.mirrorPlaneNormal);
        assert(bPlaneComputed);
        instance.mirrorPlanePoint += instance.position;

        // 実際に反射計算へ使うワールド空間の鏡平面情報を出力する。
        TCHAR debugText[256];
        _stprintf_s(debugText,
                    _T("Mirror plane world point=(%.3f, %.3f, %.3f) normal=(%.3f, %.3f, %.3f)\r\n"),
                    instance.mirrorPlanePoint.x,
                    instance.mirrorPlanePoint.y,
                    instance.mirrorPlanePoint.z,
                    instance.mirrorPlaneNormal.x,
                    instance.mirrorPlaneNormal.y,
                    instance.mirrorPlaneNormal.z);
        OutputDebugString(debugText);
    }

    g_meshInstances.push_back(instance);

    return true;
}

void ReleaseMeshResources()
{
    for (auto& meshInstance : g_meshInstances)
    {
        for (auto& texture : meshInstance.textures)
        {
            SAFE_RELEASE(texture);
        }

        meshInstance.textures.clear();
        meshInstance.materials.clear();
        meshInstance.numMaterials = 0;
        SAFE_RELEASE(meshInstance.pMesh);
    }

    g_meshInstances.clear();
}

HRESULT GetOrCreateTexture(const std::basic_string<TCHAR>& texturePath, LPDIRECT3DTEXTURE9* ppTexture)
{
    if (ppTexture == NULL)
    {
        return E_POINTER;
    }

    *ppTexture = NULL;

    for (const auto& cacheEntry : g_textureCache)
    {
        if (cacheEntry.path == texturePath && cacheEntry.pTexture != NULL)
        {
            cacheEntry.pTexture->AddRef();
            *ppTexture = cacheEntry.pTexture;
            return S_OK;
        }
    }

    LPDIRECT3DTEXTURE9 pTexture = NULL;
    HRESULT hResult = E_FAIL;

#ifdef UNICODE
    hResult = D3DXCreateTextureFromFileW(g_pd3dDevice, texturePath.c_str(), &pTexture);
#else
    int len = WideCharToMultiByte(CP_ACP, 0, texturePath.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string texturePathA(len, '\0');
    WideCharToMultiByte(CP_ACP, 0, texturePath.c_str(), -1, &texturePathA[0], len, nullptr, nullptr);
    hResult = D3DXCreateTextureFromFileA(g_pd3dDevice, texturePathA.c_str(), &pTexture);
#endif

    if (FAILED(hResult))
    {
        SAFE_RELEASE(pTexture);
        return hResult;
    }

    TextureCacheEntry cacheEntry;
    cacheEntry.path = texturePath;
    cacheEntry.pTexture = pTexture;
    g_textureCache.push_back(cacheEntry);

    pTexture->AddRef();
    *ppTexture = pTexture;
    return S_OK;
}

void ReleaseTextureCache()
{
    for (auto& cacheEntry : g_textureCache)
    {
        SAFE_RELEASE(cacheEntry.pTexture);
    }

    g_textureCache.clear();
}

bool ComputeMirrorPlaneFromMesh(LPD3DXMESH pMesh, D3DXVECTOR3* pPlanePoint, D3DXVECTOR3* pPlaneNormal)
{
    if (pMesh == NULL || pPlanePoint == NULL || pPlaneNormal == NULL)
    {
        return false;
    }

    if ((pMesh->GetFVF() & D3DFVF_XYZ) == 0 || pMesh->GetNumFaces() == 0)
    {
        return false;
    }

    void* pVertices = NULL;
    HRESULT hResult = pMesh->LockVertexBuffer(D3DLOCK_READONLY, &pVertices);
    if (FAILED(hResult))
    {
        return false;
    }

    LPDIRECT3DINDEXBUFFER9 pIndexBuffer = NULL;
    hResult = pMesh->GetIndexBuffer(&pIndexBuffer);
    if (FAILED(hResult))
    {
        pMesh->UnlockVertexBuffer();
        return false;
    }

    void* pIndices = NULL;
    hResult = pIndexBuffer->Lock(0, 0, &pIndices, D3DLOCK_READONLY);
    if (FAILED(hResult))
    {
        SAFE_RELEASE(pIndexBuffer);
        pMesh->UnlockVertexBuffer();
        return false;
    }

    const UINT vertexStride = pMesh->GetNumBytesPerVertex();
    const BYTE* pVertexBytes = static_cast<const BYTE*>(pVertices);
    DWORD i0 = 0;
    DWORD i1 = 0;
    DWORD i2 = 0;

    if ((pMesh->GetOptions() & D3DXMESH_32BIT) != 0)
    {
        const DWORD* pIndexData = static_cast<const DWORD*>(pIndices);
        i0 = pIndexData[0];
        i1 = pIndexData[1];
        i2 = pIndexData[2];
    }
    else
    {
        const WORD* pIndexData = static_cast<const WORD*>(pIndices);
        i0 = pIndexData[0];
        i1 = pIndexData[1];
        i2 = pIndexData[2];
    }

    const D3DXVECTOR3& v0 = *reinterpret_cast<const D3DXVECTOR3*>(pVertexBytes + vertexStride * i0);
    const D3DXVECTOR3& v1 = *reinterpret_cast<const D3DXVECTOR3*>(pVertexBytes + vertexStride * i1);
    const D3DXVECTOR3& v2 = *reinterpret_cast<const D3DXVECTOR3*>(pVertexBytes + vertexStride * i2);

    D3DXVECTOR3 edge1 = v1 - v0;
    D3DXVECTOR3 edge2 = v2 - v0;
    D3DXVECTOR3 normal;
    D3DXVec3Cross(&normal, &edge1, &edge2);

    const bool isValidNormal = D3DXVec3LengthSq(&normal) > 0.0f;
    if (isValidNormal)
    {
        D3DXVec3Normalize(&normal, &normal);
        *pPlanePoint = v0;
        *pPlaneNormal = normal;

        // 鏡平面の向きをデバッグしやすいように出力する。
        TCHAR debugText[256];
        _stprintf_s(debugText,
                    _T("Mirror plane local point=(%.3f, %.3f, %.3f) normal=(%.3f, %.3f, %.3f)\r\n"),
                    pPlanePoint->x,
                    pPlanePoint->y,
                    pPlanePoint->z,
                    pPlaneNormal->x,
                    pPlaneNormal->y,
                    pPlaneNormal->z);
        OutputDebugString(debugText);
    }

    pIndexBuffer->Unlock();
    SAFE_RELEASE(pIndexBuffer);
    pMesh->UnlockVertexBuffer();

    return isValidNormal;
}

void UpdateFrame()
{
    LARGE_INTEGER currentCounter = { };
    QueryPerformanceCounter(&currentCounter);

    float deltaTime = 0.016f;
    if (g_qpcFrequency.QuadPart > 0)
    {
        deltaTime = static_cast<float>(currentCounter.QuadPart - g_prevFrameCounter.QuadPart)
                  / static_cast<float>(g_qpcFrequency.QuadPart);
    }

    g_prevFrameCounter = currentCounter;

    if (deltaTime < 0.0f)
    {
        deltaTime = 0.0f;
    }

    if (deltaTime > 0.1f)
    {
        deltaTime = 0.1f;
    }

    UpdateCamera(deltaTime);
}

void UpdateCamera(float deltaTime)
{
    D3DXVECTOR3 forward = GetCameraForward();
    D3DXVECTOR3 forwardOnPlane(forward.x, 0.0f, forward.z);
    if (D3DXVec3LengthSq(&forwardOnPlane) > 0.0f)
    {
        D3DXVec3Normalize(&forwardOnPlane, &forwardOnPlane);
    }

    D3DXVECTOR3 worldUp(0.0f, 1.0f, 0.0f);
    D3DXVECTOR3 right;
    D3DXVec3Cross(&right, &worldUp, &forwardOnPlane);
    if (D3DXVec3LengthSq(&right) > 0.0f)
    {
        D3DXVec3Normalize(&right, &right);
    }

    D3DXVECTOR3 move(0.0f, 0.0f, 0.0f);

    if ((GetAsyncKeyState('W') & 0x8000) != 0)
    {
        move += forwardOnPlane;
    }

    if ((GetAsyncKeyState('S') & 0x8000) != 0)
    {
        move -= forwardOnPlane;
    }

    if ((GetAsyncKeyState('D') & 0x8000) != 0)
    {
        move += right;
    }

    if ((GetAsyncKeyState('A') & 0x8000) != 0)
    {
        move -= right;
    }

    if ((GetAsyncKeyState('E') & 0x8000) != 0)
    {
        move.y += 1.0f;
    }

    if ((GetAsyncKeyState('Q') & 0x8000) != 0)
    {
        move.y -= 1.0f;
    }

    if (D3DXVec3LengthSq(&move) > 0.0f)
    {
        D3DXVec3Normalize(&move, &move);
        g_cameraPosition += move * (kCameraMoveSpeed * deltaTime);
    }
}

D3DXVECTOR3 GetCameraForward()
{
    const float cosPitch = cosf(g_cameraPitch);
    D3DXVECTOR3 forward(sinf(g_cameraYaw) * cosPitch,
                        sinf(g_cameraPitch),
                        cosf(g_cameraYaw) * cosPitch);
    D3DXVec3Normalize(&forward, &forward);
    return forward;
}

D3DXVECTOR3 GetCameraFocusPoint()
{
    return g_cameraPosition + GetCameraForward() * kLoadedMeshDistance;
}

bool OpenMeshFileDialog(HWND hWnd)
{
    TCHAR szFile[MAX_PATH] = { };
    OPENFILENAME ofn = { };
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hWnd;
    ofn.lpstrFilter = _T("X Files (*.x)\0*.x\0All Files (*.*)\0*.*\0");
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    ofn.lpstrDefExt = _T("x");

    if (!GetOpenFileName(&ofn))
    {
        return false;
    }

    bool bLoaded = AddMeshFromXFile(szFile, GetCameraFocusPoint(), true);
    g_bMouseInitialized = false;
    return bLoaded;
}

std::basic_string<TCHAR> BuildTexturePath(const TCHAR* meshPath, const char* textureFileName)
{
#ifdef UNICODE
    int texturePathLen = MultiByteToWideChar(CP_ACP, 0, textureFileName, -1, nullptr, 0);
    std::wstring texturePath(texturePathLen, 0);
    MultiByteToWideChar(CP_ACP, 0, textureFileName, -1, &texturePath[0], texturePathLen);
#else
    std::string texturePath(textureFileName);
#endif

    TCHAR meshDirectory[MAX_PATH] = { };
    _tcscpy_s(meshDirectory, meshPath);
    PathRemoveFileSpec(meshDirectory);

    TCHAR combinedPath[MAX_PATH] = { };
    PathCombine(combinedPath, meshDirectory, texturePath.c_str());
    return std::basic_string<TCHAR>(combinedPath);
}

MeshInstance* GetMirrorMeshInstance()
{
    for (auto& meshInstance : g_meshInstances)
    {
        if (meshInstance.isMirrorSurface)
        {
            return &meshInstance;
        }
    }

    return NULL;
}

void RenderSceneToCurrentTarget(const D3DXMATRIX& view,
                                const D3DXMATRIX& proj,
                                bool skipMirrorSurface,
                                bool skipGroundSurface,
                                bool useMirrorTextureForMirrorSurface)
{
    HRESULT hResult = g_pEffect1->SetTechnique("Technique1");
    assert(hResult == S_OK);

    UINT numPass = 0;
    hResult = g_pEffect1->Begin(&numPass, 0);
    assert(hResult == S_OK);

    hResult = g_pEffect1->BeginPass(0);
    assert(hResult == S_OK);

    D3DXMATRIX matWorld;
    D3DXMATRIX matWorldViewProj;

    for (const auto& meshInstance : g_meshInstances)
    {
        if (skipMirrorSurface && meshInstance.isMirrorSurface)
        {
            continue;
        }

        if (skipGroundSurface && meshInstance.isGroundSurface)
        {
            continue;
        }

        D3DXMatrixTranslation(&matWorld, meshInstance.position.x, meshInstance.position.y, meshInstance.position.z);
        matWorldViewProj = matWorld * view * proj;

        // 鏡面だけはワールド座標から鏡カメラ射影UVを作るため、World行列も渡す。
        hResult = g_pEffect1->SetMatrix("g_matWorld", &matWorld);
        assert(hResult == S_OK);

        hResult = g_pEffect1->SetMatrix("g_matWorldViewProj", &matWorldViewProj);
        assert(hResult == S_OK);

        const bool useMirrorTexture = useMirrorTextureForMirrorSurface && meshInstance.isMirrorSurface && g_pMirrorRenderTarget != NULL;
        hResult = g_pEffect1->SetBool("g_bUseLighting", !useMirrorTexture);
        assert(hResult == S_OK);

        // 鏡面だけ射影テクスチャ用の分岐を有効にする。
        hResult = g_pEffect1->SetBool("g_bMirrorSurface", useMirrorTexture);
        assert(hResult == S_OK);

        hResult = g_pEffect1->SetMatrix("g_matMirrorViewProj", &g_matMirrorViewProj);
        assert(hResult == S_OK);

        hResult = g_pEffect1->SetBool("g_bUseTexture", TRUE);
        assert(hResult == S_OK);

        for (DWORD i = 0; i < meshInstance.numMaterials; i++)
        {
            LPDIRECT3DTEXTURE9 pTexture = useMirrorTexture ? g_pMirrorRenderTarget : meshInstance.textures[i];
            hResult = g_pEffect1->SetTexture("texture1", pTexture);
            assert(hResult == S_OK);

            hResult = g_pEffect1->CommitChanges();
            assert(hResult == S_OK);

            hResult = meshInstance.pMesh->DrawSubset(i);
            assert(hResult == S_OK);
        }
    }

    hResult = g_pEffect1->EndPass();
    assert(hResult == S_OK);

    hResult = g_pEffect1->End();
    assert(hResult == S_OK);
}

void RenderMirrorTexture()
{
    MeshInstance* pMirrorMesh = GetMirrorMeshInstance();
    if (pMirrorMesh == NULL)
    {
        return;
    }

    HRESULT hResult = E_FAIL;

    LPDIRECT3DSURFACE9 pOldRenderTarget = NULL;
    hResult = g_pd3dDevice->GetRenderTarget(0, &pOldRenderTarget);
    assert(hResult == S_OK);

    LPDIRECT3DSURFACE9 pOldDepthStencil = NULL;
    hResult = g_pd3dDevice->GetDepthStencilSurface(&pOldDepthStencil);
    assert(hResult == S_OK);

    LPDIRECT3DSURFACE9 pMirrorRenderSurface = NULL;
    hResult = g_pMirrorRenderTarget->GetSurfaceLevel(0, &pMirrorRenderSurface);
    assert(hResult == S_OK);

    hResult = g_pd3dDevice->SetRenderTarget(0, pMirrorRenderSurface);
    assert(hResult == S_OK);

    D3DXMATRIX matReflection;
    const D3DXVECTOR3& planePoint = pMirrorMesh->mirrorPlanePoint;
    const D3DXVECTOR3& planeNormal = pMirrorMesh->mirrorPlaneNormal;
    D3DXPLANE mirrorPlane(planeNormal.x,
                          planeNormal.y,
                          planeNormal.z,
                          -D3DXVec3Dot(&planeNormal, &planePoint));
    D3DXMatrixReflect(&matReflection, &mirrorPlane);

    D3DXVECTOR3 eye = g_cameraPosition;
    D3DXVECTOR3 target = GetCameraFocusPoint();
    D3DXVECTOR3 up(0.0f, 1.0f, 0.0f);
    D3DXVECTOR3 reflectedEye;
    D3DXVECTOR3 reflectedTarget;
    D3DXVECTOR3 reflectedUp;

    D3DXVec3TransformCoord(&reflectedEye, &eye, &matReflection);
    D3DXVec3TransformCoord(&reflectedTarget, &target, &matReflection);
    D3DXVec3TransformNormal(&reflectedUp, &up, &matReflection);

    D3DXMATRIX view;
    D3DXMatrixLookAtLH(&view, &reflectedEye, &reflectedTarget, &reflectedUp);

    D3DXMATRIX proj;
    D3DXMatrixPerspectiveFovLH(&proj,
                               D3DXToRadian(45),
                               static_cast<float>(kWindowWidth) / static_cast<float>(kWindowHeight),
                               1.0f,
                               10000.0f);

    // 後段の鏡面サンプリングで使うため、鏡カメラのViewProjectionを保持する。
    g_matMirrorViewProj = view * proj;

    hResult = g_pd3dDevice->Clear(0,
                                  NULL,
                                  D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER,
                                  D3DCOLOR_XRGB(20, 20, 20),
                                  1.0f,
                                  0);
    assert(hResult == S_OK);

    hResult = g_pd3dDevice->BeginScene();
    assert(hResult == S_OK);

    // 鏡面自身だけスキップし、地面は反射に含める。
    RenderSceneToCurrentTarget(view, proj, true, false, false);

    hResult = g_pd3dDevice->EndScene();
    assert(hResult == S_OK);

    hResult = g_pd3dDevice->SetRenderTarget(0, pOldRenderTarget);
    assert(hResult == S_OK);

    hResult = g_pd3dDevice->SetDepthStencilSurface(pOldDepthStencil);
    assert(hResult == S_OK);

    SAFE_RELEASE(pMirrorRenderSurface);
    SAFE_RELEASE(pOldDepthStencil);
    SAFE_RELEASE(pOldRenderTarget);
}

void RenderPass1()
{
    HRESULT hResult = E_FAIL;

    RenderMirrorTexture();

    LPDIRECT3DSURFACE9 pOldRenderTarget = nullptr;
    hResult = g_pd3dDevice->GetRenderTarget(0, &pOldRenderTarget);
    assert(hResult == S_OK);

    LPDIRECT3DSURFACE9 pRenderTarget;
    hResult = g_pRenderTarget->GetSurfaceLevel(0, &pRenderTarget);
    assert(hResult == S_OK);

    hResult = g_pd3dDevice->SetRenderTarget(0, pRenderTarget);
    assert(hResult == S_OK);

    D3DXMATRIX matWorldViewProj;
    D3DXMATRIX matWorld;
    D3DXMATRIX View, Proj;

    D3DXMatrixPerspectiveFovLH(&Proj,
                               D3DXToRadian(45),
                               static_cast<float>(kWindowWidth) / static_cast<float>(kWindowHeight),
                               1.0f,
                               10000.0f);

    D3DXVECTOR3 vec1 = g_cameraPosition;
    D3DXVECTOR3 vec2 = GetCameraFocusPoint();
    D3DXVECTOR3 vec3(0, 1, 0);
    D3DXMatrixLookAtLH(&View, &vec1, &vec2, &vec3);

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
    _tcscpy_s(msg, 100, _T("WASD:移動  Q/E:下降/上昇  マウス:回転  F1:Xファイル読込"));
    TextDraw(g_pFont, msg, 0, 0);

    RenderSceneToCurrentTarget(View, Proj, false, false, true);

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
    float du = 0.5f / static_cast<float>(kWindowWidth);
    float dv = 0.5f / static_cast<float>(kWindowHeight);

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
    case WM_MOUSEMOVE:
    {
        POINT currentMousePos;
        currentMousePos.x = GET_X_LPARAM(lParam);
        currentMousePos.y = GET_Y_LPARAM(lParam);

        if (!g_bMouseInitialized)
        {
            g_prevMousePos = currentMousePos;
            g_bMouseInitialized = true;
            return 0;
        }

        LONG deltaX = currentMousePos.x - g_prevMousePos.x;
        LONG deltaY = currentMousePos.y - g_prevMousePos.y;
        g_prevMousePos = currentMousePos;

        g_cameraYaw += static_cast<float>(deltaX) * kCameraRotationSpeed;
        g_cameraPitch -= static_cast<float>(deltaY) * kCameraRotationSpeed;

        const float pitchLimit = D3DXToRadian(89.0f);
        if (g_cameraPitch > pitchLimit)
        {
            g_cameraPitch = pitchLimit;
        }
        else if (g_cameraPitch < -pitchLimit)
        {
            g_cameraPitch = -pitchLimit;
        }

        return 0;
    }
    case WM_KEYDOWN:
    {
        if (wParam == VK_F1 && (lParam & 0x40000000) == 0)
        {
            OpenMeshFileDialog(hWnd);
            return 0;
        }
        break;
    }
    }

    return DefWindowProc(hWnd, msg, wParam, lParam);
}
