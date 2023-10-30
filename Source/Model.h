#pragma once

#include "DXAPI/stdafx.h"

using namespace DirectX;

class Vertex
{
public:
    Vertex(XMFLOAT3 pos, XMFLOAT4 col)
        :POSITION(pos), NORMAL(0, 0, 0), COLOR(col), UV(0, 0)
    {}

    Vertex(XMFLOAT4 pos, XMFLOAT4 col, XMFLOAT4 n)
        :POSITION(pos.x, pos.y, pos.z), COLOR(col), UV(0, 0), NORMAL(n.x, n.y, n.z)
    {}

    Vertex(XMFLOAT4 pos, XMFLOAT4 col, XMFLOAT4 n, XMFLOAT2 uv)
        :POSITION(pos.x, pos.y, pos.z), COLOR(col), NORMAL(n.x, n.y, n.z), UV(uv.x, uv.y)
    {}


    XMFLOAT3 POSITION;
    XMFLOAT4 COLOR;
    XMFLOAT3 NORMAL;
    XMFLOAT2 UV;
};