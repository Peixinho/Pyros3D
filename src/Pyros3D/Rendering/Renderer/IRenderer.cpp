//============================================================================
// Name        : IRenderer.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Renderer Interface
//============================================================================

#include <Pyros3D/Rendering/Renderer/IRenderer.h>
#include <Pyros3D/Other/PyrosGL.h>

namespace p3d {

	// ViewPort Dimension
	uint32 IRenderer::_viewPortStartX = 0;
	uint32 IRenderer::_viewPortStartY = 0;
	uint32 IRenderer::_viewPortEndX = 0;
	uint32 IRenderer::_viewPortEndY = 0;

	IRenderer::IRenderer() {}

	IRenderer::IRenderer(const uint32 Width, const uint32 Height)
	{
		// Background Unset by Default
		BackgroundColorSet = false;

		// Set Global Light Default Color
		GlobalLight = Vec4(0.2f, 0.2f, 0.2f, 0.2f);

		// Save Dimensions
		this->Width = Width;
		this->Height = Height;

		// Depth Bias
		IsUsingDepthBias = false;

		// Custom ViewPort
		customViewPort = false;

		// Blending Flag
		blending = false;

		// Defaults
		ClearBufferBit(Buffer_Bit::Color | Buffer_Bit::Depth);
		depthWritting = depthTesting = false;
		clearDepthBuffer = true;
		sorting = true;
		scissorTest = false;
		scissorTestX = 0;
		scissorTestY = 0;
		scissorTestWidth = Width;
		scissorTestHeight = Height;
		lod = false;
		ClipPlane = false;
	}

	void IRenderer::Resize(const uint32 Width, const uint32 Height)
	{
		// Save Dimensions
		this->Width = Width;
		this->Height = Height;

		if (!customViewPort)
		{
			viewPortEndX = Width;
			viewPortEndY = Height;
		}
	}

	void IRenderer::SetViewPort(const uint32 initX, const uint32 initY, const uint32 endX, const uint32 endY)
	{
		viewPortStartX = initX;
		viewPortStartY = initY;
		viewPortEndX = endX;
		viewPortEndY = endY;
		customViewPort = true;
	}
	void IRenderer::_SetViewPort(const uint32 initX, const uint32 initY, const uint32 endX, const uint32 endY)
	{
		if (initX != _viewPortStartX || initY != _viewPortStartY || endX != _viewPortEndX || endY != _viewPortEndY)
		{
			_viewPortStartX = initX;
			_viewPortStartY = initY;
			_viewPortEndX = endX;
			_viewPortEndY = endY;
			GLCHECKER(glViewport(initX, initY, endX, endY));
		}
	}

	IRenderer::~IRenderer()
	{

	}

	// Internal Function
	void IRenderer::RenderScene(const p3d::Projection& projection, GameObject* Camera, SceneGraph* Scene) {

	}
	void IRenderer::RenderSceneByTag(const p3d::Projection& projection, GameObject* Camera, SceneGraph* Scene, const uint32 Tag) {

	}
	void IRenderer::RenderSceneByTag(const p3d::Projection& projection, GameObject* Camera, SceneGraph* Scene, const std::string &Tag) {

	}

	void IRenderer::InitRender()
	{
		LastProgramUsed = -1;
		LastMaterialUsed = -1;
		LastMeshRendered = -1;
		InternalDrawType = -1;
		LastMaterialPTR = NULL;
		LastMeshRenderedPTR = NULL;
		cullFace = -1;
	}

	void IRenderer::EndRender()
	{
		if (LastMeshRenderedPTR != NULL && LastMaterialPTR != NULL)
		{
			// Unbind Index Buffer
			if (LastMeshRenderedPTR->Geometry->GetGeometryType() == GeometryType::BUFFER)
				GLCHECKER(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0));
			// Unbind Vertex Attributes
			UnbindMesh(LastMeshRenderedPTR, LastMaterialPTR);
			// Unbind Shadow Maps
			UnbindShadowMaps(LastMaterialPTR);
			// Material After Render
			LastMaterialPTR->AfterRender();
			// Unbind Shader Program
			GLCHECKER(glUseProgram(0));
			// Unset Pointers
			LastMaterialPTR = NULL;
			LastMeshRenderedPTR = NULL;
			LastProgramUsed = -1;
			LastMaterialUsed = -1;
			LastMeshRendered = -1;
			depthWritting = false;
			depthTesting = false;

			DepthTest();
			DepthWrite();

			DisableBlending();
		}
	}

	void IRenderer::RenderObject(RenderingMesh* rmesh, GameObject* owner, IMaterial* Material)
	{
		// model cache
		ModelMatrix = owner->GetWorldTransformation() * rmesh->Pivot;

		NormalMatrixIsDirty = true;
		ModelViewMatrixIsDirty = true;
		ModelViewProjectionMatrixIsDirty = true;
		ModelMatrixInverseIsDirty = true;
		ModelViewMatrixInverseIsDirty = true;
		ModelMatrixInverseTransposeIsDirty = true;

		if ((LastMeshRenderedPTR != rmesh || LastMaterialPTR != Material || LastMeshRenderedPTR->Geometry->GetGeometryType() == GeometryType::ARRAY) && LastProgramUsed != -1)
		{
			// Unbind Index Buffer
			if (LastMeshRenderedPTR->Geometry->GetGeometryType() == GeometryType::BUFFER)
				GLCHECKER(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0));
			// Unbind Mesh
			UnbindMesh(LastMeshRenderedPTR, LastMaterialPTR);
			// Material Stuff After Render
			UnbindShadowMaps(LastMaterialPTR);
			// After Render
			LastMaterialPTR->AfterRender();
		}
		if (LastProgramUsed != Material->GetShader()) GLCHECKER(glUseProgram(Material->GetShader()));

		if (LastMeshRenderedPTR != rmesh || LastMaterialPTR != Material || LastMeshRenderedPTR->Geometry->GetGeometryType() == GeometryType::ARRAY)
		{
			// Bind Mesh
			BindMesh(rmesh, Material);

			// Material Stuff Pre Render
			Material->PreRender();

			// Bind Shadow Maps
			BindShadowMaps(Material);

			// Send Global Uniforms
			SendGlobalUniforms(rmesh, Material);

			// Send Vertex Attributes
			SendAttributes(rmesh, Material);

			if (Material->depthBias)
				EnableDepthBias(Vec2(Material->depthFactor, Material->depthUnits));

			// Bind Index Buffer
			if (rmesh->Geometry->GetGeometryType() == GeometryType::BUFFER)
				GLCHECKER(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, rmesh->Geometry->IndexBuffer->ID));
		}

		// Check double sided
		if (rmesh->Material != Material)
		{
			if (rmesh->Material->GetCullFace() != Material->GetCullFace())
			{
				Material->SetCullFace(rmesh->Material->GetCullFace());
				cullFaceChanged = true;
			}
		}
		if (LastMaterialPTR != Material || cullFaceChanged)
		{
			// Check if Material is DoubleSided
			if (Material->GetCullFace() != cullFace)
			{
				switch (Material->GetCullFace())
				{
				case CullFace::FrontFace:
					GLCHECKER(glEnable(GL_CULL_FACE));
					GLCHECKER(glCullFace(GL_FRONT));
					break;
				case CullFace::DoubleSided:
					GLCHECKER(glDisable(GL_CULL_FACE));
					break;
				case CullFace::BackFace:
				default:
					GLCHECKER(glEnable(GL_CULL_FACE));
					GLCHECKER(glCullFace(GL_BACK));
					break;
				}
				cullFace = Material->GetCullFace();
				cullFaceChanged = false;
			}

			// Check if Material is WireFrame
			(Material->IsWireFrame() ? EnableWireFrame() : DisableWireFrame());

			// Material Render Method
			Material->Render();
		}

		if (LastMeshRenderedPTR != rmesh && (InternalDrawType == -1 || InternalDrawType != rmesh->GetDrawingType()))
		{
			// getting material drawing type
			switch (rmesh->GetDrawingType())
			{
			case DrawingType::Lines:
				DrawType = GL_LINES;
				break;
#if !defined(GLES2)
			case DrawingType::Polygons:
				DrawType = GL_POLYGON;
				break;
			case DrawingType::Quads:
				DrawType = GL_QUADS;
				break;
#endif
			case DrawingType::Points:
				DrawType = GL_POINTS;
				break;
			case DrawingType::Line_Loop:
				DrawType = GL_LINE_LOOP;
				break;
			case DrawingType::Line_Strip:
				DrawType = GL_LINE_STRIP;
				break;
			case DrawingType::Triangle_Fan:
				DrawType = GL_TRIANGLE_FAN;
				break;
			case DrawingType::Triangle_Strip:
				DrawType = GL_TRIANGLE_STRIP;
				break;
			case DrawingType::Triangles:
			default:
				DrawType = GL_TRIANGLES;
				break;
			}
			InternalDrawType = rmesh->GetDrawingType();
		}

		// Send User Uniforms
		SendUserUniforms(rmesh, Material);

		// Send Model Specific Uniforms
		SendModelUniforms(rmesh, Material);

		// Depth Write
		if (Material->IsDepthWritting() != depthWritting)
		{
			depthWritting = Material->IsDepthWritting();
			DepthWrite();
		}

		// Depth Test
		if (Material->IsDepthTesting() != depthTesting || Material->depthTestMode != depthTestMode)
		{
			depthTesting = Material->IsDepthTesting();
			DepthTest(Material->depthTestMode);
		}

		// Enable / Disable Blending
		if (Material->blending || Material->IsTransparent())
		{
			// Default for Transparency
			uint32 s = BlendFunc::Src_Alpha;
			uint32 d = BlendFunc::One_Minus_Src_Alpha;
			uint32 m = BlendEq::Add;

			// Override for transparency
			if (Material->blending)
			{
				s = Material->sfactor;
				d = Material->dfactor;
				m = Material->mode;
			}

			if (!blending || s != sfactor || d != dfactor || m != mode)
			{
				EnableBlending();
				BlendingEquation(m);
				BlendingFunction(s, d);
			}
		}
		else if (blending && (!Material->IsTransparent() || !Material->blending)) DisableBlending();
		
		// Draw
		if (rmesh->Geometry->GetGeometryType() == GeometryType::BUFFER)
		{
			GLCHECKER(glDrawElements(DrawType, rmesh->Geometry->GetIndexData().size(), __INDEX_TYPE__, BUFFER_OFFSET(0)));
		}
		else {
			GLCHECKER(glDrawElements(DrawType, rmesh->Geometry->GetIndexData().size(), __INDEX_TYPE__, &rmesh->Geometry->index[0]));
		}

		// Save Last Material and Mesh
		LastProgramUsed = Material->GetShader();
		LastMaterialPTR = Material;
		LastMaterialUsed = Material->GetInternalID();
		LastMeshRendered = rmesh->Geometry->GetInternalID();
		LastMeshRenderedPTR = rmesh;

		if (Material->depthBias)
			DisableDepthBias();
	}

	void IRenderer::EnableSorting()
	{
		sorting = true;
	}

	void IRenderer::DisableSorting()
	{
		sorting = false;
	}

	void IRenderer::ClearBufferBit(const uint32 Option)
	{
		glBufferOptions = 0;
		if (Option & Buffer_Bit::Color) glBufferOptions |= GL_COLOR_BUFFER_BIT;
		if (Option & Buffer_Bit::Depth) glBufferOptions |= GL_DEPTH_BUFFER_BIT;
		if (Option & Buffer_Bit::Stencil) glBufferOptions |= GL_STENCIL_BUFFER_BIT;

		bufferOptions = Option;
	}

	void IRenderer::DrawBackground()
	{
		if (BackgroundColorSet)
			GLCHECKER(glClearColor(BackgroundColor.x, BackgroundColor.y, BackgroundColor.z, BackgroundColor.w));
	}

	void IRenderer::DepthTest(const uint32 test)
	{
		depthTestMode = test;

		if (depthTesting)
		{
			GLCHECKER(glEnable(GL_DEPTH_TEST));

			switch (test)
			{
			case DepthTest::Always:
				GLCHECKER(glDepthFunc(GL_ALWAYS));
				break;
			case DepthTest::Equal:
				GLCHECKER(glDepthFunc(GL_EQUAL));
				break;
			case DepthTest::GEqual:
				GLCHECKER(glDepthFunc(GL_GEQUAL));
				break;
			case DepthTest::Greater:
				GLCHECKER(glDepthFunc(GL_GREATER));
				break;
			case DepthTest::LEqual:
				GLCHECKER(glDepthFunc(GL_LEQUAL));
				break;
			case DepthTest::Never:
				GLCHECKER(glDepthFunc(GL_NEVER));
				break;
			case DepthTest::NotEqual:
				GLCHECKER(glDepthFunc(GL_NOTEQUAL));
				break;
			case DepthTest::Less:
			default:
				GLCHECKER(glDepthFunc(GL_LESS));
				break;
			}
		}
		else {
			GLCHECKER(glDisable(GL_DEPTH_TEST));
		}
	}
	
	void IRenderer::DepthWrite()
	{
		if (depthWritting)
		{
			GLCHECKER(glDepthMask(GL_TRUE));
		}
		else {
			GLCHECKER(glDepthMask(GL_FALSE));
		}
	}
	void IRenderer::EnableClearDepthBuffer()
	{
		clearDepthBuffer = true;
	}
	void IRenderer::DisableClearDepthBuffer()
	{
		clearDepthBuffer = false;
	}
	void IRenderer::ClearDepthBuffer()
	{
#if !defined(GLES2)
		if (clearDepthBuffer) {
			GLCHECKER(glDepthMask(GL_TRUE));
			GLCHECKER(glClearDepth(1.f));
		}
#endif
	}
	void IRenderer::EnableStencil()
	{
		GLCHECKER(glEnable(GL_STENCIL_TEST));
	}
	void IRenderer::DisableStencil()
	{
		GLCHECKER(glDisable(GL_STENCIL_TEST));
	}
	void IRenderer::ClearStencilBuffer()
	{
		GLCHECKER(glClearStencil(0));
	}
	void IRenderer::StencilFunction(const uint32 func, const uint32 ref, const uint32 mask)
	{
		uint32 Func = GL_ALWAYS;
		switch (func)
		{
		case StencilFunc::Never:
			Func = GL_NEVER;
			break;
		case StencilFunc::Less:
			Func = GL_LESS;
			break;
		case StencilFunc::LEqual:
			Func = GL_LEQUAL;
			break;
		case StencilFunc::Greater:
			Func = GL_GREATER;
			break;
		case StencilFunc::GEqual:
			Func = GL_GEQUAL;
			break;
		case StencilFunc::Equal:
			Func = GL_EQUAL;
			break;
		case StencilFunc::Notequal:
			Func = GL_NOTEQUAL;
			break;
		default:
		case StencilFunc::Always:
			Func = GL_ALWAYS;
			break;
		}
		GLCHECKER(glStencilFunc(Func, ref, mask));
	}
	void IRenderer::StencilOperation(const uint32 sfail, const uint32 dpfail, const uint32 dppass)
	{
		uint32 Sfail = GL_KEEP;
		switch (sfail)
		{
		case StencilOp::Zero:
			Sfail = GL_KEEP;
			break;
		case StencilOp::Replace:
			Sfail = GL_REPLACE;
			break;
		case StencilOp::Incr:
			Sfail = GL_INCR;
			break;
		case StencilOp::Incr_Wrap:
			Sfail = GL_INCR_WRAP;
			break;
		case StencilOp::Decr:
			Sfail = GL_DECR;
			break;
		case StencilOp::Decr_Wrap:
			Sfail = GL_DECR_WRAP;
			break;
		case StencilOp::Invert:
			Sfail = GL_INVERT;
			break;
		default:
		case StencilOp::Keep:
			Sfail = GL_KEEP;
			break;
		};
		uint32 DPfail = GL_KEEP;
		switch (dpfail)
		{
		case StencilOp::Zero:
			DPfail = GL_KEEP;
			break;
		case StencilOp::Replace:
			DPfail = GL_REPLACE;
			break;
		case StencilOp::Incr:
			DPfail = GL_INCR;
			break;
		case StencilOp::Incr_Wrap:
			DPfail = GL_INCR_WRAP;
			break;
		case StencilOp::Decr:
			DPfail = GL_DECR;
			break;
		case StencilOp::Decr_Wrap:
			DPfail = GL_DECR_WRAP;
			break;
		case StencilOp::Invert:
			DPfail = GL_INVERT;
			break;
		default:
		case StencilOp::Keep:
			DPfail = GL_KEEP;
			break;
		};
		uint32 DPPASS = GL_KEEP;
		switch (dppass)
		{
		case StencilOp::Zero:
			DPPASS = GL_KEEP;
			break;
		case StencilOp::Replace:
			DPPASS = GL_REPLACE;
			break;
		case StencilOp::Incr:
			DPPASS = GL_INCR;
			break;
		case StencilOp::Incr_Wrap:
			DPPASS = GL_INCR_WRAP;
			break;
		case StencilOp::Decr:
			DPPASS = GL_DECR;
			break;
		case StencilOp::Decr_Wrap:
			DPPASS = GL_DECR_WRAP;
			break;
		case StencilOp::Invert:
			DPPASS = GL_INVERT;
			break;
		default:
		case StencilOp::Keep:
			DPPASS = GL_KEEP;
			break;
		};
		// Set Stencil Op
		GLCHECKER(glStencilOp(Sfail, DPfail, DPPASS));
	}
	void IRenderer::ColorMask(const f32 r, const f32 g, const f32 b, const f32 a)
	{
		GLCHECKER(glColorMask(r, g, b, a));
	}
	void IRenderer::ClearScreen()
	{
		GLCHECKER(glClear((GLuint)glBufferOptions));
	}

	void IRenderer::SetGlobalLight(const Vec4& Light)
	{
		GlobalLight = Light;
	}

	void IRenderer::EnableDepthBias(const Vec2& Bias)
	{
		if (!IsUsingDepthBias)
		{
			IsUsingDepthBias = true;
			GLCHECKER(glEnable(GL_POLYGON_OFFSET_FILL));    // enable polygon offset fill to combat "z-fighting"
		}
		GLCHECKER(glPolygonOffset(Bias.x, Bias.y));
	}

	void IRenderer::DisableDepthBias()
	{
		if (IsUsingDepthBias)
		{
			IsUsingDepthBias = false;
			GLCHECKER(glDisable(GL_POLYGON_OFFSET_FILL));
		}
	}

	void IRenderer::EnableBlending()
	{
		if (!blending)
		{
			// Enable Blending
			GLCHECKER(glEnable(GL_BLEND));
			blending = true;
		}
	}

	void IRenderer::DisableBlending()
	{
		if (blending)
		{
			// Disables Blending
			GLCHECKER(glDisable(GL_BLEND));
			blending = false;
			sfactor = dfactor = mode = -1;
		}
	}

	void IRenderer::BlendingFunction(const uint32 sfactor, const uint32 dfactor)
	{
		this->sfactor = sfactor;
		this->dfactor = dfactor;

		uint32 Sfactor = GL_ONE;
		switch (sfactor)
		{
		case BlendFunc::Zero:
			Sfactor = GL_ZERO;
			break;
		case BlendFunc::Src_Color:
			Sfactor = GL_SRC_COLOR;
			break;
		case BlendFunc::One_Minus_Src_Color:
			Sfactor = GL_ONE_MINUS_SRC_COLOR;
			break;
		case BlendFunc::Dst_Color:
			Sfactor = GL_DST_COLOR;
			break;
		case BlendFunc::One_Minus_Dst_Color:
			Sfactor = GL_ONE_MINUS_DST_COLOR;
			break;
		case BlendFunc::Src_Alpha:
			Sfactor = GL_SRC_ALPHA;
			break;
		case BlendFunc::One_Minus_Src_Alpha:
			Sfactor = GL_ONE_MINUS_SRC_ALPHA;
			break;
		case BlendFunc::Dst_Alpha:
			Sfactor = GL_DST_ALPHA;
			break;
		case BlendFunc::One_Minus_Dst_Alpha:
			Sfactor = GL_ONE_MINUS_DST_ALPHA;
			break;
		case BlendFunc::Constant_Color:
			Sfactor = GL_CONSTANT_COLOR;
			break;
		case BlendFunc::One_Minus_Constant_Color:
			Sfactor = GL_ONE_MINUS_CONSTANT_COLOR;
			break;
		case BlendFunc::Constant_Alpha:
			Sfactor = GL_CONSTANT_ALPHA;
			break;
		case BlendFunc::One_Minus_Constant_Alpha:
			Sfactor = GL_ONE_MINUS_CONSTANT_ALPHA;
			break;
		case BlendFunc::Src_Alpha_Saturate:
			Sfactor = GL_SRC_ALPHA_SATURATE;
			break;
#if !defined(GLES2)
		case BlendFunc::Src1_Color:
			Sfactor = GL_SRC1_COLOR;
			break;
		case BlendFunc::One_Minus_Src1_Color:
			Sfactor = GL_ONE_MINUS_SRC1_COLOR;
			break;
		case BlendFunc::Src1_Alpha:
			Sfactor = GL_SRC1_ALPHA;
			break;
		case BlendFunc::One_Minus_Src1_Alpha:
			Sfactor = GL_ONE_MINUS_SRC1_ALPHA;
			break;
#endif
		default:
		case BlendFunc::One:
			Sfactor = GL_ONE;
			break;
		}
		uint32 Dfactor = GL_ONE;
		switch (dfactor)
		{
		case BlendFunc::Zero:
			Dfactor = GL_ZERO;
			break;
		case BlendFunc::Src_Color:
			Dfactor = GL_SRC_COLOR;
			break;
		case BlendFunc::One_Minus_Src_Color:
			Dfactor = GL_ONE_MINUS_SRC_COLOR;
			break;
		case BlendFunc::Dst_Color:
			Dfactor = GL_DST_COLOR;
			break;
		case BlendFunc::One_Minus_Dst_Color:
			Dfactor = GL_ONE_MINUS_DST_COLOR;
			break;
		case BlendFunc::Src_Alpha:
			Dfactor = GL_SRC_ALPHA;
			break;
		case BlendFunc::One_Minus_Src_Alpha:
			Dfactor = GL_ONE_MINUS_SRC_ALPHA;
			break;
		case BlendFunc::Dst_Alpha:
			Dfactor = GL_DST_ALPHA;
			break;
		case BlendFunc::One_Minus_Dst_Alpha:
			Dfactor = GL_ONE_MINUS_DST_ALPHA;
			break;
		case BlendFunc::Constant_Color:
			Dfactor = GL_CONSTANT_COLOR;
			break;
		case BlendFunc::One_Minus_Constant_Color:
			Dfactor = GL_ONE_MINUS_CONSTANT_COLOR;
			break;
		case BlendFunc::Constant_Alpha:
			Dfactor = GL_CONSTANT_ALPHA;
			break;
		case BlendFunc::One_Minus_Constant_Alpha:
			Dfactor = GL_ONE_MINUS_CONSTANT_ALPHA;
			break;
		case BlendFunc::Src_Alpha_Saturate:
			Dfactor = GL_SRC_ALPHA_SATURATE;
			break;
#if !defined(GLES2)
		case BlendFunc::Src1_Color:
			Dfactor = GL_SRC1_COLOR;
			break;
		case BlendFunc::One_Minus_Src1_Color:
			Dfactor = GL_ONE_MINUS_SRC1_COLOR;
			break;
		case BlendFunc::Src1_Alpha:
			Dfactor = GL_SRC1_ALPHA;
			break;
		case BlendFunc::One_Minus_Src1_Alpha:
			Dfactor = GL_ONE_MINUS_SRC1_ALPHA;
			break;
#endif
		default:
		case BlendFunc::One:
			Dfactor = GL_ONE;
			break;
		}
		GLCHECKER(glBlendFunc(Sfactor, Dfactor));
	}
	void IRenderer::EnableScissorTest()
	{
		scissorTest = true;
	}
	void IRenderer::DisableScissorTest()
	{
		scissorTest = false;
	}
	void IRenderer::ScissorTestRect(const f32 x, const f32 y, const f32 width, const f32 height)
	{
		scissorTestX = x;
		scissorTestY = y;
		scissorTestWidth = width;
		scissorTestHeight = height;
	}
	void IRenderer::BlendingEquation(const uint32 mode)
	{
		this->mode = mode;

		uint32 Mode = GL_FUNC_ADD;
		switch (mode)
		{
		case BlendEq::Subtract:
			Mode = GL_FUNC_SUBTRACT;
			break;
		case BlendEq::Reverse_Subtract:
			Mode = GL_FUNC_REVERSE_SUBTRACT;
			break;
		default:
		case BlendEq::Add:
			Mode = GL_FUNC_ADD;
			break;
		}
		GLCHECKER(glBlendEquation(Mode));
	}
	void IRenderer::EnableWireFrame()
	{
#if !defined(GLES2)
		GLCHECKER(glPolygonMode(GL_FRONT_AND_BACK, GL_LINE));
#endif
	}

	void IRenderer::DisableWireFrame()
	{
#if !defined(GLES2)
		GLCHECKER(glPolygonMode(GL_FRONT_AND_BACK, GL_FILL));
#endif
	}

	void IRenderer::EnableClipPlane(const uint32 &numberOfClipPlanes)
	{
		ClipPlane = true;
		ClipPlaneNumber = numberOfClipPlanes;
	}
	void IRenderer::DisableClipPlane()
	{
		ClipPlane = false;
	}
	void IRenderer::SetClipPlane0(const Vec4 &clipPlane)
	{
		ClipPlanes[0] = clipPlane;
	}
	void IRenderer::SetClipPlane1(const Vec4 &clipPlane)
	{
		ClipPlanes[1] = clipPlane;
	}
	void IRenderer::SetClipPlane2(const Vec4 &clipPlane)
	{
		ClipPlanes[2] = clipPlane;
	}
	void IRenderer::SetClipPlane3(const Vec4 &clipPlane)
	{
		ClipPlanes[3] = clipPlane;
	}
	void IRenderer::SetClipPlane4(const Vec4 &clipPlane)
	{
		ClipPlanes[4] = clipPlane;
	}
	void IRenderer::SetClipPlane5(const Vec4 &clipPlane)
	{
		ClipPlanes[5] = clipPlane;
	}
	void IRenderer::SetClipPlane6(const Vec4 &clipPlane)
	{
		ClipPlanes[6] = clipPlane;
	}
	void IRenderer::SetClipPlane7(const Vec4 &clipPlane)
	{
		ClipPlanes[7] = clipPlane;
	}

	void IRenderer::SetBackground(const Vec4& Color)
	{
		BackgroundColor = Color;
		BackgroundColorSet = true;
	}
	void IRenderer::UnsetBackground()
	{
		BackgroundColorSet = false;
	}
	// Culling Methods
	void IRenderer::ActivateCulling(const uint32 cullingType)
	{
		culling = new FrustumCulling();
		IsCulling = true;
	}

	void IRenderer::DeactivateCulling()
	{
		IsCulling = false;
		delete culling;
	}

	bool IRenderer::CullingSphereTest(RenderingMesh* rmesh, GameObject* owner)
	{
		return culling->SphereInFrustum(owner->GetWorldPosition() + rmesh->Geometry->GetBoundingSphereCenter(), rmesh->Geometry->GetBoundingSphereRadius());
	}
	bool IRenderer::CullingBoxTest(RenderingMesh* rmesh, GameObject* owner)
	{
		// Get Bounding Values (min and max)
		Vec3 _min = rmesh->Geometry->GetBoundingMinValue();
		Vec3 _max = rmesh->Geometry->GetBoundingMaxValue();

		// Defining box vertex and Apply Transform
		Vec3 v[8];
		v[0] = owner->GetWorldTransformation() * _min;
		v[1] = owner->GetWorldTransformation() * Vec3(_min.x, _min.y, _max.z);
		v[2] = owner->GetWorldTransformation() * Vec3(_min.x, _max.y, _max.z);
		v[3] = owner->GetWorldTransformation() * _max;
		v[4] = owner->GetWorldTransformation() * Vec3(_min.x, _max.y, _min.z);
		v[5] = owner->GetWorldTransformation() * Vec3(_max.x, _min.y, _min.z);
		v[6] = owner->GetWorldTransformation() * Vec3(_max.x, _max.y, _min.z);
		v[7] = owner->GetWorldTransformation() * Vec3(_max.x, _min.y, _max.z);

		// Get new Min and Max
		Vec3 min = v[0];
		Vec3 max = v[0];
		for (uint32 i = 1; i<8; i++)
		{
			if (v[i].x<min.x) min.x = v[i].x;
			if (v[i].y<min.y) min.y = v[i].y;
			if (v[i].z<min.z) min.z = v[i].z;
			if (v[i].x>max.x) max.x = v[i].x;
			if (v[i].y>max.y) max.y = v[i].y;
			if (v[i].z>max.z) max.z = v[i].z;
		}
		// Build new Box
		AABox aabb = AABox(min, max);

		// Return test
		return culling->ABoxInFrustum(aabb);
	}
	bool IRenderer::CullingPointTest(RenderingMesh* rmesh, GameObject* owner)
	{
		return culling->PointInFrustum(owner->GetWorldPosition());
	}
	void IRenderer::UpdateCulling(const Matrix& ViewProjectionMatrix)
	{
		culling->Update(ViewProjectionMatrix);
	}

	void IRenderer::SendGlobalUniforms(RenderingMesh* rmesh, IMaterial* Material)
	{
		// Send Global Uniforms
		uint32 counter = 0;
		for (std::vector<Uniform>::iterator k = Material->GlobalUniforms.begin(); k != Material->GlobalUniforms.end(); k++)
		{
			if (rmesh->ShadersGlobalCache[Material->GetShader()][counter] == -2)
				rmesh->ShadersGlobalCache[Material->GetShader()][counter] = Shader::GetUniformLocation(Material->GetShader(), (*k).Name);

			if (rmesh->ShadersGlobalCache[Material->GetShader()][counter] >= 0)
			{
				switch ((*k).Usage)
				{
				case DataUsage::ViewMatrix:
					Shader::SendUniform((*k), &ViewMatrix, rmesh->ShadersGlobalCache[Material->GetShader()][counter]);
					break;
				case DataUsage::ProjectionMatrix:
					Shader::SendUniform((*k), &ProjectionMatrix, rmesh->ShadersGlobalCache[Material->GetShader()][counter]);
					break;
				case DataUsage::ViewProjectionMatrix:
					if (ViewProjectionMatrixIsDirty == true)
					{
						ViewProjectionMatrix = ProjectionMatrix * ViewMatrix;
						ViewProjectionMatrixIsDirty = false;
					}
					Shader::SendUniform((*k), &ViewProjectionMatrix, rmesh->ShadersGlobalCache[Material->GetShader()][counter]);
					break;
				case DataUsage::ViewMatrixInverse:
					if (ViewMatrixInverseIsDirty == true)
					{
						ViewMatrixInverse = ViewMatrix.Inverse();
						ViewMatrixInverseIsDirty = false;
					}
					Shader::SendUniform((*k), &ViewMatrixInverse, rmesh->ShadersGlobalCache[Material->GetShader()][counter]);
					break;
				case DataUsage::ProjectionMatrixInverse:
					if (ProjectionMatrixInverseIsDirty == true)
					{
						ProjectionMatrixInverse = ProjectionMatrix.Inverse();
						ProjectionMatrixInverseIsDirty = false;
					}
					Shader::SendUniform((*k), &ProjectionMatrixInverse, rmesh->ShadersGlobalCache[Material->GetShader()][counter]);
					break;
				case DataUsage::CameraPosition:
					Shader::SendUniform((*k), &CameraPosition, rmesh->ShadersGlobalCache[Material->GetShader()][counter]);
					break;
				case DataUsage::Timer:
				{
					f32 t = (f32)Timer;
					Shader::SendUniform((*k), &t, rmesh->ShadersGlobalCache[Material->GetShader()][counter]);
				}
				break;
				case DataUsage::GlobalAmbientLight:
					Shader::SendUniform((*k), &GlobalLight, rmesh->ShadersGlobalCache[Material->GetShader()][counter]);
					break;
				case DataUsage::Lights:
					if (Lights.size()>0)
						Shader::SendUniform((*k), &Lights[0], rmesh->ShadersGlobalCache[Material->GetShader()][counter], NumberOfLights);
					break;
				case DataUsage::NumberOfLights:
					Shader::SendUniform((*k), &NumberOfLights, rmesh->ShadersGlobalCache[Material->GetShader()][counter]);
					break;
				case DataUsage::NearFarPlane:
					Shader::SendUniform((*k), &NearFarPlane, rmesh->ShadersGlobalCache[Material->GetShader()][counter]);
					break;
				case DataUsage::ScreenDimensions:
				{
					Vec2 dim = Vec2(Width, Height);
					Shader::SendUniform((*k), &dim, rmesh->ShadersGlobalCache[Material->GetShader()][counter]);
				}
				break;
				case DataUsage::DirectionalShadowMap:
					if (DirectionalShadowMapsUnits.size()>0)
						Shader::SendUniform((*k), &DirectionalShadowMapsUnits[0], rmesh->ShadersGlobalCache[Material->GetShader()][counter], DirectionalShadowMapsUnits.size());
					break;
				case DataUsage::DirectionalShadowMatrix:
					if (DirectionalShadowMatrix.size()>0)
						Shader::SendUniform((*k), &DirectionalShadowMatrix[0], rmesh->ShadersGlobalCache[Material->GetShader()][counter], DirectionalShadowMatrix.size());
					break;
				case DataUsage::DirectionalShadowFar:
					Shader::SendUniform((*k), &DirectionalShadowFar, rmesh->ShadersGlobalCache[Material->GetShader()][counter]);
					break;
				case DataUsage::NumberOfDirectionalShadows:
					Shader::SendUniform((*k), &NumberOfDirectionalShadows, rmesh->ShadersGlobalCache[Material->GetShader()][counter]);
					break;
				case DataUsage::PointShadowMap:
					if (PointShadowMapsUnits.size()>0)
						Shader::SendUniform((*k), &PointShadowMapsUnits[0], rmesh->ShadersGlobalCache[Material->GetShader()][counter], PointShadowMapsUnits.size());
					break;
				case DataUsage::PointShadowMatrix:
					Shader::SendUniform((*k), &PointShadowMatrix[0], rmesh->ShadersGlobalCache[Material->GetShader()][counter], PointShadowMatrix.size());
					break;
				case DataUsage::NumberOfPointShadows:
					Shader::SendUniform((*k), &NumberOfPointShadows, rmesh->ShadersGlobalCache[Material->GetShader()][counter]);
					break;
				case DataUsage::SpotShadowMap:
					Shader::SendUniform((*k), &SpotShadowMapsUnits[0], rmesh->ShadersGlobalCache[Material->GetShader()][counter], SpotShadowMapsUnits.size());
					break;
				case DataUsage::SpotShadowMatrix:
					Shader::SendUniform((*k), &SpotShadowMatrix[0], rmesh->ShadersGlobalCache[Material->GetShader()][counter], SpotShadowMatrix.size());
					break;
				case DataUsage::NumberOfSpotShadows:
					Shader::SendUniform((*k), &NumberOfSpotShadows, rmesh->ShadersGlobalCache[Material->GetShader()][counter]);
					break;
				case DataUsage::ClipPlanes:
					Shader::SendUniform((*k), &ClipPlanes, rmesh->ShadersGlobalCache[Material->GetShader()][counter], ClipPlaneNumber);
					break;
				default:
					Shader::SendUniform((*k), rmesh->ShadersGlobalCache[Material->GetShader()][counter]);
					break;
				}
			}
			counter++;
		}
	}

	void IRenderer::SendUserUniforms(RenderingMesh* rmesh, IMaterial* Material)
	{
		// User Specific Uniforms
		uint32 counter = 0;
		for (std::map<StringID, Uniform>::iterator k = Material->UserUniforms.begin(); k != Material->UserUniforms.end(); k++)
		{
			if (rmesh->ShadersUserCache[Material->GetShader()][counter] == -2)
				rmesh->ShadersUserCache[Material->GetShader()][counter] = Shader::GetUniformLocation(Material->GetShader(), (*k).second.Name);

			if (rmesh->ShadersUserCache[Material->GetShader()][counter] >= 0)
				Shader::SendUniform((*k).second, rmesh->ShadersUserCache[Material->GetShader()][counter]);

			counter++;
		}
	}

	void IRenderer::SendModelUniforms(RenderingMesh* rmesh, IMaterial* Material)
	{
		uint32 counter = 0;
		for (std::vector<Uniform>::iterator k = Material->ModelUniforms.begin(); k != Material->ModelUniforms.end(); k++)
		{
			if (rmesh->ShadersModelCache[Material->GetShader()][counter] == -2)
				rmesh->ShadersModelCache[Material->GetShader()][counter] = Shader::GetUniformLocation(Material->GetShader(), (*k).Name);
				

			if (rmesh->ShadersModelCache[Material->GetShader()][counter] >= 0)
			{
				switch ((*k).Usage)
				{
				case DataUsage::ModelMatrix:
					Shader::SendUniform((*k), &ModelMatrix, rmesh->ShadersModelCache[Material->GetShader()][counter]);
					break;
				case DataUsage::NormalMatrix:
					if (NormalMatrixIsDirty == true)
					{
						NormalMatrix = (ViewMatrix*ModelMatrix);
						NormalMatrixIsDirty = false;
					}
					Shader::SendUniform((*k), &NormalMatrix, rmesh->ShadersModelCache[Material->GetShader()][counter]);
					break;
				case DataUsage::ModelViewMatrix:
					if (ModelViewMatrixIsDirty == true)
					{
						ModelViewMatrix = ViewMatrix*ModelMatrix;
						ModelViewMatrixIsDirty = false;
					}
					Shader::SendUniform((*k), &ModelViewMatrix, rmesh->ShadersModelCache[Material->GetShader()][counter]);
					break;
				case DataUsage::ModelViewProjectionMatrix:
					if (ModelViewProjectionMatrixIsDirty == true)
					{
						ModelViewProjectionMatrix = ProjectionMatrix*ViewMatrix*ModelMatrix;
						ModelViewProjectionMatrixIsDirty = false;
					}
					Shader::SendUniform((*k), &ModelViewProjectionMatrix, rmesh->ShadersModelCache[Material->GetShader()][counter]);
					break;
				case DataUsage::ModelMatrixInverse:
					if (ModelMatrixInverseIsDirty == true)
					{
						ModelMatrixInverse = ModelMatrix.Inverse();
						ModelMatrixInverseIsDirty = false;
					}
					Shader::SendUniform((*k), &ModelMatrixInverse, rmesh->ShadersModelCache[Material->GetShader()][counter]);
					break;
				case DataUsage::ModelViewMatrixInverse:
					if (ModelViewMatrixInverseIsDirty == true)
					{
						ModelViewMatrixInverse = (ViewMatrix*ModelMatrix).Inverse();
						ModelViewMatrixInverseIsDirty = false;
					}
					Shader::SendUniform((*k), &ModelViewMatrixInverse, rmesh->ShadersModelCache[Material->GetShader()][counter]);
					break;
				case DataUsage::ModelMatrixInverseTranspose:
					if (ModelMatrixInverseTransposeIsDirty == true)
					{
						ModelMatrixInverseTranspose = ModelMatrixInverse.Transpose();
						ModelMatrixInverseTransposeIsDirty = false;
					}
					Shader::SendUniform((*k), &ModelMatrixInverseTranspose, rmesh->ShadersModelCache[Material->GetShader()][counter]);
					break;
				case DataUsage::Skinning:
				{
					Shader::SendUniform((*k), &rmesh->SkinningBones[0], rmesh->ShadersModelCache[Material->GetShader()][counter], rmesh->SkinningBones.size());
				}
				break;
				}
			}
			counter++;
		}
	}

	void IRenderer::BindMesh(RenderingMesh* rmesh, IMaterial* material)
	{
		if (rmesh->ShadersAttributesCache.find(material->GetShader()) == rmesh->ShadersAttributesCache.end())
		{
			// Reset Attribute IDs
			for (std::vector<AttributeArray*>::iterator i = rmesh->Geometry->Attributes.begin(); i != rmesh->Geometry->Attributes.end(); i++)
			{
				std::vector<int32> attribs;
				for (std::vector<VertexAttribute*>::iterator k = (*i)->Attributes.begin(); k != (*i)->Attributes.end(); k++)
				{
					attribs.push_back(Shader::GetAttributeLocation(material->GetShader(), (*k)->Name));
				}
				rmesh->ShadersAttributesCache[material->GetShader()].push_back(attribs);
			}
			for (std::vector<Uniform>::iterator k = material->GlobalUniforms.begin(); k != material->GlobalUniforms.end(); k++)
			{
				rmesh->ShadersGlobalCache[material->GetShader()].push_back(Shader::GetUniformLocation(material->GetShader(), (*k).Name));
			}
			for (std::vector<Uniform>::iterator k = material->ModelUniforms.begin(); k != material->ModelUniforms.end(); k++)
			{
				rmesh->ShadersModelCache[material->GetShader()].push_back(Shader::GetUniformLocation(material->GetShader(), (*k).Name));
			}
			for (std::map<StringID, Uniform>::iterator k = material->UserUniforms.begin(); k != material->UserUniforms.end(); k++)
			{
				rmesh->ShadersUserCache[material->GetShader()].push_back(Shader::GetUniformLocation(material->GetShader(), (*k).second.Name));
			}
		}
	}
	void IRenderer::UnbindMesh(RenderingMesh* rmesh, IMaterial* material)
	{
		// Disable Attributes
		if (rmesh->Geometry->Attributes.size()>0)
		{
			uint32 counterBuffers = 0;
			for (std::vector<AttributeArray*>::iterator k = rmesh->Geometry->Attributes.begin(); k != rmesh->Geometry->Attributes.end(); k++)
			{
				uint32 counter = 0;
				for (std::vector<VertexAttribute*>::iterator l = (*k)->Attributes.begin(); l != (*k)->Attributes.end(); l++)
				{
					// If exists in shader
					if (rmesh->ShadersAttributesCache[material->GetShader()][counterBuffers][counter] >= 0)
					{
						GLCHECKER(glDisableVertexAttribArray(rmesh->ShadersAttributesCache[material->GetShader()][counterBuffers][counter]));
					}
					counter++;
				}
				counterBuffers++;
			}
			if (rmesh->Geometry->GetGeometryType() == GeometryType::BUFFER)
				GLCHECKER(glBindBuffer(GL_ARRAY_BUFFER, 0));
		}
	}

	void IRenderer::BindShadowMaps(IMaterial* material)
	{
		// Bind Shadows Textures
		if (material->IsCastingShadows())
		{
			DirectionalShadowMapsUnits.clear();
			for (std::vector<Texture*>::iterator i = DirectionalShadowMapsTextures.begin(); i != DirectionalShadowMapsTextures.end(); i++)
			{
				(*i)->Bind();
				DirectionalShadowMapsUnits.push_back(Texture::GetLastBindedUnit());
			}

			PointShadowMapsUnits.clear();
			for (std::vector<Texture*>::iterator i = PointShadowMapsTextures.begin(); i != PointShadowMapsTextures.end(); i++)
			{
				(*i)->Bind();
				PointShadowMapsUnits.push_back(Texture::GetLastBindedUnit());
			}

			SpotShadowMapsUnits.clear();
			for (std::vector<Texture*>::iterator i = SpotShadowMapsTextures.begin(); i != SpotShadowMapsTextures.end(); i++)
			{
				(*i)->Bind();
				SpotShadowMapsUnits.push_back(Texture::GetLastBindedUnit());
			}
		}
	}

	void IRenderer::UnbindShadowMaps(IMaterial* material)
	{
		// Unbind Shadows Textures
		if (material->IsCastingShadows())
		{
			// Spot Lights
			for (std::vector<Texture*>::reverse_iterator i = SpotShadowMapsTextures.rbegin(); i != SpotShadowMapsTextures.rend(); i++)
			{
				(*i)->Unbind();
			}
			// Point Lights
			for (std::vector<Texture*>::reverse_iterator i = PointShadowMapsTextures.rbegin(); i != PointShadowMapsTextures.rend(); i++)
			{
				(*i)->Unbind();
			}
			// Directional Lights
			for (std::vector<Texture*>::reverse_iterator i = DirectionalShadowMapsTextures.rbegin(); i != DirectionalShadowMapsTextures.rend(); i++)
			{
				(*i)->Unbind();
			}
		}
	}

	void IRenderer::SendAttributes(RenderingMesh* rmesh, IMaterial* material)
	{
		// Check if custom Attributes exists
		if (rmesh->Geometry->Attributes.size()>0)
		{
			if (rmesh->Geometry->GetGeometryType() == GeometryType::BUFFER)
			{

				// VBO
				uint32 counterBuffers = 0;
				for (std::vector<AttributeArray*>::iterator k = rmesh->Geometry->Attributes.begin(); k != rmesh->Geometry->Attributes.end(); k++)
				{

					AttributeBuffer* bf = (AttributeBuffer*)(*k);

					// Bind VAO
					GLCHECKER(glBindBuffer(GL_ARRAY_BUFFER, bf->Buffer->ID));

					// Get Struct Data
					if (bf->attributeSize == 0)
					{
						for (std::vector<VertexAttribute*>::iterator l = (*k)->Attributes.begin(); l != (*k)->Attributes.end(); l++)
						{
							bf->attributeSize += (*l)->byteSize;
						}
					}

					// Counter
					uint32 counter = 0;
					for (std::vector<VertexAttribute*>::iterator l = (*k)->Attributes.begin(); l != (*k)->Attributes.end(); l++)
					{
						// Check if is not set
						if (rmesh->ShadersAttributesCache[material->GetShader()][counterBuffers][counter] == -2)
						{
							// set VAO ID
							rmesh->ShadersAttributesCache[material->GetShader()][counterBuffers][counter] = Shader::GetAttributeLocation(material->GetShader(), (*l)->Name);

						}
						// If exists in shader
						if (rmesh->ShadersAttributesCache[material->GetShader()][counterBuffers][counter] >= 0)
						{
							AttributeBuffer* bf = (AttributeBuffer*)(*k);
							GLCHECKER(glVertexAttribPointer(
								rmesh->ShadersAttributesCache[material->GetShader()][counterBuffers][counter],
								Buffer::Attribute::GetTypeCount((*l)->Type),
								Buffer::Attribute::GetType((*l)->Type),
								GL_FALSE,
								bf->attributeSize,
								BUFFER_OFFSET((*l)->Offset)
								));

							// Enable Attribute
							GLCHECKER(glEnableVertexAttribArray(rmesh->ShadersAttributesCache[material->GetShader()][counterBuffers][counter]));
						}
						counter++;
					}
					counterBuffers++;
				}
			}
			else {

				// Arrays
				uint32 counterBuffers = 0;
				for (std::vector<AttributeArray*>::iterator k = rmesh->Geometry->Attributes.begin(); k != rmesh->Geometry->Attributes.end(); k++)
				{

					// Counter
					uint32 counter = 0;
					for (std::vector<VertexAttribute*>::iterator l = (*k)->Attributes.begin(); l != (*k)->Attributes.end(); l++)
					{
						// Check if is not set
						if (rmesh->ShadersAttributesCache[material->GetShader()][counterBuffers][counter] == -2)
						{
							// set VAO ID
							rmesh->ShadersAttributesCache[material->GetShader()][counterBuffers][counter] = Shader::GetAttributeLocation(material->GetShader(), (*l)->Name);

						}
						// If exists in shader
						if (rmesh->ShadersAttributesCache[material->GetShader()][counterBuffers][counter] >= 0)
						{

							GLCHECKER(glEnableVertexAttribArray(rmesh->ShadersAttributesCache[material->GetShader()][counterBuffers][counter]));
							GLCHECKER(glVertexAttribPointer(
								rmesh->ShadersAttributesCache[material->GetShader()][counterBuffers][counter],
								Buffer::Attribute::GetTypeCount((*l)->Type),
								Buffer::Attribute::GetType((*l)->Type),
								GL_FALSE,
								0,
								&(*l)->Data[0]
								));

							// Enable Attribute
							GLCHECKER(glEnableVertexAttribArray(rmesh->ShadersAttributesCache[material->GetShader()][counterBuffers][counter]));
						}
						counter++;
					}
					counterBuffers++;
				}
			}
		}
	}

};
