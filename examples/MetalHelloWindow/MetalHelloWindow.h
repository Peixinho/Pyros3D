//============================================================================
// Name        : MetalHelloWindow.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Metal backend's first-light smoke test - mirrors what
//               VulkanRenderDevice's own ClearAndPresent() milestone
//               verified before anything else (IRenderer, BaseExample,
//               ForwardRenderer) depended on it. Deliberately does NOT
//               derive from BaseExample: that pulls in SceneGraph/
//               ForwardRenderer/ImGui-GL, none of which this device
//               implements yet (see MetalRenderDevice.mm's stub list) -
//               this only needs a window + MetalRenderDevice::ClearAndPresent().
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

};

#endif /* METALHELLOWINDOW_H */
