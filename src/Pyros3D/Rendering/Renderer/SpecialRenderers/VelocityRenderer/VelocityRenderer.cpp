#include <cstdlib>
//============================================================================
// Name        : VelocityRenderer.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Dynamic Cube Map aka Environment Map
//============================================================================

#include <cmath>
#include <unordered_set>
#include <Pyros3D/Rendering/RenderState.h>
#include <cstring>
#include <Pyros3D/Rendering/Renderer/SpecialRenderers/VelocityRenderer/VelocityRenderer.h>
#include <Pyros3D/Other/PyrosGL.h>

namespace p3d {

	VelocityRenderer::VelocityRenderer(const uint32 Width, const uint32 Height) : IRenderer(Width, Height)
	{

		echo("TRACE: Velocity Renderer Created");

		// Don't frustum-cull the velocity pass: a mismatched VP (or a
		// shared-UBO hangover from Forward PreRender shadows) can drop
		// every mesh, leaving a cleared velocity map and an identity blur
		// that looks like "motion blur does nothing" on GL.
		//ActivateCulling(CullingMode::FrustumCulling);

		// Create Texture, Frame Buffer and Set the Texture as Attachment
		velocityMap = new Texture();
		// RGBA16F (not RG16F): macOS GL has been unreliable with RG16F
		// colour attachments + out vec2; PostEffectsManager already uses
		// RGBA16F successfully on both backends. Velocity still lives in .rg.
		velocityMap->CreateEmptyTexture(TextureType::Texture, TextureDataType::RGBA16F, Width, Height, false);
		velocityMap->SetRepeat(TextureRepeat::ClampToEdge, TextureRepeat::ClampToEdge, TextureRepeat::ClampToEdge);
		// Nearest: a linear blend of two velocities is a direction nothing
		// moved in, and it shows up as a kink in the smear.
		velocityMap->SetMinMagFilter(TextureFilter::Nearest, TextureFilter::Nearest);

		// Depth as a texture (not a renderbuffer) - same multi-attach path
		// as PostEffectsManager / Deferred G-buffer. Keeps Vulkan's
		// pending-attachment finalize path consistent across backends.
		depthMap = new Texture();
		depthMap->CreateEmptyTexture(TextureType::Texture, TextureDataType::DepthComponent, Width, Height, false);
		depthMap->SetRepeat(TextureRepeat::ClampToEdge, TextureRepeat::ClampToEdge, TextureRepeat::ClampToEdge);
		// See PostEffectsManager::Init() - Linear on a depth texture is
		// "unloadable" on Apple GL and samples as zero.
		depthMap->SetMinMagFilter(TextureFilter::Nearest, TextureFilter::Nearest);

		// Initialize Frame Buffer
		fbo = new FrameBuffer();
		fbo->SetDebugName("Velocity");
		fbo->Init(FrameBufferAttachmentFormat::Depth_Attachment, TextureType::Texture, depthMap);
		fbo->AddAttach(FrameBufferAttachmentFormat::Color_Attachment0, TextureType::Texture, velocityMap);
		velocityMaterial = new GenericShaderMaterial(ShaderUsage::VelocityRendering);

		// Default View Port Init Values
		viewPortStartX = viewPortStartY = 0;
		viewPortEndX = viewPortEndY = 0;

	}

	void VelocityRenderer::Resize(const uint32 &Width, const uint32 &Height)
	{
		fbo->Resize(Width, Height);
		// A velocity from the old size lands on the wrong texel.
		havePrevious = false;
		cameraReproject = Matrix();

		IRenderer::Resize(Width, Height);
	}

	VelocityRenderer::~VelocityRenderer()
	{
		delete velocityMap;
		delete depthMap;
		delete fbo;
		delete velocityMaterial;
	}

	// Whether an object's matrix changed by more than a body at rest under
	// physics trembles: a tenth of a millimetre, or as much turn at a metre.
	static bool MovedVisibly(const Matrix &before, const Matrix &now)
	{
		for (int i = 0; i < 16; i++)
			if (fabsf(before.m[i] - now.m[i]) > 1e-4f) return true;
		return false;
	}

	void VelocityRenderer::RenderVelocityMap(const p3d::Projection &Projection, GameObject* Camera, SceneGraph* Scene)
	{

		InitRender();

		this->Scene = Scene;
		this->Camera = Camera;
		this->projection = Projection;

		// Our own previous frame, not whatever Prv* currently holds. Those
		// belong to whichever renderer ran last (the scene, then the UI),
		// so reading them made a camera move and an object move disagree,
		// and the first frame after a resize smeared the whole picture.
		const Matrix curP = projection.m;
		const Matrix curV = Camera->GetWorldTransformation().Inverse();
		const Matrix prvP = havePrevious ? previousProjection : curP;
		const Matrix prvV = havePrevious ? previousView : curV;
		// Same matrices the velocity shader multiplies, including the
		// backend's clip-space fix, so a sky texel reprojected with this
		// matches a mesh texel written by that shader.
		IRenderDevice &device = GetActiveRenderDevice();
		const Matrix curVP = device.TranslateProjectionMatrix(curP) * curV;
		const Matrix prvVP = device.TranslateProjectionMatrix(prvP) * prvV;
		cameraReproject = havePrevious ? prvVP * curVP.Inverse() : Matrix();
		previousProjection = curP;
		previousView = curV;
		havePrevious = true;

		// Universal Cache
		PrvProjectionMatrix = prvP;
		ProjectionMatrix = curP;
		NearFarPlane = Vec2(projection.Near, projection.Far);

		// View Matrix and Position
		PrvViewMatrix = prvV;
		ViewMatrix = curV;
		CameraPosition = Camera->GetWorldPosition();

		// Flags
		ViewMatrixInverseIsDirty = true;
		ProjectionMatrixInverseIsDirty = true;
		ViewProjectionMatrixIsDirty = true;

		// Sort ourselves - don't rely on a prior Forward PreRender having
		// filled Scene's sorted list (shadow-less / empty-light paths skip
		// that, and CubemapRenderer's old "reuse last sort" assumption is
		// fragile for DemoLauncher).
		// Drawing only what moved, the order things are drawn in is of no
		// account and the scene's own list of its meshes will do: sorting all
		// seven thousand of a game's island to pick out thirty was most of
		// what this pass cost.
		// What has been moved since this pass last ran.
		std::unordered_set<GameObject*> movedNow;
		bool everything = false;
		{
			std::vector<GameObject*> moved;
			if (RenderState::MovedSince(movedSeq, moved)) movedNow.insert(moved.begin(), moved.end());
			else everything = true;         // more than the log remembers
			movedSeq = RenderState::MovedCount();
		}

		// ...and their meshes. From the list the scene's renderer keeps, where
		// each object's are looked up (and are known to be there still): this
		// pass went through every mesh the scene has, seven thousand on a
		// game's island, to find the thirty that had moved - a sixteenth of the
		// frame. Without such a list, as before.
		if (!dynamicOnly) rmesh = GroupAndSortAssets(Scene, Camera);
		else
		{
			const FrameList* kept = everything ? NULL : CurrentWorldList(Scene);
			if (kept == NULL) rmesh = Scene->GetRenderingMeshes();
			else
			{
				rmesh.clear();
				const size_t nOpaque = kept->opaque.size();
				for (std::unordered_set<GameObject*>::const_iterator o = movedNow.begin(); o != movedNow.end(); ++o)
				{
					std::unordered_map<GameObject*, std::vector<uint32> >::const_iterator at = kept->where.find(*o);
					if (at == kept->where.end()) continue;
					for (size_t e = 0; e < at->second.size(); e++)
						if (at->second[e] < nOpaque) rmesh.push_back(kept->opaque[at->second[e]]);       // (what is seen through is not drawn here)
				}
			}
		}

		// Cleared even with nothing to draw: TAA reads this map every frame,
		// and an empty scene would otherwise leave it holding whatever the
		// last scene with meshes wrote.
		{

			// Save Time
			Timer = Scene->GetTime();

			// Zero velocity where nothing is drawn. The clear colour is the
			// scene background's, and Vulkan/Metal clear on Bind - a grey sky
			// read back as a 0.2 UV/frame velocity.
			IRenderDevice &dev = GetActiveRenderDevice();
			const Vec4 sceneClear = dev.GetClearColor();
			dev.SetClearColor(Vec4(0.f, 0.f, 0.f, 0.f));

			// Bind FBO (color + depth were attached once in the ctor).
			// Re-AddAttach every frame breaks Vulkan: Bind already opens a
			// multi-attachment render pass, and AttachFramebufferTexture2D
			// with wasAlreadyBound=true builds a 1-attachment framebuffer
			// against it (MoltenVK EXC_BAD_ACCESS in image-view setup).
			fbo->Bind();

			// Set ViewPort
			if (viewPortEndX == 0 || viewPortEndY == 0)
			{
				viewPortEndX = Width;
				viewPortEndY = Height;
			}

			_SetViewPort(viewPortStartX, viewPortStartY, viewPortEndX, viewPortEndY);

			// Clear Screen
			ClearBufferBit(Buffer_Bit::Depth | Buffer_Bit::Color);
			EnableClearDepthBuffer();
			ClearDepthBuffer();
			ClearScreen();

			// Render Scene with Objects Material
			// How many pixels a unit of size covers at a unit's distance, and the
			// radius on screen below which a mover is left to the camera's motion.
			const f32 pixelsPerUnitAtOne = fabsf(projection.m.m[5]) * 0.5f * (f32)Height;
			static const f32 kSmallestMoverPixels = 14.f;
			// (whether a mesh is drawn here: see the notes inside)
			auto wanted = [&](RenderingMesh* mesh) -> bool {
				GameObject* owner = mesh->renderingComponent->GetOwner();
				if (owner == NULL) return false;
				if (!(mesh->renderingComponent->IsActive() && mesh->Active == true)) return false;
				// (SetDynamicOnly) Only what moves by itself: an object whose place in the
				// world changed since the last frame, or one with bones.
				// Everything else stood still, and whoever reads this map
				// (TAA, motion blur) works its motion out from depth and
				// the two cameras. This pass used to draw the whole scene
				// again, object by object - four thousand draws and two
				// million triangles a frame on a game's island.
				// (bones alone no longer count: a figure standing and breathing has
				// not moved, and this pass draws no pose anyway - see the note on it)
				const bool bones = !dynamicOnly && !mesh->SkinningBones.empty();
				if (dynamicOnly && mesh->Material && mesh->Material->IsTransparent()) return false;
				// (moved: its matrix was worked out again since this pass
				// last looked - RenderState's log - AND came out different.
				// The matrix kept from "last frame" means nothing on an
				// object the scene has stopped updating.)
				if (dynamicOnly && !bones && ((!everything && movedNow.find(owner) == movedNow.end())
					|| !MovedVisibly(owner->GetPrvWorldTransformation(), owner->GetWorldTransformation()))) return false;
				// (...and big enough on screen for its own motion to matter:
				// a bot two hundred metres off is a dozen pixels, and
				// settled by the camera's motion alone it looks no
				// different. Drawing every mover at any distance was a
				// millisecond of the frame.)
				if (dynamicOnly)
				{
					const f32 radius = owner->GetBoundingSphereRadiusWorldSpace();
					const f32 distance = (owner->GetWorldPosition() - CameraPosition).magnitude();
					if (distance > radius && radius / distance * pixelsPerUnitAtOne < kSmallestMoverPixels) return false;
				}
				return true;
			};
			uint32 drawn = 0;
			for (std::vector<RenderingMesh*>::iterator k = rmesh.begin(); k != rmesh.end(); k++)
			{
				if (!wanted(*k)) continue;
				RenderObject((*k), (*k)->renderingComponent->GetOwner(), velocityMaterial);
				drawn++;
			}
			// PYROS_VERIFY_VELOCITY=1: what was drawn held against what going
			// through every mesh of the scene's world would have drawn.
			static const bool verify = std::getenv("PYROS_VERIFY_VELOCITY") != NULL;
			if (verify && dynamicOnly)
			{
				static uint64 frames = 0, wrong = 0, total = 0;
				uint32 all = 0;
				const std::vector<RenderingMesh*> &every = Scene->GetRenderingMeshes();
				for (size_t k = 0; k < every.size(); k++)
					if (every[k]->renderingComponent->GetRenderLayer() == RenderLayer::World && wanted(every[k])) all++;
				frames++; total += drawn;
				if (all != drawn && ++wrong <= 20) fprintf(stderr, "[velocity] WRONG: %u drawn, %u wanted\n", drawn, all);
				if (frames % 600 == 0) fprintf(stderr, "[velocity] %llu frames, %llu wrong, %llu meshes drawn\n", (unsigned long long)frames, (unsigned long long)wrong, (unsigned long long)total);
			}

			fbo->UnBind();
			dev.SetClearColor(sceneClear);

			EndRender();
		}
	}

	Texture* VelocityRenderer::GetTexture()
	{
		return velocityMap;
	}

};
