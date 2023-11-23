# D3DRT
A lite code Rasteration and Ray Tracing Renderer, Implemented by DirectX12 and DXR pipeline.

## Usage

```
git clone --recurse-submodules https://github.com/Edenlia/D3DRT.git
```

open D3DRT.sln and build

## Features

**Implement rasteration and raytracing pipelines and can switch render mode in runtime.**

![switch_mode](./Imgs/switch_mode.gif)



**Implement multiple materials (Blinn-Phong and Disney Principled) in rasteration mode.**

![Rasteration](./Imgs/Rasteration.png)



**imgui change material parameters in runtime**

![imgui](./Imgs/imgui.gif)



**Important sampling for Disney principled BRDF**

Uniform sampling(left), Important sampling(right), using 500ssp: 

![Important sampling](./Imgs/Important_sampling.png)



**Low Discrepancy Sequence for denoise**

Pseudorandom number(left), Sobol sequence number(right), using 20ssp: 

![sobol](./Imgs/sobol.png)

## Reference

https://developer.nvidia.com/rtx/raytracing/dxr/DX12-Raytracing-tutorial-Part-1

https://github.com/microsoft/DirectX-Graphics-Samples

https://github.com/AKGWSB/EzRT
