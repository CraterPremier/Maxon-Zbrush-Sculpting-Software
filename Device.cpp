#include "Device.h"

Device::Device(const PresentParameters& params)
    : m_params(params)
    , m_backBuffer(params.BackBufferSize)
    , m_depthBuffer(params.BackBufferSize)
    , m_pixelShader(nullptr)
    , m_vertexShader(nullptr)
    , m_constant_buffer(nullptr)
    , m_constant_buffer_size(0)
    , m_cullMode(CullMode::Back)
    , m_viewport(0, 0, (float)params.BackBufferSize.x, (float)params.BackBufferSize.y, 0, 1)
{
}

void Device::DrawFullScreenQuad() {
    if (!m_pixelShader) return;

    int w = m_backBuffer.width();
    int h = m_backBuffer.height();

    VertexOutput Input = {};

    // Проходим по всем пикселям
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            // Вычисляем нормализованные координаты текстуры [0,1]
            Input.UV = float2((float)x / (w - 1), (float)y / (h - 1));

            // Вызываем пиксельный шейдер
            float4 color = m_pixelShader(Input, m_constant_buffer);

            // Записываем в буфер кадра
            m_backBuffer.set_pixel(int2(x, y), color);
        }
    }
}

void Device::Present() {
    HDC hdc = GetDC(m_params.hDeviceWindow);
    if (hdc) {
        // Получаем текущий размер клиентской области окна (может меняться)
        RECT clientRect;
        GetClientRect(m_params.hDeviceWindow, &clientRect);
        int2 dstSize(clientRect.right - clientRect.left, clientRect.bottom - clientRect.top);

        // Выводим задний буфер в окно (можно масштабировать, но пока просто копируем)
        m_backBuffer.present(hdc, int2(0, 0), dstSize);

        ReleaseDC(m_params.hDeviceWindow, hdc);
    }
}
