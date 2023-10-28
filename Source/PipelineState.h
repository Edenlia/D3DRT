#pragma once

#include "DXAPI/stdafx.h"
#include "Util/Utility.h"
#include "RootSignature.h"

class PSO
{
public:

    PSO(const wchar_t* Name) : m_Name(Name), m_RootSignature(nullptr), m_PSO(nullptr) {}

    static void DestroyAll(void);

    void SetRootSignature(const RootSignature& BindMappings)
    {
        m_RootSignature = &BindMappings;
    }

    const RootSignature& GetRootSignature(void) const
    {
        ASSERT(m_RootSignature != nullptr);
        return *m_RootSignature;
    }

    ID3D12PipelineState* GetPipelineStateObject(void) const { return m_PSO; }

protected:

    const wchar_t* m_Name;

    const RootSignature* m_RootSignature;

    ID3D12PipelineState* m_PSO;
};

