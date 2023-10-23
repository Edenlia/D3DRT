#ifndef _D3D_UTIL_H_
#define _D3D_UTIL_H_

#include "../DXAPI/stdafx.h"

UINT CalcConstantBufferByteSize(UINT byteSize)
{
	// Constant buffers must be a multiple of the 256
	return (byteSize + 255) & ~255;
}

#endif
