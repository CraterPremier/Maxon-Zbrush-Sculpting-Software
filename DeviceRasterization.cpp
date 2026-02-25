#include "Device.h"

float4 Device::ClipToScreen(const float4& clipPos) const {
    float invW = 1.0f / clipPos.w;
    float xNDC = clipPos.x * invW;               // [-1, 1]
    float yNDC = clipPos.y * invW;               // [-1, 1]
    float zNDC = clipPos.z * invW;               // [0, 1] для LH перспективы

    float screenX = m_viewport.x + (xNDC * 0.5f + 0.5f) * m_viewport.width;
    float screenY = m_viewport.y + (1.0f - (yNDC * 0.5f + 0.5f)) * m_viewport.height; // flip Y
    float screenZ = m_viewport.minZ + zNDC * (m_viewport.maxZ - m_viewport.minZ);

    return float4(screenX, screenY, screenZ, 1.0f);
}

VertexOutput Interpolate(const VertexOutput& v0, const VertexOutput& v1, const VertexOutput& v2, float a, float b, float c) {
    VertexOutput result;

    // Интерполяция позиции
    result.Position = a * v0.Position + b * v1.Position + c * v2.Position;

    // Интерполяция цвета
    result.Color = a * v0.Color + b * v1.Color + c * v2.Color;

    // Интерполяция текстурных координат
    result.UV = a * v0.UV + b * v1.UV + c * v2.UV;

    return result;
}

inline float edgeFunction(const float4& a, const float4& b, const float2& c) {
    return (c.x - a.x) * (b.y - a.y) - (c.y - a.y) * (b.x - a.x);
}
inline float edgeFunction(const float4& a, const float4& b, const float4& c) {
    return (c.x - a.x) * (b.y - a.y) - (c.y - a.y) * (b.x - a.x);
}

void Device::RasterizeTriangle( const VertexOutput& v0, const VertexOutput& v1, const VertexOutput& v2)
{
    // Получаем размеры экрана
    int width = m_backBuffer.width();
    int height = m_backBuffer.height();

    // 1. Вычисляем bounding box треугольника
    float minX = std::min({ v0.Position.x, v1.Position.x, v2.Position.x });
    float maxX = std::max({ v0.Position.x, v1.Position.x, v2.Position.x });
    float minY = std::min({ v0.Position.y, v1.Position.y, v2.Position.y });
    float maxY = std::max({ v0.Position.y, v1.Position.y, v2.Position.y });

    // Отсекаем по границам экрана
    int iMinX = std::max(0, (int)std::floor(minX));
    int iMaxX = std::min(width - 1, (int)std::ceil(maxX));
    int iMinY = std::max(0, (int)std::floor(minY));
    int iMaxY = std::min(height - 1, (int)std::ceil(maxY));

    // 2. Предвычисляем площадь треугольника (удвоенная)
    float area2 = edgeFunction(v0.Position, v1.Position, v2.Position);

    if (m_cullMode == CullMode::Back && area2 < 0) return;
    if (m_cullMode == CullMode::Front && area2 > 0) return;

    if (std::abs(area2) < 1e-6f) return; // вырожденный треугольник

    // Массив вершин для удобства
    const VertexOutput* verts[3] = { &v0, &v1, &v2 };

    // 3. Проходим по всем пикселям bounding box
    for (int y = iMinY; y <= iMaxY; ++y) {
        for (int x = iMinX; x <= iMaxX; ++x) {
            // Центр пикселя (с половинным смещением)
            float2 p((float)x + 0.5f, (float)y + 0.5f);

            // Вычисляем барицентрические координаты через edge-функции
            float f0 = edgeFunction(v1.Position, v2.Position, p);
            float f1 = edgeFunction(v2.Position, v0.Position, p);
            float f2 = edgeFunction(v0.Position, v1.Position, p);

            if (f0 * area2 < 0 || f1 * area2 < 0 || f2 * area2 < 0) continue;

            // Нормируем на полную площадь
            float a = f0 / area2; // alpha для v0 (напротив v0?)
            float b = f1 / area2; // beta для v1?
            float c = f2 / area2; // gamma для v2?

            // Проверка: a, b, c должны быть в [0,1] и a+b+c = 1
            // Из-за погрешностей могут выходить за диапазон, но можно игнорировать

            // Интерполяция атрибутов
            float z = a * v0.Position.z + b * v1.Position.z + c * v2.Position.z;
            float4 color = a * v0.Color + b * v1.Color + c * v2.Color;
            float2 uv = a * v0.UV + b * v1.UV + c * v2.UV;

            // Проверка глубины
            int idx = y * width + x;
            if (z < m_depthBuffer.at(idx)) {
                m_depthBuffer.at(idx) = z;

                // Формируем VertexOutput для пиксельного шейдера
                VertexOutput frag;
                frag.Position = float4((float)x, (float)y, z, 1.0f); // только для отладки, не обязательно
                frag.Color = color;
                frag.UV = uv;

                // Вызов пиксельного шейдера
                float4 finalColor = m_pixelShader(frag, m_constant_buffer);

                // Запись во фреймбуфер
                m_backBuffer.set_pixel(int2(x, y), finalColor);
            }
        }
    }
}

void Device::DrawIndexed(uint32_t indexCount, uint32_t startIndex) {
    if (!m_vertexShader || !m_pixelShader || m_vertexBuffer.empty() || m_indexBuffer.empty())
        return;

    for (uint32_t i = startIndex; i < startIndex + indexCount; i += 3) {
        // Проверка, что не выходим за границы
        if (i + 2 >= startIndex + indexCount) break;

        uint32_t i0 = m_indexBuffer[i];
        uint32_t i1 = m_indexBuffer[i + 1];
        uint32_t i2 = m_indexBuffer[i + 2];

        if (i0 >= m_vertexBuffer.size() || i1 >= m_vertexBuffer.size() || i2 >= m_vertexBuffer.size())
            continue; // пропускаем некорректные индексы

        const VertexInput& in0 = m_vertexBuffer[i0];
        const VertexInput& in1 = m_vertexBuffer[i1];
        const VertexInput& in2 = m_vertexBuffer[i2];

        VertexOutput out0 = m_vertexShader(in0, m_constant_buffer);
        VertexOutput out1 = m_vertexShader(in1, m_constant_buffer);
        VertexOutput out2 = m_vertexShader(in2, m_constant_buffer);

        // Преобразование в экранные координаты
        out0.Position = ClipToScreen(out0.Position);
        out1.Position = ClipToScreen(out1.Position);
        out2.Position = ClipToScreen(out2.Position);

        // Растеризация треугольника
        RasterizeTriangle(out0, out1, out2);
    }
}
