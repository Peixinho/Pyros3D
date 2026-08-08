//============================================================================
// Name        : MetalHelloWindow.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : See MetalHelloWindow.h.
//============================================================================

#include "MetalHelloWindow.h"
#include <cstdio>

using namespace p3d;

MetalHelloWindow::MetalHelloWindow()
	: SDL2MetalContext(800, 600, "Pyros3D - Metal Hello Window", WindowType::Close)
{
}

MetalHelloWindow::~MetalHelloWindow() {}

void MetalHelloWindow::Init()
{
	if (GetMetalRenderDevice() == NULL)
		fprintf(stderr, "MetalHelloWindow: no MetalRenderDevice - see SDL2MetalContext's own stderr output above for why\n");
}

void MetalHelloWindow::Update()
{
}

void MetalHelloWindow::Draw()
{
	// Direct MetalRenderDevice::ClearAndPresent() call, not through
	// IRenderer/GetActiveRenderDevice() - see the header comment on why
	// this test bypasses both.
	MetalRenderDevice* device = GetMetalRenderDevice();
	if (device == NULL)
		return;
	device->ClearAndPresent(Vec4(0.10f, 0.35f, 0.65f, 1.0f));
}
