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
#include <memory>
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

	public:

		// Pixels ready for upload: RGBA8, edges already bled.
		struct DecodedImage
		{
			int32 width = 0, height = 0;
			std::vector<uchar> pixels;
		};

	private:

		uint32 Type;
		uint32 DataType;
		std::vector<uint32> Width;
		std::vector<uint32> Height;
		bool haveImage;
		bool isMipMap, isMipMapManual;
	protected:
		uint32 cubemapFaces;
		// Sample count for TextureType::Texture_Multisample
		uint32 storedSamples;

		// How it is sampled
		f32 Transparency;
		uint32 MinFilter;
		uint32 MagFilter;
		bool dataMap = false;
		uint32 SRepeat;
		uint32 TRepeat;
		uint32 RRepeat;
		uint32 Anysotropic;
		Vec4 borderColor;

		// Not stored before this - LoadTexture()'s path was used once
		// then discarded, leaving no way to recover "what file was this"
		// after construction (needed e.g. for scene serialization). Empty
		// when built via CreateEmptyTexture()/LoadTextureFromMemory()
		// instead - those have no file source to record.
		std::string Filename;
		Vec3 averageColour; bool haveAverageColour = false;

		// Fallback for when there's no Filename (built via
		// LoadTextureFromMemory() directly, not through LoadTexture()) -
		// the original compressed file bytes (PNG/DDS/etc, NOT decoded
		// pixel data) were never retained before this, only ever used
		// once by stbi_load_from_memory() then discarded. Only populated
		// when Filename is empty, to avoid duplicating storage for the
		// common path-based-load case (LoadTexture() calls
		// LoadTextureFromMemory() internally too).
		std::vector<uchar> RawData;

		// The rest of a load: records the size and keeps the pixels.
		bool UploadDecoded(DecodedImage& image, bool Mipmapping, const uint32 level);

	public:

		// Constructor
		Texture();

		// Texture. Inside an open AssetBundle::Use it takes the pixels from
		// the bundle rather than reading the file - see AssetBundle.h.
		bool LoadTexture(const std::string& Filename, const uint32 Type = TextureType::Texture, bool Mipmapping = true, const uint32 level = 0);
		// A shared, path-keyed load: returns the SAME Texture for the same
		// file instead of decoding and uploading it again.
		//
		// A .p3dm's mesh data is binary and quick to read; its textures are
		// PNGs, decoded on the calling thread by stb_image. RenderingMesh
		// built a fresh Texture per submesh per map, so a station scene whose
		// 41 models share 22 images did ~600 inflate+unfilter passes and ~600
		// GPU uploads on every load - 40 s of a 42 s scene load, and many
		// times the texture memory it needed.
		//
		// Held weakly, so the last material to release a texture frees it and
		// unloading a scene still returns the memory. Callers that need to
		// mutate per-instance texture state (filter, wrap, transparency) must
		// NOT use this - they would be editing every other user's texture.
		//
		// clampToEdge is part of the cache key: a terrain tile's own colour
		// map must not wrap (bilinear filtering at its edge would blend in
		// the opposite edge, a visible seam between tiles) while a tiling
		// texture must, so the same file can exist once per mode.
		// The file a path names, links and ".." followed - what LoadShared and AssetBundle key a
		// picture by, so two paths to one file are one decode and one texture.
		static std::string FileKey(const std::string &Filename);
		static std::shared_ptr<Texture> LoadShared(const std::string& Filename,
			const uint32 Type = TextureType::Texture, bool Mipmapping = true, bool clampToEdge = false);
		bool IsClampedToEdge() const { return SRepeat == TextureRepeat::ClampToEdge; }
		// A data map - numbers packed in a picture's channels (a sea's depth, a travel time in two bytes) - is
		// loaded as written: never block-compressed, which would mix its channels into blocks. A scene sampler
		// asks for it with "data": true.
		bool IsDataMap() const { return dataMap; }
		void SetDataMap(const bool on) { dataMap = on; }
		// Drops entries whose texture is already gone. Housekeeping only.
		static void PurgeSharedCache();
		// The shared textures alive now whose file name ends with `suffix` (a path relative to a
		// project matches its absolute form): for what rewrites a texture's pixels in place.
		static std::vector<std::shared_ptr<Texture> > FindShared(const std::string &suffix);
		// The file changed on disk: the next LoadShared() of it reads it
		// again. Textures already handed out keep what they have - their
		// users let go of them as they are rebuilt.
		static void ForgetShared(const std::string& Filename);
		// True when LoadShared() would return an existing Texture for this
		// file (any type or mip flag) - nothing to decode.
		static bool IsSharedLoaded(const std::string& Filename);
		// ...loaded in that wrap mode, which is its own cache entry.
		static bool IsSharedLoaded(const std::string& Filename, const bool clampToEdge);

		// Read and decode with no device call, so any thread may run them.
		static bool DecodeFile(const std::string& Filename, DecodedImage& out);

		// Block compression (BlockCompress.h) of the textures loaded from images with mipmaps - a
		// material's maps: BC1 (an eighth of RGBA8) or BC3 (a quarter, with alpha), where the device
		// samples them; RGBA8 elsewhere. On by default; PYROS_TEXTURE_COMPRESSION=0 or a project's
		// "quality": {"textureCompression": false} turns it off. Encoded once per image and kept in
		// a cache outside the project (the user's cache folder; PYROS_TEXTURE_CACHE=<dir> moves it),
		// keyed by the image's path, size and time - a later load reads the blocks, not the image.
		// Compression is asked for by who loads a material's maps (the scene's materials, a
		// model's own): textures loaded while one of these lives on this thread are compressed;
		// a sprite, a UI image, a heightmap or an icon - loaded outside one - never are.
		struct PYROS3D_API CompressedLoads { CompressedLoads(); ~CompressedLoads(); };
		static bool InCompressedLoads();
		// Whether `Filename` has blocks in the compression cache that are still its own (an image
		// loader ahead of time need not decode it - the blocks will be read instead).
		static bool HasCompressedCache(const std::string& Filename);
		// ...and makes them when not (decode, encode, store - no device call: a loader's worker can).
		// False when compression is off or the device does not take blocks.
		static bool MakeCompressedCache(const std::string& Filename);
		static void SetCompression(const bool on);
		static bool GetCompression();
		static bool DecodeMemory(const uchar* data, const size_t length, DecodedImage& out);
		bool LoadTextureFromMemory(std::vector<uchar> data, const uint32 length, const uint32 Type = TextureType::Texture, bool Mipmapping = true, const uint32 level = 0);
		bool CreateEmptyTexture(const uint32 Type, const uint32 DataType, const int32 width = 0, const int32 height = 0, bool Mipmapping = true, const uint32 level = 0, const uint32 msaa = 0); // msaa if using multisample only
		void SetMinMagFilter(const uint32 MinFilter, const uint32 MagFilter);
		// Whether it is already filtered this way: lets a caller that only wants to be sure
		// skip SetMinMagFilter, which binds and unbinds on whichever unit is active.
		bool HasMinMagFilter(const uint32 MinFilter, const uint32 MagFilter) const { return this->MinFilter == MinFilter && this->MagFilter == MagFilter; }
		void SetRepeat(const uint32 WrapS, const uint32 WrapT, const int32 WrapR = -1);
		void SetBorderColor(const Vec4 &Color);
		// (a shadow map's: the renderer compares depths itself)
		void EnableCompareMode() {}
		void SetTransparency(const f32 Transparency);
		void Resize(const uint32 Width, const uint32 Height, const uint32 level = 0);
		void UpdateData(void* srcPTR, const uint32 level = 0);
		// The pixels were changed in place (through GetCpuPixels' storage): the renderer makes its copy again
		void UpdateMipmap() { cpuRevision++; }
		// (rows are always tightly packed)
		void SetTextureByteAlignment(const uint32) {}
		// The engine's TextureType::* this was created as.
		const uint32 GetTextureType() const { return Type; }
		// The engine's TextureDataType::* this was created with.
		const uint32 GetDataType() const { return DataType; }
		const uint32 GetWidth(const uint32 level = 0) const;
		const uint32 GetHeight(const uint32 level = 0) const;
		const std::string &GetFilename() const { return Filename; }
		// The picture's average colour, linear (as a shader's albedo is), taken when it was
		// decoded from a file; false when it was not (made, or filled from memory).
		bool GetAverageColour(Vec3 &out) const { if (haveAverageColour) out = averageColour; return haveAverageColour; }
		// Fallback raw file bytes - see RawData's declaration comment.
		// Empty whenever GetFilename() is non-empty (redundant then).
		const std::vector<uchar> &GetRawData() const { return RawData; }

		// The pixels, kept on the CPU (the renderer makes the GPU texture itself) - by face (a TextureType: Texture, or a cubemap's face) and mip level.
		// RGBA8 for a decoded picture; otherwise as DataType says. GetCpuRevision() changes when any does.
		const std::vector<uchar>* GetCpuPixels(const uint32 face = 0, const uint32 level = 0) const
		{
			std::map<std::pair<uint32, uint32>, std::vector<uchar> >::const_iterator i = cpuPixels.find(std::make_pair(face, level));
			return i == cpuPixels.end() ? NULL : &i->second;
		}
		uint32 GetCpuRevision() const { return cpuRevision; }
		// Never the same for two textures, while the process lives: what a cache keyed by a texture's address checks (a
		// new one can be made where a freed one was - the renderer's GPU textures)
		uint32 GetInstanceID() const { return instanceId; }
		uint32 GetMinFilter() const { return MinFilter; }
		uint32 GetMagFilter() const { return MagFilter; }
		uint32 GetRepeatS() const { return SRepeat; }
		uint32 GetRepeatT() const { return TRepeat; }
		bool IsMipmapped() const { return isMipMap; }
	private:
		std::map<std::pair<uint32, uint32>, std::vector<uchar> > cpuPixels;
		uint32 cpuRevision = 0;
		static uint32 NextInstanceID();
		uint32 instanceId = NextInstanceID();
		void KeepCpuPixels(const uint32 level, std::vector<uchar> &&pixels) { cpuPixels[std::make_pair(Type, level)] = std::move(pixels); cpuRevision++; }
	public:

		// Get Texture Data
		// `faceType` overrides which sub-target is read - pass a
		// TextureType::Cubemap* value to read one face of a cubemap.
		// Without it, the face it was last made as.
		std::vector<uchar> GetTextureData(const uint32 level = 0, const int32 faceType = -1);

		// Destructor
		virtual ~Texture();

	};

};

#endif /* TEXTURE_H */
