#include <windows.h>
#include <cmath>
#include "Device.h"

std::vector<VertexInput> vertices = { };
std::vector<uint32_t> indices = { };

Device* g_pDevice = nullptr;

struct TransformCB {
    float4x4 wvp;  // world-view-projection matrix
};

VertexOutput vsTransform(const VertexInput& in, const void* constants) {
    const TransformCB* cb = static_cast<const TransformCB*>(constants);
    VertexOutput out;
    out.Position = cb->wvp * float4(in.Position.x, in.Position.y, in.Position.z, 1.0f);
    out.Color = in.Color;
    out.UV = float2(0, 0);
    return out;
}

float4 psColor(const VertexOutput& in, const void* constants) {
    return in.Color;
}

void CreateCube(std::vector<VertexInput>& vertices, std::vector<uint32_t>& indices) {
    vertices.clear();
    indices.clear();

    // Координаты вершин куба (от -1 до 1)
    float3 corners[8] = {
        float3(-1, -1, -1), // 0
        float3(1, -1, -1), // 1
        float3(1,  1, -1), // 2
        float3(-1,  1, -1), // 3
        float3(-1, -1,  1), // 4
        float3(1, -1,  1), // 5
        float3(1,  1,  1), // 6
        float3(-1,  1,  1)  // 7
    };

    // Цвета для каждой грани (R, G, B, A)
    float4 colors[6] = {
        float4(1, 0, 0, 1), // +X (красный)
        float4(0, 1, 0, 1), // -X (зелёный)
        float4(0, 0, 1, 1), // +Y (синий)
        float4(1, 1, 0, 1), // -Y (жёлтый)
        float4(0, 1, 1, 1), // +Z (голубой)
        float4(1, 0, 1, 1)  // -Z (фиолетовый)
    };

    // Определяем грани: для каждой грани задаём 4 индекса из corners[] и номер цвета
    int faces[6][4] = {
        {1, 5, 6, 2}, // +X (правая)
        {0, 3, 7, 4}, // -X (левая)
        {3, 2, 6, 7}, // +Y (верх)
        {0, 4, 5, 1}, // -Y (низ)
        {4, 7, 6, 5}, // +Z (передняя)
        {0, 1, 2, 3}  // -Z (задняя)
    };

    // Для каждой грани создаём 4 вершины (с одинаковым цветом) и 2 треугольника
    for (int f = 0; f < 6; ++f) {
        int idx[4] = { faces[f][0], faces[f][1], faces[f][2], faces[f][3] };
        float4 col = colors[f];

        // Добавляем 4 вершины
        int start = (int)vertices.size();
        vertices.push_back({ corners[idx[0]], col });
        vertices.push_back({ corners[idx[1]], col });
        vertices.push_back({ corners[idx[2]], col });
        vertices.push_back({ corners[idx[3]], col });

        // Первый треугольник (0,1,2)
        indices.push_back(start);
        indices.push_back(start + 1);
        indices.push_back(start + 2);

        // Второй треугольник (0,2,3)
        indices.push_back(start);
        indices.push_back(start + 2);
        indices.push_back(start + 3);
    }
}

void RenderFrame() {
    static float angleX = 0.0f;
    static float angleY = 0.0f;
    angleX += 0.01f;
    angleY += 0.02f;

    float3 eye(0, 0, -50);
    float3 target(0, 0, 0);
    float3 up(0, 1, 0);

    float4x4 view = lookAtLH(eye, target, up);

    float4x4 model = rotationY(angleY) * rotationX(angleX);
    float aspect = 800.0f / 600.0f;
    float4x4 proj = perspectiveLH(DegToRad(67.5f), aspect, 0.1f, 100.0f);

    TransformCB cb;
    cb.wvp = proj * view * model;

    g_pDevice->Clear(float4(0.2f, 0.2f, 0.2f, 1.0f));
    g_pDevice->ClearDepth(1.0f);

    g_pDevice->SetVertexShader(vsTransform);
    g_pDevice->SetPixelShader(psColor);
    g_pDevice->SetConstantBuffer(&cb, sizeof(cb));
    g_pDevice->SetVertexBuffer(vertices);
    g_pDevice->SetIndexBuffer(indices);

    g_pDevice->DrawIndexed((uint32_t)indices.size(), 0);

    g_pDevice->Present();
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE:
        SetTimer(hWnd, 1, 16, NULL); // 60 FPS
        CreateCube(vertices, indices);
        return 0;
    case WM_TIMER: {
        if (!g_pDevice) 
            break;
        RenderFrame();
        return 0;
    }
    case WM_DESTROY:
        KillTimer(hWnd, 1);
        PostQuitMessage(0);
        return 0;
    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE) DestroyWindow(hWnd);
        return 0;
    }
    return DefWindowProc(hWnd, msg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow) {
    const wchar_t CLASS_NAME[] = L"TriangleTest";
    WNDCLASS wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    RegisterClass(&wc);

    HWND hWnd = CreateWindowEx(0, CLASS_NAME, L"Software Renderer - Triangle Test",
        WS_OVERLAPPEDWINDOW & ~WS_THICKFRAME & ~WS_MAXIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, 800, 600,
        nullptr, nullptr, hInstance, nullptr);
    if (!hWnd) return -1;

    PresentParameters pp;
    pp.BackBufferSize = int2(800, 600);
    pp.hDeviceWindow = hWnd;
    pp.Windowed = true;

    Device device(pp);
    g_pDevice = &device;

    ShowWindow(hWnd, nCmdShow);

    MSG msg = {};
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    g_pDevice = nullptr;
    return 0;
}
