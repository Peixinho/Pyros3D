//============================================================================
// Name        : PyrosLuaPostFX.cpp
// Description : Post-effects manager and effects.
//============================================================================

#ifdef LUA_BINDINGS

#include <Pyros3D/Utils/Bindings/PyrosLuaBindings.h>
#include <Pyros3D/Utils/Bindings/PyrosLuaHelpers.h>

namespace p3d {

	void RegisterLuaPostFX(sol::state* lua)
	{
		{
			// Post Effects Manager
			sol::constructors<sol::types<int, int>> con;
			lua->new_usertype<PostEffectsManager>("PostEffectsManager",
				con,
				"resize", &PostEffectsManager::Resize,
				"captureFrame", &PostEffectsManager::CaptureFrame,
				"endCapture", &PostEffectsManager::EndCapture,
				"processPostEffects", &PostEffectsManager::ProcessPostEffects,
				"addEffect", &PostEffectsManager::AddEffect,
				"removeEffect", &PostEffectsManager::RemoveEffect,
				"removeAllEffects", &PostEffectsManager::RemoveAllEffects,
				"getNumberEffects", &PostEffectsManager::GetNumberEffects,
				"getExternalFrameBuffer", &PostEffectsManager::GetExternalFrameBuffer,
				"getColor", &PostEffectsManager::GetColor,
				"getDepth", &PostEffectsManager::GetDepth,
				"getLastRTT", &PostEffectsManager::GetLastRTT
				);
		}
		{
			// RTT source flags for IEffect constructors (TonemapEffect, etc.)
			lua->new_enum("RTT",
				"Color", RTT::Color,
				"Depth", RTT::Depth,
				"LastRTT", RTT::LastRTT,
				"CustomTexture", RTT::CustomTexture
			);
		}
		{
			// Named post-effect factory - AddEffect takes ownership (see
			// PostEffectsManager::RemoveAllEffects), so Lua must not also
			// own a TonemapEffect userdata that would double-delete on GC.
			lua->set_function("addPostEffect", [](PostEffectsManager &m, const std::string &name, int width, int height) {
				if (name == "tonemap")
					m.AddEffect(new TonemapEffect(RTT::Color, width, height));
				else
					echo("ERROR: addPostEffect - unknown effect '" + name + "'");
			});

			// SSAO full chain (ssao → blur → composite). Returns nothing;
			// per-frame view matrix via ssaoSetViewMatrix. Handles cleared
			// by clearPostEffectHandles() when the chain is torn down.
			static SSAOEffect *g_ssao = nullptr;
			static BlurSSAOEffect *g_ssaoBlur = nullptr;
			static VelocityRenderer *g_velocityRenderer = nullptr;
			static MotionBlurEffect *g_motionBlur = nullptr;
			lua->set_function("clearPostEffectHandles", []() {
				g_ssao = nullptr;
				g_ssaoBlur = nullptr;
				// Null the MotionBlurEffect handle only - the effect itself is
				// owned by PostEffectsManager and deleted in removeAllEffects.
				// VelocityRenderer owns the velocity texture the effect samples,
				// so it must outlive that delete (see destroyMotionBlurVelocity).
				g_motionBlur = nullptr;
				Texture::ResetUnitCounter();
			});
			lua->set_function("destroyMotionBlurVelocity", []() {
				delete g_velocityRenderer;
				g_velocityRenderer = nullptr;
			});
			lua->set_function("buildMotionBlurPostChain", [](PostEffectsManager &m, int width, int height) {
				delete g_velocityRenderer;
				g_velocityRenderer = new VelocityRenderer((uint32)width, (uint32)height);
				g_motionBlur = new MotionBlurEffect(RTT::Color, g_velocityRenderer->GetTexture(), (uint32)width, (uint32)height);
				g_motionBlur->SetTargetFPS(60.0f);
				g_motionBlur->SetCurrentFPS(60.0f);
				m.AddEffect(g_motionBlur);
			});
			lua->set_function("motionBlurRenderVelocity", [](const Projection &proj, GameObject *cam, SceneGraph *scene) {
				if (g_velocityRenderer && cam && scene)
					g_velocityRenderer->RenderVelocityMap(proj, cam, scene);
			});
			lua->set_function("motionBlurSetFPS", [](float currentFps) {
				if (g_motionBlur)
					g_motionBlur->SetCurrentFPS(currentFps);
			});
			lua->set_function("motionBlurResize", [](int width, int height) {
				if (g_velocityRenderer)
					g_velocityRenderer->Resize((uint32)width, (uint32)height);
				if (g_motionBlur)
					g_motionBlur->Resize((uint32)width, (uint32)height);
			});
			lua->set_function("motionBlurActive", []() {
				return g_velocityRenderer != nullptr && g_motionBlur != nullptr;
			});
			lua->set_function("buildSSAOPostChain", [](PostEffectsManager &m, int width, int height) {
				g_ssao = new SSAOEffect(RTT::Depth, width, height);
				g_ssao->SetRadius(0.2f);
				g_ssao->SetStrength(1.5f);
				g_ssao->SetTreshOld(2.0f);
				g_ssao->SetScale(1.0f);
				g_ssaoBlur = new BlurSSAOEffect(RTT::LastRTT, width, height);
				m.AddEffect(g_ssao);
				m.AddEffect(g_ssaoBlur);
				m.AddEffect(new SSAOCompositeEffect(RTT::Color, RTT::LastRTT, width, height));
			});
			lua->set_function("ssaoSetViewMatrix", [](const Matrix &m) {
				if (g_ssao) g_ssao->SetViewMatrix(m);
			});
			lua->set_function("ssaoSetParams", [](float radius, float strength, float threshold, float scale, float blurIntensity) {
				if (g_ssao)
				{
					g_ssao->SetRadius(radius);
					g_ssao->SetStrength(strength);
					g_ssao->SetTreshOld(threshold);
					g_ssao->SetScale(scale);
				}
				if (g_ssaoBlur) g_ssaoBlur->SetIntensity(blurIntensity);
			});
			lua->set_function("buildDOFPostChain", [](PostEffectsManager &m, int width, int height) {
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
			});

			lua->new_enum("CullingMode",
				"FrustumCulling", CullingMode::FrustumCulling
			);
		}
		{
			// IEffect
			sol::constructors<sol::types<int, int>> con;
			lua->new_usertype<IEffect>("IEffect",
				con,
				"compileShaders", &IEffect::CompileShaders,
				"resize", &IEffect::Resize,
				"getWidth", &IEffect::GetWidth,
				"getHeight", &IEffect::GetHeight,
				"getTexture", &IEffect::GetTexture
				);
		}
		{
			// Bloom
			sol::constructors<sol::types<int, int, int>> con;
			lua->new_usertype<BloomEffect>("BloomEffect",
				con,
				sol::base_classes, sol::bases<IEffect>()
				);
		}
		{
			// BlurXEffect
			sol::constructors<sol::types<int, int, int>> con;
			lua->new_usertype<BlurXEffect>("BlurXEffect",
				con,
				sol::base_classes, sol::bases<IEffect>()
				);
		}
		{
			// BlurYEffect
			sol::constructors<sol::types<int, int, int>> con;
			lua->new_usertype<BlurYEffect>("BlurYEffect",
				con,
				sol::base_classes, sol::bases<IEffect>()
				);
		}
		{
			// Resize Effect
			sol::constructors<sol::types<int, int, int>> con;
			lua->new_usertype<ResizeEffect>("ResizeEffect",
				con,
				sol::base_classes, sol::bases<IEffect>()
				);
		}
		{
			// RTT Debug
			sol::constructors<sol::types<int, int, int, int>> con;
			lua->new_usertype<RTTDebug>("RTTDebug",
				con,
				sol::base_classes, sol::bases<IEffect>()
				);
		}
		{
			// SSAO Effect
			sol::constructors<sol::types<int, int, int>> con;
			lua->new_usertype<SSAOEffect>("SSAOEffect",
				con,
				"setViewMatrix", &SSAOEffect::SetViewMatrix,
				"setRadius", &SSAOEffect::SetRadius,
				"setStrength", &SSAOEffect::SetStrength,
				"setScale", &SSAOEffect::SetScale,
				"setTreshOld", &SSAOEffect::SetTreshOld,
				sol::base_classes, sol::bases<IEffect>()
				);
		}
		{
			// VigneteEffec
			sol::constructors<sol::types<int, int, int, float, float>> con;
			lua->new_usertype<VignetteEffect>("VignetEffect",
				con,
				"setRadius", &VignetteEffect::SetRadius,
				"setSoftness", &VignetteEffect::SetSoftness,
				sol::base_classes, sol::bases<IEffect>()
				);
		}

		{
			// MotionBlur
			sol::constructors<sol::types<int, Texture*, int, int>> con;
			lua->new_usertype<MotionBlurEffect>("MotionBlur",
				con,
				"setTargetFPS", &MotionBlurEffect::SetTargetFPS,
				"setCurrentFPS", &MotionBlurEffect::SetCurrentFPS,
				sol::base_classes, sol::bases<IEffect>()
				);
		}

	}

} // namespace p3d

#endif
