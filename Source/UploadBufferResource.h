#ifndef _UPLOAD_BUFFER_RESOURCE_H_
#define _UPLOAD_BUFFER_RESOURCE_H_

#include "D3DUtil.h"
#include "./stdafx.h"
#include "./DXSample.h"

template <typename T>
class UploadBufferResource
{
public:
	UploadBufferResource(ComPtr<ID3D12Device> d3dDevice, UINT elementCount, bool isConstantBuffer)
		: m_isConstantBuffer(isConstantBuffer) 
	{
		UINT m_elementByteSize = sizeof(T);

		if (isConstantBuffer)
		{
			m_elementByteSize = CalcConstantBufferByteSize(sizeof(T));
			// Create upload heap and resources
			ThrowIfFailed(d3dDevice->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
				D3D12_HEAP_FLAG_NONE,
				&CD3DX12_RESOURCE_DESC::Buffer(m_elementByteSize * elementCount),
				D3D12_RESOURCE_STATE_GENERIC_READ,
				nullptr,
				IID_PPV_ARGS(&m_uploadBuffer)));
			// return the pointer that should be used to update the data
			ThrowIfFailed(m_uploadBuffer->Map(0,// subresource index
				nullptr,// to the range of the resource to map
				reinterpret_cast<void**>(&m_mappedData)));// pointer to the mapped resource
		}
	}

	~UploadBufferResource() {
		if (m_uploadBuffer != nullptr)
			m_uploadBuffer->Unmap(0, nullptr);// unmap the resource

		m_mappedData = nullptr;// set the pointer to null
	}

	void CopyData(int elementIndex, const T& Data)
	{
		memcpy(&m_mappedData[elementIndex * m_elementByteSize], &Data, sizeof(T));
	}

	ComPtr<ID3D12Resource> Resource()const
	{
		return m_uploadBuffer;
	}

private:
	bool m_isConstantBuffer;
	ComPtr<ID3D12Resource> m_uploadBuffer;
	T* m_mappedData = nullptr;
	UINT m_elementByteSize;

};


#endif
