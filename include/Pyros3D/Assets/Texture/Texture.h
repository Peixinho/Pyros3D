//============================================================================
// Name        : Texture.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Texture
//============================================================================

#ifndef TEXTURE_H
#define TEXTURE_H

#include <Pyros3D/Core/Math/Math.h>
#include <Pyros3D/Core/Logs/Log.h>
#include <Pyros3D/Other/Export.h>
#include <Pyros3D/Core/File/File.h>
#include <map>
#include <vector>

namespace p3d {

	namespace TextureTransparency {
		enum {
			Opaque = 0,
			Transparent
		};
	}

	namespace TextureFilter {
		enum {
			Nearest = 0,
			Linear,
			LinearMipmapLinear,
			LinearMipmapNearest,
			NearestMipmapNearest,
			NearestMipmapLinear
		};
	}

	namespace TextureRepeat {
		enum {
			Clamp = 0,
			ClampToBorder,
			ClampToEdge,
			Repeat
		};
	}

	namespace TextureDataType {
		enum {
			RGBA = 0,
			BGR,
			BGRA,
			DepthComponent,
			DepthComponent16,
			DepthComponent24,
			DepthComponent32,
			R8,
			R16F,
			R32F,
			R16I,
			R32I,
			RG8,
			RG16F,
			RG32F,
			RG16I,
			RG32I,
			RGB8,
			RGB16F,
			RGB32F,
			RGB16I,
			RGB32I,
			RGBA16F,
			RGBA32F,
			RGBA16I,
			RGBA32I,
			LUMINANCE,
			LUMINANCE_ALPHA
		};
	}

	namespace TextureType {
		enum {
			CubemapPositive_X = 0,
			CubemapNegative_X,
			CubemapPositive_Y,
			CubemapNegative_Y,
			CubemapPositive_Z,
			CubemapNegative_Z,
			Texture_Multisample,
			Texture
		};
	}

	class PYROS3D_API Texture {

	private:

		// Internal ID for GL
		int32 GL_ID;
		uint32 Type;
		uint32 DataType;
		std::vector<uint32> Width;
		std::vector<uint32> Height;
		bool haveImage;
		bool isMipMap, isMipMapManual;
		uint32 cubemapFaces;
		// Sample count for TextureType::Texture_Multisample - Resize() needs
		// this to re-issue UploadTexture2DMultisample() with the same
		// sample count the texture was originally created with.
		uint32 storedSamples;

		// GL Properties
		f32 Transparency;
		uint32 MinFilter;
		uint32 MagFilter;
		uint32 SRepeat;
		uint32 TRepeat;
		uint32 RRepeat;
		uint32 mode, GLMode;
		uint32 subMode, GLSubMode;
		uint32 internalFormat, internalFormat2, internalFormat3;
		uint32 Anysotropic;
		Vec4 borderColor;

		// Not stored before this - LoadTexture()'s path was used once
		// then discarded, leaving no way to recover "what file was this"
		// after construction (needed e.g. for scene serialization). Empty
		// when built via CreateEmptyTexture()/LoadTextureFromMemory()
		// instead - those have no file source to record.
		std::string Filename;

		// Fallback for when there's no Filename (built via
		// LoadTextureFromMemory() directly, not through LoadTexture()) -
		// the original compressed file bytes (PNG/DDS/etc, NOT decoded
		// pixel data) were never retained before this, only ever used
		// once by stbi_load_from_memory() then discarded. Only populated
		// when Filename is empty, to avoid duplicating storage for the
		// common path-based-load case (LoadTexture() calls
		// LoadTextureFromMemory() internally too).
		std::vector<uchar> RawData;

		bool CreateTexture(uchar* data = NULL, bool Mipmapping = true, const uint32 level = 0, const uint32 msaa = 0);

		void GetGLModes();
		void GetInternalFormat();

#if !defined(GLES3)
		bool LoadDDS(uchar* data, bool Mipmapping = true, const uint32 level = 0);
#else
		bool LoadETC1(uchar* data, bool Mipmapping = true, const uint32 level = 0);
		uint16 swapBytes(uint16 aData);
#endif

	public:

		// Constructor
		Texture();

		// Texture
		bool LoadTexture(const std::string& Filename, const uint32 Type = TextureType::Texture, bool Mipmapping = true, const uint32 level = 0);
		bool LoadTextureFromMemory(std::vector<uchar> data, const uint32 length, const uint32 Type = TextureType::Texture, bool Mipmapping = true, const uint32 level = 0);
		bool CreateEmptyTexture(const uint32 Type, const uint32 DataType, const int32 width = 0, const int32 height = 0, bool Mipmapping = true, const uint32 level = 0, const uint32 msaa = 0); // msaa if using multisample only
		void SetMinMagFilter(const uint32 MinFilter, const uint32 MagFilter);
		void SetRepeat(const uint32 WrapS, const uint32 WrapT, const int32 WrapR = -1);
		void SetBorderColor(const Vec4 &Color);
		void EnableCompareMode();
		void SetTransparency(const f32 Transparency);
		void Resize(const uint32 Width, const uint32 Height, const uint32 level = 0);
		void UpdateData(void* srcPTR, const uint32 level = 0);
		void UpdateMipmap();
		void SetTextureByteAlignment(const uint32 Value);
		const uint32 GetBindID() const;
		// The engine's TextureType::* this was created as. Distinct from
		// FBOAttachment::TextureType, which holds the *native* target
		// (GL_TEXTURE_2D and friends) - a real trap, since both are called
		// "texture type" and only one of them means the engine enum.
		const uint32 GetTextureType() const { return Type; }
		// The engine's TextureDataType::* this was created with. The reliable
		// way to tell a depth target from a colour one: FBOAttachment's
		// AttachmentFormat is backend-translated (GL reports 36096 for depth
		// where the engine enum is 16, and Vulkan does not translate at all),
		// so it cannot be compared against the engine enum portably.
		const uint32 GetDataType() const { return DataType; }
		const uint32 GetWidth(const uint32 level = 0) const;
		const uint32 GetHeight(const uint32 level = 0) const;
		const std::string &GetFilename() const { return Filename; }
		// Fallback raw file bytes - see RawData's declaration comment.
		// Empty whenever GetFilename() is non-empty (redundant then).
		const std::vector<uchar> &GetRawData() const { return RawData; }

		// Use Asset
		void Bind();
		void Unbind();
		void DeleteTexture();

		// Get Texture Data
		// `faceType` overrides which sub-target is read - pass a
		// TextureType::Cubemap* value to read one face of a cubemap.
		// Without it a cubemap reads back whichever face happened to be
		// current (the last one CreateEmptyTexture() touched), which makes
		// reading all six impossible. TextureType::Texture (the default)
		// keeps the previous behaviour exactly.
		std::vector<uchar> GetTextureData(const uint32 level = 0, const int32 faceType = -1);

		// Get Last Binded Texture
		static uint32 GetLastBindedUnit();

		// Reset the static Bind()/Unbind() unit counter - call on demo/renderer
		// teardown so a mismatched Bind/Unbind cannot permanently shift every
		// subsequent texture unit assignment (black screen / unloadable tex).
		static void ResetUnitCounter() { UnitBinded = 0; LastUnitBinded = 0; }

		// Destructor
		virtual ~Texture();

		// Keep Unit Binded
		static uint32 LastUnitBinded;
		static uint32 UnitBinded;

	};

};

#endif /* TEXTURE_H */
