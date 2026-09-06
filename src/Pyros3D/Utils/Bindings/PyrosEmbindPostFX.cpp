//============================================================================
// Name        : PyrosEmbindPostFX.cpp
// Description : Embind post-effects + motion blur / SSAO / DOF helpers.
//============================================================================

#if defined(__EMSCRIPTEN__) || defined(EMSCRIPTEN)

#include <emscripten/bind.h>

#include <Pyros3D/Rendering/PostEffects/PostEffectsManager.h>
#include <Pyros3D/Rendering/PostEffects/Effects/IEffect.h>
#include <Pyros3D/Rendering/PostEffects/Effects/BloomEffect.h>
#include <Pyros3D/Rendering/PostEffects/Effects/BlurXEffect.h>
#include <Pyros3D/Rendering/PostEffects/Effects/BlurYEffect.h>
#include <Pyros3D/Rendering/PostEffects/Effects/ResizeEffect.h>
#include <Pyros3D/Rendering/PostEffects/Effects/RTTDebug.h>
#include <Pyros3D/Rendering/PostEffects/Effects/SSAOEffect.h>
#include <Pyros3D/Rendering/PostEffects/Effects/VignetteEffect.h>
#include <Pyros3D/Rendering/PostEffects/Effects/MotionBlurEffect.h>
#include <Pyros3D/Rendering/PostEffects/Effects/TonemapEffect.h>
#include <Pyros3D/Rendering/PostEffects/Effects/BlurSSAOEffect.h>
#include <Pyros3D/Rendering/PostEffects/Effects/SSAOCompositeEffect.h>
#include <Pyros3D/Rendering/PostEffects/Effects/DepthOfFieldEffect.h>
#include <Pyros3D/Rendering/Renderer/SpecialRenderers/VelocityRenderer/VelocityRenderer.h>
#include <Pyros3D/Assets/Texture/Texture.h>
#include <Pyros3D/Core/Logs/Log.h>
#include <Pyros3D/Core/Projection/Projection.h>
#include <Pyros3D/GameObjects/GameObject.h>
#include <Pyros3D/SceneGraph/SceneGraph.h>
#include <Pyros3D/Core/Math/Math.h>

#include <memory>
#include <string>

using namespace emscripten;
using namespace p3d;
using namespace p3d::Math;

namespace {

	static SSAOEffect *g_ssao = nullptr;
	static BlurSSAOEffect *g_ssaoBlur = nullptr;
	static VelocityRenderer *g_velocityRenderer = nullptr;
	static MotionBlurEffect *g_motionBlur = nullptr;

	void AddPostEffect(PostEffectsManager &m, const std::string &name, int width, int height)
	{
		if (name == "tonemap")
			m.AddEffect(new TonemapEffect(RTT::Color, width, height));
		else
			echo("ERROR: addPostEffect - unknown effect '" + name + "'");
	}

	void ClearPostEffectHandles()
	{
		g_ssao = nullptr;
		g_ssaoBlur = nullptr;
		g_motionBlur = nullptr;
		Texture::ResetUnitCounter();
	}

	void DestroyMotionBlurVelocity()
	{
		delete g_velocityRenderer;
		g_velocityRenderer = nullptr;
	}

	void BuildMotionBlurPostChain(PostEffectsManager &m, int width, int height)
	{
		delete g_velocityRenderer;
		g_velocityRenderer = new VelocityRenderer((uint32)width, (uint32)height);
		g_motionBlur = new MotionBlurEffect(RTT::Color, g_velocityRenderer->GetTexture(), (uint32)width, (uint32)height);
		g_motionBlur->SetTargetFPS(60.0f);
		g_motionBlur->SetCurrentFPS(60.0f);
		m.AddEffect(g_motionBlur);
	}

	void MotionBlurRenderVelocity(const Projection &proj, const std::shared_ptr<GameObject> &cam, SceneGraph &scene)
	{
		if (g_velocityRenderer && cam)
			g_velocityRenderer->RenderVelocityMap(proj, cam.get(), &scene);
	}

	void MotionBlurSetFPS(float currentFps)
	{
		if (g_motionBlur)
			g_motionBlur->SetCurrentFPS(currentFps);
	}

	void MotionBlurResize(int width, int height)
	{
		if (g_velocityRenderer)
			g_velocityRenderer->Resize((uint32)width, (uint32)height);
		if (g_motionBlur)
			g_motionBlur->Resize((uint32)width, (uint32)height);
	}

	bool MotionBlurActive()
	{
		return g_velocityRenderer != nullptr && g_motionBlur != nullptr;
	}

	void BuildSSAOPostChain(PostEffectsManager &m, int width, int height)
	{
		g_ssao = new SSAOEffect(RTT::Depth, width, height);
		g_ssao->SetRadius(0.2f);
		g_ssao->SetStrength(1.5f);
		g_ssao->SetTreshOld(2.0f);
		g_ssao->SetScale(1.0f);
		g_ssaoBlur = new BlurSSAOEffect(RTT::LastRTT, width, height);
		m.AddEffect(g_ssao);
		m.AddEffect(g_ssaoBlur);
		m.AddEffect(new SSAOCompositeEffect(RTT::Color, RTT::LastRTT, width, height));
	}

	void SsaoSetViewMatrix(const Matrix &mat)
	{
		if (g_ssao) g_ssao->SetViewMatrix(mat);
	}

	void SsaoSetParams(float radius, float strength, float threshold, float scale, float blurIntensity)
	{
		if (g_ssao)
		{
			g_ssao->SetRadius(radius);
			g_ssao->SetStrength(strength);
			g_ssao->SetTreshOld(threshold);
			g_ssao->SetScale(scale);
		}
		if (g_ssaoBlur) g_ssaoBlur->SetIntensity(blurIntensity);
	}

	void BuildDOFPostChain(PostEffectsManager &m, int width, int height)
	{
		auto *blurX = new BlurXEffect(RTT::Color, width, height);
		auto *blurY = new BlurYEffect(RTT::LastRTT, width, height);
		Texture *fullRes = blurY->GetTexture();
		const int lw = (int)(width * 0.25f), lh = (int)(height * 0.25f);
		auto *resize = new ResizeEffect(RTT::Color, lw, lh);
		auto *blurXlow = new BlurXEffect(RTT::LastRTT, lw, lh);
		auto *blurYlow = new BlurYEffect(RTT::LastRTT, lw, lh);
		Texture *lowRes = blurYlow->GetTexture();
		m.AddEffect(blurX);
		m.AddEffect(blurY);
		m.AddEffect(resize);
		m.AddEffect(blurXlow);
		m.AddEffect(blurYlow);
		m.AddEffect(new DepthOfFieldEffect(lowRes, fullRes, width, height));
	}

	void PostEffects_Process(PostEffectsManager &m, Projection &proj)
	{
		m.ProcessPostEffects(&proj);
	}

	// PostEffectsManager takes ownership of raw IEffect*.
	void PostEffects_AddEffect(PostEffectsManager &m, IEffect *effect)
	{
		m.AddEffect(effect);
	}

} // namespace

namespace p3d {
	void PyrosEmbindPostFXForceLink() {}
}

EMSCRIPTEN_BINDINGS(pyros3d_postfx)
{
	class_<IEffect>("IEffect")
		.constructor<int, int>()
		.function("compileShaders", &IEffect::CompileShaders)
		.function("resize", &IEffect::Resize)
		.function("getWidth", &IEffect::GetWidth)
		.function("getHeight", &IEffect::GetHeight)
		.function("getTexture", &IEffect::GetTexture, allow_raw_pointers());

	// See BloomEffect.h - bloom is a bright pass and a composite now, with a
	// blur between them. PostEffectChain::AppendBuiltIn("Bloom") assembles
	// all four; these are here because the pieces are useful alone.
	class_<BloomBrightPassEffect, base<IEffect>>("BloomBrightPassEffect")
		.constructor<int, int, int>()
		.function("setThreshold", &BloomBrightPassEffect::SetThreshold)
		.function("setKnee", &BloomBrightPassEffect::SetKnee);
	class_<BloomCompositeEffect, base<IEffect>>("BloomCompositeEffect")
		.constructor<int, int, int>()
		.function("setIntensity", &BloomCompositeEffect::SetIntensity);
	class_<BlurXEffect, base<IEffect>>("BlurXEffect")
		.constructor<int, int, int>();
	class_<BlurYEffect, base<IEffect>>("BlurYEffect")
		.constructor<int, int, int>();
	class_<ResizeEffect, base<IEffect>>("ResizeEffect")
		.constructor<int, int, int>();
	class_<RTTDebug, base<IEffect>>("RTTDebug")
		.constructor<int, int, int, int>();
	class_<SSAOEffect, base<IEffect>>("SSAOEffect")
		.constructor<int, int, int>()
		.function("setViewMatrix", &SSAOEffect::SetViewMatrix)
		.function("setRadius", &SSAOEffect::SetRadius)
		.function("setStrength", &SSAOEffect::SetStrength)
		.function("setScale", &SSAOEffect::SetScale)
		.function("setTreshOld", &SSAOEffect::SetTreshOld);

	// Lua typo name preserved
	class_<VignetteEffect, base<IEffect>>("VignetEffect")
		.constructor<int, int, int, float, float>()
		.function("setRadius", &VignetteEffect::SetRadius)
		.function("setSoftness", &VignetteEffect::SetSoftness);

	class_<MotionBlurEffect, base<IEffect>>("MotionBlur")
		.constructor<int, Texture *, int, int>(allow_raw_pointers())
		.function("setTargetFPS", &MotionBlurEffect::SetTargetFPS)
		.function("setCurrentFPS", &MotionBlurEffect::SetCurrentFPS);

	class_<PostEffectsManager>("PostEffectsManager")
		.constructor<int, int>()
		.function("resize", &PostEffectsManager::Resize)
		.function("captureFrame", &PostEffectsManager::CaptureFrame)
		.function("endCapture", &PostEffectsManager::EndCapture)
		.function("processPostEffects", &PostEffects_Process)
		.function("addEffect", &PostEffects_AddEffect, allow_raw_pointers())
		.function("removeEffect", &PostEffectsManager::RemoveEffect, allow_raw_pointers())
		.function("removeAllEffects", &PostEffectsManager::RemoveAllEffects)
		.function("getNumberEffects", &PostEffectsManager::GetNumberEffects)
		.function("getExternalFrameBuffer", &PostEffectsManager::GetExternalFrameBuffer, allow_raw_pointers())
		.function("getColor", &PostEffectsManager::GetColor, allow_raw_pointers())
		.function("getDepth", &PostEffectsManager::GetDepth, allow_raw_pointers())
		.function("getLastRTT", &PostEffectsManager::GetLastRTT, allow_raw_pointers());

	emscripten::function("addPostEffect", &AddPostEffect);
	emscripten::function("clearPostEffectHandles", &ClearPostEffectHandles);
	emscripten::function("destroyMotionBlurVelocity", &DestroyMotionBlurVelocity);
	emscripten::function("buildMotionBlurPostChain", &BuildMotionBlurPostChain);
	emscripten::function("motionBlurRenderVelocity", &MotionBlurRenderVelocity);
	emscripten::function("motionBlurSetFPS", &MotionBlurSetFPS);
	emscripten::function("motionBlurResize", &MotionBlurResize);
	emscripten::function("motionBlurActive", &MotionBlurActive);
	emscripten::function("buildSSAOPostChain", &BuildSSAOPostChain);
	emscripten::function("ssaoSetViewMatrix", &SsaoSetViewMatrix);
	emscripten::function("ssaoSetParams", &SsaoSetParams);
	emscripten::function("buildDOFPostChain", &BuildDOFPostChain);
}

#endif /* EMSCRIPTEN */
