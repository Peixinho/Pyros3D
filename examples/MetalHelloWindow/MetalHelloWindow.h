//============================================================================
// Name        : MetalHelloWindow.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Metal backend's smoke test - a real, GPU-rasterized,
//               depth-tested, indexed-triangle cube drawn through the
//               actual IRenderDevice per-frame contract (BeginFrame/
//               CreatePipeline/CreateBuffer/CreateUniformBuffer/
//               BindPipeline/DrawElements/EndFrame), not the ClearAndPresent
//               bypass that verified the very first milestone. Deliberately
//               does NOT derive from BaseExample: that pulls in SceneGraph/
//               ForwardRenderer/ImGui-GL, none of which this device
//               implements yet (see MetalRenderDevice.mm's stub list) -
//               this drives MetalRenderDevice directly instead, mirroring
//               how VulkanRenderDevice's own bring-up proved CreatePipeline/
//               DrawFrame before any IRenderer integration existed.
//============================================================================

#ifndef METALHELLOWINDOW_H
#define METALHELLOWINDOW_H

#include "../WindowManagers/SDL2Metal/SDL2MetalContext.h"
#include <Pyros3D/Rendering/Device/MetalRenderDevice.h>

using namespace p3d;

class MetalHelloWindow : public SDL2MetalContext {

public:

    MetalHelloWindow();
    virtual ~MetalHelloWindow();

    virtual void Init();
    virtual void Update();
    virtual void Draw();

private:

    // Real IRenderDevice handles - not touched by IRenderer/ForwardRenderer
    // at all, this class drives them directly (see the class comment).
    DeviceHandle vertexShader, fragmentShader;
    DeviceHandle program;
    DeviceHandle vertexBuffer, indexBuffer, vao;
    DeviceHandle mvpBuffer, colorBuffer;
    DeviceHandle pipeline;
    uint32 indexCount;
    bool ready;

    Matrix projection, view;

};

#endif /* METALHELLOWINDOW_H */
