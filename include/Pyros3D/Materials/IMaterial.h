//============================================================================
// Name        : IMaterial.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : IMaterial Interface
//============================================================================

#ifndef IMATERIAL_H
#define IMATERIAL_H

#include <Pyros3D/Core/Math/Math.h>
#include <Pyros3D/Materials/GenericShaderMaterials/ShaderLib.h>
#include <Pyros3D/Materials/BlendState.h>
#include <Pyros3D/Ext/StringIDs/StringID.hpp>
#include <Pyros3D/Assets/Texture/Texture.h>
#include <Pyros3D/Other/Export.h>
#include <list>
#include <map>
#include <vector>

namespace p3d {

	// Culling
	namespace CullFace {
		enum {
			BackFace,
			FrontFace,
			DoubleSided
		};
	}

	class PYROS3D_API IMaterial {

	public:

		// Constructor
		IMaterial();

		// Destructor
		virtual ~IMaterial();

		// Properties
		void SetCullFace(const uint32 &face);
		uint32 GetCullFace() const;
		bool isWireFrame;
		bool IsWireFrame() const;
		const f32 &GetOpacity() const;
		bool IsTransparent() const;
		void SetOpacity(const f32 &opacity);
		void SetTransparencyFlag(bool transparency);

		// Polygon Offset
		void EnableDethBias(f32 factor, f32 units);
		void DisableDethBias();

		// Blending
		void EnableBlending() { blending = true; }
		void DisableBlending() { blending = false; }
		void BlendingFunction(const uint32 sFactor, const uint32 dFactor) { sfactor = sFactor; dfactor = dFactor; }
		void BlendingEquation(const uint32 Mode) { mode = Mode; }
		bool IsBlendingEnabled() const { return blending; }
		uint32 GetBlendingSFactor() const { return sfactor; }
		uint32 GetBlendingDFactor() const { return dfactor; }
		uint32 GetBlendingEquation() const { return mode; }
		bool IsDepthBiasEnabled() const { return depthBias; }
		f32 GetDepthBiasFactor() const { return depthFactor; }
		f32 GetDepthBiasUnits() const { return depthUnits; }

		// Render WireFrame
		void StartRenderWireFrame();
		void StopRenderWireFrame();

		// Shadows Casting
		void EnableCastingShadows();
		void DisableCastingShadows();
		bool IsCastingShadows();

		// Destroy
		void Destroy();

		// Get Internal ID
		uint32 GetInternalID();

		// The material asset file (an editor .mat) this material was built
		// from, or empty for one that belongs to a single object. The scene
		// serializer writes it next to the material's inline data, so a
		// scene keeps knowing which asset an object uses - the inline copy
		// is still what a game loads, so a runtime never needs the .mat.
		const std::string &GetAssetPath() const { return assetPath; }
		void SetAssetPath(const std::string &path) { assetPath = path; }

		// Depth Test and Write
		// Enabling the depth TEST says nothing about depth WRITING, and this
		// used to set forceDepthWrite too. Two things fell out of that, both
		// silent: IsDepthWritting()'s transparency rule below could never
		// fire (so a transparent surface wrote depth and punched a hole in
		// whatever was drawn behind it afterwards - a sprite's invisible
		// border came out as the clear colour), and DisableDepthWrite()
		// became a no-op on any material with depth testing on, which is
		// nearly all of them and includes the calls UIBatcher already makes.
		// depthWrite defaults to true, so opaque materials are unaffected.
		void EnableDepthTest(const uint32 test = 0) { depthTest = true; depthTestMode = test; }
		void DisableDepthTest() { forceDepthWrite = depthTest = false; }
		void EnableDepthWrite() { depthWrite = true; }
		void DisableDepthWrite() { depthWrite = false; }
		bool IsDepthWritting()
		{
			if (forceDepthWrite) return true;
			else {
				if (IsTransparent()) return false;
				else return depthWrite;
			}
		}
		bool IsDepthTesting()
		{
			return depthTest;
		}

	protected:

		std::string assetPath;

		// Depth Test and Write
		bool forceDepthWrite, depthTest, depthWrite;
		uint32 depthTestMode;

		//Casting Shadows
		bool isCastingShadows;

		// Cull Face
		uint32 cullFace;

		// Transparency
		bool isTransparent;

		// Opacity
		f32 opacity;

		// Material Options
		uint32 materialID;

		// Depth Bias
		f32 depthFactor, depthUnits;
		bool depthBias;

		// Blending
		bool blending;
		uint32 sfactor, dfactor, mode;

	private:

		// Internal ID
		static uint32 _InternalID;

	};

}

#endif /* IMATERIALl_H */