//============================================================================
// Name        : MetalHelloWindow.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : See MetalHelloWindow.h.
//============================================================================

#include "MetalHelloWindow.h"
#include <Pyros3D/Core/Buffers/GeometryBuffer.h>
#include <Pyros3D/Rendering/Components/Rendering/RenderingComponent.h>
#include <cstdio>

using namespace p3d;

namespace {

	// Explicit layout()s throughout - CompileShaderStage() fails loudly on
	// anything needing AutoFixForVulkan() (loose uniforms), not implemented
	// on this backend yet (see its comment). Binding 8/9, not 0/1: MSL
	// shares one buffer-index namespace between per-vertex attribute
	// buffers (bound at indices 0..vertexLayout.size()-1, see
	// MetalRenderDevice::CreatePipeline()'s vertex descriptor) and UBOs -
	// reserving 0-7 for vertex buffers avoids colliding with this cube's
	// own position buffer at index 0.
	const char* kVertexSource =
		"#version 450\n"
		"layout(location = 0) in vec3 aPosition;\n"
		"layout(std140, binding = 8) uniform MVPBlock { mat4 uMVP; };\n"
		"void main() {\n"
		"    gl_Position = uMVP * vec4(aPosition, 1.0);\n"
		"}\n";

	const char* kFragmentSource =
		"#version 450\n"
		"layout(location = 0) out vec4 fragColor;\n"
		"layout(std140, binding = 9) uniform ColorBlock { vec4 uColor; };\n"
		"void main() {\n"
		"    fragColor = uColor;\n"
		"}\n";

	// Unit cube, centered at the origin - position only (matches
	// kVertexSource's single aPosition attribute).
	const float kCubePositions[8 * 3] = {
		-1.f, -1.f, -1.f,
		 1.f, -1.f, -1.f,
		 1.f,  1.f, -1.f,
		-1.f,  1.f, -1.f,
		-1.f, -1.f,  1.f,
		 1.f, -1.f,  1.f,
		 1.f,  1.f,  1.f,
		-1.f,  1.f,  1.f,
	};

	// 12 triangles, 2 per face - winding doesn't matter here since this
	// test never calls SetCullFaceMode() (Metal's own encoder default is
	// MTLCullModeNone - see MetalRenderDevice::DisableCullFace()'s
	// counterpart).
	const uint32 kCubeIndices[36] = {
		0, 1, 2,  0, 2, 3, // -Z
		4, 5, 6,  4, 6, 7, // +Z
		0, 1, 5,  0, 5, 4, // -Y
		3, 2, 6,  3, 6, 7, // +Y
		0, 3, 7,  0, 7, 4, // -X
		1, 2, 6,  1, 6, 5, // +X
	};

}

MetalHelloWindow::MetalHelloWindow()
	: SDL2MetalContext(800, 600, "Pyros3D - Metal Hello Window", WindowType::Close),
	  vertexShader(0), fragmentShader(0), program(0),
	  vertexBuffer(0), indexBuffer(0), vao(0),
	  mvpBuffer(0), colorBuffer(0), pipeline(0),
	  indexCount(0), ready(false)
{
}

MetalHelloWindow::~MetalHelloWindow() {}

void MetalHelloWindow::Init()
{
	MetalRenderDevice* device = GetMetalRenderDevice();
	if (device == NULL)
	{
		fprintf(stderr, "MetalHelloWindow: no MetalRenderDevice - see SDL2MetalContext's own stderr output above for why\n");
		return;
	}

	projection = Matrix::PerspectiveMatrix(60.f, (f32)Width / (f32)Height, 0.1f, 100.f);
	view.identity();
	view.LookAt(Vec3(3.f, 3.f, 6.f), Vec3(0.f, 0.f, 0.f), Vec3(0.f, 1.f, 0.f));

	std::string errorLog;

	vertexShader = device->CreateShaderStage(ShaderType::VertexShader);
	if (!device->CompileShaderStage(vertexShader, kVertexSource, errorLog))
	{
		fprintf(stderr, "MetalHelloWindow: vertex shader compile failed: %s\n", errorLog.c_str());
		return;
	}

	fragmentShader = device->CreateShaderStage(ShaderType::FragmentShader);
	errorLog.clear();
	if (!device->CompileShaderStage(fragmentShader, kFragmentSource, errorLog))
	{
		fprintf(stderr, "MetalHelloWindow: fragment shader compile failed: %s\n", errorLog.c_str());
		return;
	}

	program = device->CreateProgram();
	device->AttachShaderStage(program, vertexShader);
	device->AttachShaderStage(program, fragmentShader);
	errorLog.clear();
	if (!device->LinkProgram(program, errorLog))
	{
		fprintf(stderr, "MetalHelloWindow: LinkProgram failed: %s\n", errorLog.c_str());
		return;
	}

	vao = device->CreateVertexArray();
	device->BindVertexArray(0, vao);
	vertexBuffer = device->CreateBuffer(Buffer::Type::Vertex, Buffer::Draw::Static, kCubePositions, sizeof(kCubePositions));
	device->BindArrayBuffer(vertexBuffer);
	indexBuffer = device->CreateBuffer(Buffer::Type::Index, Buffer::Draw::Static, kCubeIndices, sizeof(kCubeIndices));
	device->BindElementBuffer(indexBuffer);
	indexCount = sizeof(kCubeIndices) / sizeof(kCubeIndices[0]);

	mvpBuffer = device->CreateUniformBuffer(sizeof(Matrix), 8);
	colorBuffer = device->CreateUniformBuffer(sizeof(Vec4), 9);
	Vec4 orange(0.9f, 0.5f, 0.15f, 1.0f);
	device->ReplaceUniformBuffer(colorBuffer, sizeof(Vec4), &orange);

	IRenderDevice::PipelineDesc desc;
	desc.shaderProgram = program;
	IRenderDevice::VertexBufferLayoutDesc layout;
	layout.stride = sizeof(float) * 3;
	IRenderDevice::VertexAttributeDesc attr;
	attr.name = "aPosition";
	attr.type = Buffer::Attribute::Type::Vec3;
	attr.offset = 0;
	attr.divisor = 0;
	layout.attributes.push_back(attr);
	desc.vertexLayout.push_back(layout);
	// depthTest/depthWrite/depthTestMode, blendingEnabled, cullFace,
	// wireframe, noVertexInput all keep PipelineDesc's own defaults
	// (depth-tested opaque solid cube, no culling, no blending).

	pipeline = device->CreatePipeline(desc);
	if (pipeline == 0)
	{
		fprintf(stderr, "MetalHelloWindow: CreatePipeline failed\n");
		return;
	}

	ready = true;
}

void MetalHelloWindow::Update()
{
}

void MetalHelloWindow::Draw()
{
	MetalRenderDevice* device = GetMetalRenderDevice();
	if (device == NULL || !ready)
		return;

	device->SetClearColor(Vec4(0.05f, 0.07f, 0.10f, 1.0f));
	device->BeginFrame();

	Matrix mvp = device->TranslateProjectionMatrix(projection) * view;
	device->ReplaceUniformBuffer(mvpBuffer, sizeof(Matrix), &mvp);

	device->BindPipeline(0, pipeline);
	device->BindVertexArray(0, vao);
	device->DrawElements(0, DrawingType::Triangles, indexCount);

	device->EndFrame();
}
