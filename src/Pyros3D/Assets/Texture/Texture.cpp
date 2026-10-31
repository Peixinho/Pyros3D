//============================================================================
// Name        : Texture.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Texture
//============================================================================

#include <Pyros3D/Assets/Texture/Texture.h>
#include <Pyros3D/Utils/Streaming/LoadStats.h>
#include <Pyros3D/Utils/Streaming/AssetBundle.h>
#include "BlockCompress.h"
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <Pyros3D/Ext/StringIDs/StringID.hpp>
#include <string.h>
#include <Pyros3D/Resources/Resources.h>
#define STB_IMAGE_IMPLEMENTATION
#include <Pyros3D/Ext/stb/stb_image.h>
#include <cstdint>
#include <filesystem>   // INT64_MAX, used by BleedTransparentEdges below
#include <fstream>
#include <map>
#include <mutex>

namespace p3d {

	uint32 Texture::NextInstanceID() { static std::atomic<uint32> next{ 1 }; return next++; }


	// A texture is its pictures, decoded and kept on the CPU (GetCpuPixels), with what they are and how they are to be
	// sampled: the renderer makes its own GPU copy of each, and makes it again when cpuRevision moves
	Texture::Texture() : Type(TextureType::Texture), haveImage(false), isMipMap(false), isMipMapManual(false), Anysotropic(0), cubemapFaces(0), storedSamples(0) {}

	Texture::~Texture() {}

	bool Texture::LoadTexture(const std::string& Filename, const uint32 Type, bool Mipmapping, const uint32 level)
	{
		this->Type = Type;

		// A Radiance .hdr (an HDR sky, a light probe): linear float RGB, kept as floats. The mips are
		// skipped, as a sky or a probe is looked up, not minified across a surface.
		if (Filename.size() > 4 && (Filename.compare(Filename.size() - 4, 4, ".hdr") == 0 || Filename.compare(Filename.size() - 4, 4, ".HDR") == 0))
		{
			int w = 0, h = 0, n = 0;
			float *px = stbi_loadf(Filename.c_str(), &w, &h, &n, 4);
			if (px && w > 0 && h > 0)
			{
				// RGBA32F: the one float layout every backend uploads the same way (a 16F texture is
				// sent as 32-bit floats on GL and as halves on Vulkan). Not filtered: 32-bit float
				// textures are not filterable on every GPU, and one sampled with a filter it cannot
				// do comes back as nothing or NaN, which the effects after it spread over the frame.
				CreateEmptyTexture(TextureType::Texture, TextureDataType::RGBA32F, w, h, false);
				UpdateData(px);
				SetMinMagFilter(TextureFilter::Nearest, TextureFilter::Nearest);
				stbi_image_free(px);
				this->Filename = Filename;
				return true;
			}
			if (px) stbi_image_free(px);
			echo("ERROR: could not read the HDR image " + Filename);
		}

		DecodedImage image;
		bool status = AssetBundle::TakeImage(Filename, image) || DecodeFile(Filename, image);
		if (status && level == 0)
		{
			// Its average colour, linear, while the pixels are here (GetAverageColour): the light
			// bounced off a surface wants it, and reading the file again for it was a decode of
			// every picture in the scene, one after another, at the end of a load.
			const size_t count = (size_t)image.width * image.height;
			const size_t step = std::max<size_t>(1, count / 65536);
			f64 r = 0, g = 0, b = 0; size_t n = 0;
			for (size_t i = 0; i < count; i += step, n++)
			{
				const uchar* p = &image.pixels[i * 4];
				r += std::pow(p[0] / 255.0, 2.2); g += std::pow(p[1] / 255.0, 2.2); b += std::pow(p[2] / 255.0, 2.2);
			}
			if (n) { averageColour = Vec3((f32)(r / n), (f32)(g / n), (f32)(b / n)); haveAverageColour = true; }
		}
		if (status)
			status = UploadDecoded(image, Mipmapping, level);
		// Only record a real, successfully-loaded file's path - a failed
		// load falls through to the checkerboard fallback below, which
		// isn't actually this Filename's image data.
		if (status) { this->Filename = Filename; this->RawData.clear(); this->RawData.shrink_to_fit(); }
		if (!status) {
			echo("ERROR: Couldn't find texture file or failed to load, loading default one...");

			int i, j, c;
			uchar checkImage[4][4][4];
			for (i = 0; i < 4; i++) {
				for (j = 0; j < 4; j++) {
					if ((i%2==0 && j%2!=0) || (i%2!=0 && j%2==0)) {
						checkImage[i][j][0] = 255;
						checkImage[i][j][1] = 255;
						checkImage[i][j][2] = 255;
						checkImage[i][j][3] = 255;
					}
					else  {
						checkImage[i][j][0] = 255;
						checkImage[i][j][1] = 165;
						checkImage[i][j][2] = 0;
						checkImage[i][j][3] = 255;
					}
				}
			}

			this->CreateEmptyTexture(TextureType::Texture, TextureDataType::RGBA, 4, 4, false);
			this->UpdateData(&checkImage);
			this->SetRepeat(TextureRepeat::Repeat, TextureRepeat::Repeat);
			this->SetMinMagFilter(TextureFilter::Nearest, TextureFilter::Nearest);
			this->SetTextureByteAlignment(1);
			status = true;
		}
		return status;
	}

	// Fills the colour of fully-transparent texels with their nearest visible
	// neighbour's colour, leaving alpha alone.
	//
	// Every PNG writer zeroes the RGB of a fully transparent pixel, so a
	// sprite's transparent margin is (0,0,0,0). Nothing draws those texels -
	// but the sampler still *reads* them: bilinear filtering (and every mip
	// level) blends a border texel with its transparent neighbours, and the
	// colour it blends toward is black. The result is a one-or-two pixel dark
	// ring around every sprite with an alpha edge, on all four sides, which
	// looks exactly like a lighting or shadow bug and is not one. Giving the
	// invisible texels a sensible colour removes it, and cannot change any
	// visible pixel because their alpha is still zero.
	//
	// Only texels that are BOTH fully transparent AND pure black are touched.
	// That is precisely the case that haloes, and it leaves alone any texture
	// that deliberately packs data into the colour of a zero-alpha texel.
	//
	// Two sweeps of nearest-source propagation rather than repeated dilation:
	// dilation needs one pass per pixel of margin (a mostly-transparent atlas
	// would need hundreds), this is O(w*h) whatever the margin looks like.
	static void BleedTransparentEdges(std::vector<uchar> &pixels, const int32 w, const int32 h)
	{
		if (w <= 0 || h <= 0) return;
		const size_t count = (size_t)w * (size_t)h;

		std::vector<int32> src(count, -1);
		size_t opaque = 0, needsFill = 0;
		for (size_t i = 0; i < count; i++)
		{
			const uchar* p = &pixels[i * 4];
			if (p[3] != 0) { src[i] = (int32)i; opaque++; }
			else if (p[0] == 0 && p[1] == 0 && p[2] == 0) needsFill++;
		}
		// Nothing to bleed, or nothing to bleed *from*.
		if (needsFill == 0 || opaque == 0) return;

		// Squared distance from (x,y) to the candidate's source texel; -1
		// sources never win because they are rejected before this is called.
		struct L {
			static int64 Dist2(const int32 s, const int32 x, const int32 y, const int32 w)
			{
				const int64 dx = (int64)(s % w) - x;
				const int64 dy = (int64)(s / w) - y;
				return dx * dx + dy * dy;
			}
		};

		std::vector<int64> best(count, INT64_MAX);
		for (size_t i = 0; i < count; i++)
			if (src[i] >= 0) best[i] = 0;

		const int32 fwd[4][2] = { { -1, 0 }, { 0, -1 }, { -1, -1 }, { 1, -1 } };
		const int32 bwd[4][2] = { { 1, 0 }, { 0, 1 }, { 1, 1 }, { -1, 1 } };

		for (int32 pass = 0; pass < 2; pass++)
		{
			const int32 (*off)[2] = (pass == 0) ? fwd : bwd;
			for (int32 j = 0; j < h; j++)
			{
				const int32 y = (pass == 0) ? j : (h - 1 - j);
				for (int32 i = 0; i < w; i++)
				{
					const int32 x = (pass == 0) ? i : (w - 1 - i);
					const size_t idx = (size_t)y * w + x;
					if (best[idx] == 0) continue;
					for (int32 k = 0; k < 4; k++)
					{
						const int32 nx = x + off[k][0], ny = y + off[k][1];
						if (nx < 0 || ny < 0 || nx >= w || ny >= h) continue;
						const int32 cand = src[(size_t)ny * w + nx];
						if (cand < 0) continue;
						const int64 d = L::Dist2(cand, x, y, w);
						if (d < best[idx]) { best[idx] = d; src[idx] = cand; }
					}
				}
			}
		}

		for (size_t i = 0; i < count; i++)
		{
			uchar* p = &pixels[i * 4];
			if (p[3] != 0) continue;
			if (p[0] != 0 || p[1] != 0 || p[2] != 0) continue;
			const int32 s = src[i];
			if (s < 0) continue;
			const uchar* q = &pixels[(size_t)s * 4];
			p[0] = q[0]; p[1] = q[1]; p[2] = q[2];
		}
	}

	// ---- block compression and its cache (see Texture.h)
	namespace {
		std::atomic<int> g_compression(-1);     // -1: not decided yet (the environment says)

		uint64 Fnv64(const std::string &s)
		{
			uint64 h = 1469598103934665603ULL;
			for (size_t i = 0; i < s.size(); i++) { h ^= (uchar)s[i]; h *= 1099511628211ULL; }
			return h;
		}
		std::filesystem::path CompressionCacheDir()
		{
			static std::filesystem::path dir;
			static std::once_flag once;
			std::call_once(once, []() {
				if (const char* e = getenv("PYROS_TEXTURE_CACHE")) { dir = e; return; }
				// a game that ships its blocks: texcache/ beside it (what a packager put there)
				std::error_code ec;
				if (std::filesystem::is_directory("texcache", ec)) { dir = std::filesystem::absolute("texcache", ec); return; }
#if defined(_WIN32)
				if (const char* l = getenv("LOCALAPPDATA")) { dir = std::filesystem::path(l) / "Pyros3D" / "TextureCache"; return; }
#elif defined(__APPLE__)
				if (const char* h = getenv("HOME")) { dir = std::filesystem::path(h) / "Library" / "Caches" / "Pyros3D" / "textures"; return; }
#else
				if (const char* x = getenv("XDG_CACHE_HOME")) { dir = std::filesystem::path(x) / "pyros3d" / "textures"; return; }
				if (const char* h = getenv("HOME")) { dir = std::filesystem::path(h) / ".cache" / "pyros3d" / "textures"; return; }
#endif
				dir = std::filesystem::temp_directory_path() / "pyros3d-textures";
			});
			return dir;
		}
		// (004: a cut-out's mips keep its coverage - BlockCompress::EncodeMipChain;
		//  005: a colour picture's mips averaged as light, not as encoded numbers)
		const char kTexCacheMagic[8] = { 'P', '3', 'D', 'T', 'C', '0', '0', '6' };
		struct TexCacheHeader
		{
			char magic[8]; uint64 sourceSize; int64 sourceTime; uint32 format, width, height, levels; f32 average[3]; uint32 haveAverage;
		};
	}

	namespace { thread_local int t_compressedLoads = 0; }
	Texture::CompressedLoads::CompressedLoads() { t_compressedLoads++; }
	Texture::CompressedLoads::~CompressedLoads() { t_compressedLoads--; }
	bool Texture::InCompressedLoads() { return t_compressedLoads > 0 && GetCompression(); }
	void Texture::SetCompression(const bool on) { g_compression.store(on ? 1 : 0); }
	bool Texture::GetCompression()
	{
		int c = g_compression.load();
		if (c < 0)
		{
			const char* e = getenv("PYROS_TEXTURE_COMPRESSION");
			c = (e != NULL && e[0] == '0') ? 0 : 1;
			g_compression.store(c);
		}
		return c == 1;
	}

	namespace {
		// The cache's key for an image: its path as the game sees it (from its working folder, when it
		// is under it - so a game moved, or unpacked somewhere else, still finds its blocks: no links
		// resolved, the dev folder's assets/ link included), its size, and a sample of its bytes - not
		// its time, which a zip keeps only to two seconds and a copy may not keep at all.
		// What the cache holds, by file: its source's size and stamp - written beside the blocks
		// (manifest.txt: name, size, stamp, the source's path), so a game shipped without the
		// source images, only their blocks, still finds them.
		struct ManifestEntry { uint64 size; int64 stamp; };
		std::mutex g_manifestGuard;
		std::map<std::string, ManifestEntry> &Manifest()
		{
			static std::map<std::string, ManifestEntry> m;
			static bool read = false;
			if (!read)
			{
				read = true;
				std::ifstream in((CompressionCacheDir() / "manifest.txt").string());
				std::string name; ManifestEntry e;
				while (in >> name >> e.size >> e.stamp) { m[name] = e; std::string rest; std::getline(in, rest); }
			}
			return m;
		}
		void ManifestAdd(const std::filesystem::path &cacheFile, const uint64 size, const int64 stamp, const std::string &where)
		{
			std::lock_guard<std::mutex> lock(g_manifestGuard);
			const std::string name = cacheFile.filename().string();
			std::map<std::string, ManifestEntry> &m = Manifest();
			std::map<std::string, ManifestEntry>::iterator it = m.find(name);
			if (it != m.end() && it->second.size == size && it->second.stamp == stamp) return;
			m[name] = { size, stamp };
			std::ofstream out((CompressionCacheDir() / "manifest.txt").string(), std::ios::app);
			out << name << " " << size << " " << stamp << " " << where << "\n";
		}

		bool CacheFileFor(const std::string& Filename, std::filesystem::path &cacheFile, uint64 &srcSize, int64 &srcStamp, std::string *whereOut = NULL)
		{
			std::error_code ec;
			const std::filesystem::path src = std::filesystem::absolute(std::filesystem::path(Filename), ec).lexically_normal();
			std::string where = src.generic_string();
			{
				std::error_code ec2;
				const std::filesystem::path rel = src.lexically_relative(std::filesystem::current_path(ec2).lexically_normal());
				if (!ec2 && !rel.empty() && rel.native()[0] != '.') where = rel.generic_string();
			}
			if (whereOut) *whereOut = where;
			if (!std::filesystem::exists(src, ec))
			{
				// shipped without it: the manifest says what the blocks were made from
				cacheFile = CompressionCacheDir() / (std::to_string(Fnv64(where)) + ".p3dtc");
				std::lock_guard<std::mutex> lock(g_manifestGuard);
				std::map<std::string, ManifestEntry>::iterator it = Manifest().find(cacheFile.filename().string());
				if (it == Manifest().end()) return false;
				srcSize = it->second.size; srcStamp = it->second.stamp;
				return true;
			}
			srcSize = (uint64)std::filesystem::file_size(src, ec);
			if (ec || srcSize == 0) return false;
			uint64 h = 1469598103934665603ULL;
			if (FILE* f = fopen(src.string().c_str(), "rb"))
			{
				uchar buf[65536];
				size_t n = fread(buf, 1, sizeof(buf), f);
				for (size_t i = 0; i < n; i++) { h ^= buf[i]; h *= 1099511628211ULL; }
				if (srcSize > sizeof(buf) * 2 && fseek(f, -(long)sizeof(buf), SEEK_END) == 0)
				{
					n = fread(buf, 1, sizeof(buf), f);
					for (size_t i = 0; i < n; i++) { h ^= buf[i]; h *= 1099511628211ULL; }
				}
				fclose(f);
			}
			srcStamp = (int64)h;
			cacheFile = CompressionCacheDir() / (std::to_string(Fnv64(where)) + ".p3dtc");
			return true;
		}
	}

	bool Texture::HasCompressedCache(const std::string& Filename)
	{
		if (!GetCompression()) return false;
		std::filesystem::path cacheFile; uint64 srcSize = 0; int64 srcTime = 0;
		if (!CacheFileFor(Filename, cacheFile, srcSize, srcTime)) return false;
		TexCacheHeader hd;
		bool ok = false;
		if (FILE* f = fopen(cacheFile.string().c_str(), "rb"))
		{
			ok = fread(&hd, sizeof(hd), 1, f) == 1 && memcmp(hd.magic, kTexCacheMagic, 8) == 0 && hd.sourceSize == srcSize && hd.sourceTime == srcTime;
			fclose(f);
		}
		return ok;
	}

	namespace {
		// An image's blocks made and kept in the cache (written aside and moved in: a half-written
		// file is never read); hd/levels hold them after.
		void EncodeAndStore(Texture::DecodedImage &image, const std::filesystem::path &cacheFile, const uint64 srcSize, const int64 srcTime,
			TexCacheHeader &hd, std::vector<std::vector<uchar> > &levels, const std::string &where)
		{
			std::error_code ec;
			LoadStats::Scope t(LoadStats::TextureDecode);
			memcpy(hd.magic, kTexCacheMagic, 8);
			hd.sourceSize = srcSize; hd.sourceTime = srcTime;
			hd.width = (uint32)image.width; hd.height = (uint32)image.height;
			hd.format = BlockCompress::ChooseFormat(image.pixels, where.c_str());
			{
				const size_t count = (size_t)image.width * image.height;
				const size_t step = std::max<size_t>(1, count / 65536);
				f64 r = 0, g = 0, b = 0; size_t n = 0;
				for (size_t i = 0; i < count; i += step, n++)
				{
					const uchar* q = &image.pixels[i * 4];
					r += std::pow(q[0] / 255.0, 2.2); g += std::pow(q[1] / 255.0, 2.2); b += std::pow(q[2] / 255.0, 2.2);
				}
				hd.average[0] = n ? (f32)(r / n) : 0.f; hd.average[1] = n ? (f32)(g / n) : 0.f; hd.average[2] = n ? (f32)(b / n) : 0.f;
				hd.haveAverage = n ? 1 : 0;
			}
			BlockCompress::EncodeMipChain(hd.format, image.pixels, hd.width, hd.height, levels, BlockCompress::IsColourPicture(hd.format, where.c_str()));
			hd.levels = (uint32)levels.size();
			// kept for next time (written aside and moved in: a half-written file is never read)
			std::filesystem::create_directories(CompressionCacheDir(), ec);
			const std::string tmp = cacheFile.string() + ".tmp" + std::to_string((uintptr_t)&hd);
			if (FILE* f = fopen(tmp.c_str(), "wb"))
			{
				bool ok = fwrite(&hd, sizeof(hd), 1, f) == 1;
				for (size_t l = 0; l < levels.size() && ok; l++) ok = fwrite(levels[l].data(), 1, levels[l].size(), f) == levels[l].size();
				fclose(f);
				if (ok) std::filesystem::rename(tmp, cacheFile, ec);
				if (!ok || ec) std::filesystem::remove(tmp, ec);
			}
		}
	}

	bool Texture::MakeCompressedCache(const std::string& Filename)
	{
		if (!GetCompression()) return false;
		// (a float image - an HDR sky - is read by its own loader, never as blocks)
		if (Filename.size() > 4 && (Filename.compare(Filename.size() - 4, 4, ".hdr") == 0 || Filename.compare(Filename.size() - 4, 4, ".HDR") == 0)) return false;
		if (HasCompressedCache(Filename)) return true;
		std::filesystem::path cacheFile; uint64 srcSize = 0; int64 srcTime = 0; std::string where;
		if (!CacheFileFor(Filename, cacheFile, srcSize, srcTime, &where)) return false;
		DecodedImage image;
		if (!DecodeFile(Filename, image) || image.width < 4 || image.height < 4) return false;
		TexCacheHeader hd;
		std::vector<std::vector<uchar> > levels;
		EncodeAndStore(image, cacheFile, srcSize, srcTime, hd, levels, where);
		return true;
	}

	bool Texture::DecodeFile(const std::string& Filename, DecodedImage& out)
	{
		File file;
		{
			LoadStats::Scope t(LoadStats::FileRead);
			if (!file.Open(Filename))
				return false;
		}
		const bool ok = file.Size() > 0 && DecodeMemory(&file.GetData()[0], file.Size(), out);
		file.Close();
		return ok;
	}

	bool Texture::DecodeMemory(const uchar* data, const size_t length, DecodedImage& out)
	{
		LoadStats::Scope t(LoadStats::TextureDecode);
		int32 w, h, bpp;
		uchar* imagePTR = (data && length) ? stbi_load_from_memory(data, (int)length, &w, &h, &bpp, 4) : NULL;

		// Check the result *before* touching w/h/imagePTR. On failure stb
		// returns NULL and leaves w/h uninitialized, so the resize() below
		// was handed a garbage size - std::length_error escaping as far as
		// terminate() - and the memcpy() read from NULL. Any format stb
		// cannot decode reaches this (it has no DDS support at all), so an
		// unreadable texture killed the process instead of returning false.
		if (!imagePTR)
		{
			echo("ERROR: Failed to Open Texture");
			return false;
		}

		out.width = w;
		out.height = h;
		out.pixels.assign(imagePTR, imagePTR + (size_t)w * h * 4);
		stbi_image_free(imagePTR);

		// Before upload, so mip generation sees the bled colours too.
		BleedTransparentEdges(out.pixels, w, h);
		return true;
	}

	bool Texture::LoadTextureFromMemory(std::vector<uchar> data, const uint32 length, const uint32 Type, bool Mipmapping, const uint32 level)
	{
		// (the bytes kept as the texture's source for a scene that saves it - a path, when it has one, is better:
		// LoadTexture clears them)
		this->RawData = data;
		this->Type = Type;
		DecodedImage image;
		if (data.empty() || !DecodeMemory(&data[0], length, image)) return false;
		return UploadDecoded(image, Mipmapping, level);
	}

	bool Texture::UploadDecoded(DecodedImage& image, bool Mipmapping, const uint32 level)
	{
		if (this->Width.size() < level + 1)
		{
			this->Width.resize(level + 1);
			this->Height.resize(level + 1);
		}

		this->Width[level] = image.width;
		this->Height[level] = image.height;
		this->haveImage = true;
		this->Transparency = TextureTransparency::Opaque;
		this->DataType = TextureDataType::RGBA;

		isMipMap = Mipmapping;
		KeepCpuPixels(level, std::move(image.pixels));
		return true;
	}

	bool Texture::CreateEmptyTexture(const uint32 Type, const uint32 TextureDataType, const int32 width, const int32 height, bool Mipmapping, const uint32 level, const uint32 msaa)
	{
		if (this->Width.size() < level + 1)
		{
			this->Width.resize(level + 1);
			this->Height.resize(level + 1);
		}
		Width[level] = width;
		Height[level] = height;
		this->Type = Type;
		this->DataType = TextureDataType;
		this->haveImage = false;

		// Depth targets default to Nearest, everything else keeps the class
		// default. Apple's GL refuses to load a Linear-filtered depth
		// texture and silently substitutes the zero texture for it - which
		// is how SSAO ended up sampling nothing on that backend - and every
		// caller in the engine already sets Nearest by hand right after this
		// (see DirectionalLight/SpotLight::EnableCastShadows and
		// PostEffectsManager::Init). Setting it here instead of relying on
		// each caller to remember means the texture is never *created*
		// wrong, so the driver has nothing to reject in the window between
		// CreateTexture() below and the caller's own SetMinMagFilter().
		// Filtering a depth buffer is meaningless anyway: the average of two
		// depths is a surface that is not there.
		if (TextureDataType == TextureDataType::DepthComponent ||
			TextureDataType == TextureDataType::DepthComponent16 ||
			TextureDataType == TextureDataType::DepthComponent24 ||
			TextureDataType == TextureDataType::DepthComponent32)
		{
			MinFilter = TextureFilter::Nearest;
			MagFilter = TextureFilter::Nearest;
		}

		isMipMap = Mipmapping; storedSamples = msaa;
		return true;
	}

	void Texture::SetRepeat(const uint32 WrapS, const uint32 WrapT, const int32 WrapR)
	{

		SRepeat = WrapS;
		TRepeat = WrapT;
		if (WrapR > -1) RRepeat = WrapR;
	}

	void Texture::SetMinMagFilter(const uint32 MinFilter, const uint32 MagFilter)
	{

		this->MinFilter = MinFilter;
		this->MagFilter = MagFilter;
	}

	namespace {
		// Path -> texture, weakly held. Keyed on the string the caller gave,
		// which is what every call site already resolves before getting
		// here (see ResolveModelTexturePath for the model path).
		std::map<std::string, std::weak_ptr<Texture> > &SharedTextureCache()
		{
			static std::map<std::string, std::weak_ptr<Texture> > cache;
			return cache;
		}
		// Loads happen on the main thread, but IsSharedLoaded() is asked
		// from loader threads filling an AssetBundle.
		std::mutex &SharedTextureCacheMutex()
		{
			static std::mutex m;
			return m;
		}
	}

	std::vector<std::shared_ptr<Texture> > Texture::FindShared(const std::string &suffix)
	{
		std::vector<std::shared_ptr<Texture> > found;
		std::lock_guard<std::mutex> lock(SharedTextureCacheMutex());
		std::map<std::string, std::weak_ptr<Texture> > &cache = SharedTextureCache();
		for (std::map<std::string, std::weak_ptr<Texture> >::iterator i = cache.begin(); i != cache.end(); ++i)
		{
			const std::string file = i->first.substr(0, i->first.find('|'));
			if (file.size() < suffix.size() || file.compare(file.size() - suffix.size(), suffix.size(), suffix) != 0) continue;
			if (std::shared_ptr<Texture> t = i->second.lock()) found.push_back(t);
		}
		return found;
	}

	// The file a path names, links followed: two paths to one picture (a model's textures folder
	// linked to another's - recoloured copies of a kit sharing all but one map) are one texture,
	// not one copy each in memory.
	static std::string SharedTextureKey(const std::string &Filename);
	std::string Texture::FileKey(const std::string &Filename) { return SharedTextureKey(Filename); }
	static std::string SharedTextureKey(const std::string &Filename)
	{
		std::error_code ec;
		const std::filesystem::path c = std::filesystem::weakly_canonical(std::filesystem::path(Filename), ec);
		return ec ? Filename : c.generic_string();
	}

	std::shared_ptr<Texture> Texture::LoadShared(const std::string& Filename,
		const uint32 Type, bool Mipmapping, bool clampToEdge)
	{
		std::map<std::string, std::weak_ptr<Texture> > &cache = SharedTextureCache();
		const std::string key = SharedTextureKey(Filename) + "|" + std::to_string(Type) + (Mipmapping ? "|m" : "|n") + (clampToEdge ? "|c" : "")
			+ (InCompressedLoads() ? "|bc" : "");     // (a material's blocks are not what a raw reader wants)
		{
			std::lock_guard<std::mutex> lock(SharedTextureCacheMutex());
			std::map<std::string, std::weak_ptr<Texture> >::iterator it = cache.find(key);
			if (it != cache.end())
			{
				if (std::shared_ptr<Texture> hit = it->second.lock())
					return hit;
				cache.erase(it);
			}
		}
		std::shared_ptr<Texture> tex = std::make_shared<Texture>();
		if (!tex->LoadTexture(Filename, Type, Mipmapping))
			return std::shared_ptr<Texture>();
		if (clampToEdge) tex->SetRepeat(TextureRepeat::ClampToEdge, TextureRepeat::ClampToEdge);
		std::lock_guard<std::mutex> lock(SharedTextureCacheMutex());
		cache[key] = tex;
		return tex;
	}

	void Texture::ForgetShared(const std::string& Filename)
	{
		std::lock_guard<std::mutex> lock(SharedTextureCacheMutex());
		std::map<std::string, std::weak_ptr<Texture> > &cache = SharedTextureCache();
		const std::string prefix = SharedTextureKey(Filename) + "|";
		std::map<std::string, std::weak_ptr<Texture> >::iterator it = cache.lower_bound(prefix);
		while (it != cache.end() && it->first.compare(0, prefix.size(), prefix) == 0) it = cache.erase(it);
	}

	bool Texture::IsSharedLoaded(const std::string& Filename)
	{
		std::lock_guard<std::mutex> lock(SharedTextureCacheMutex());
		std::map<std::string, std::weak_ptr<Texture> > &cache = SharedTextureCache();
		const std::string prefix = SharedTextureKey(Filename) + "|";
		for (std::map<std::string, std::weak_ptr<Texture> >::const_iterator it = cache.lower_bound(prefix);
			it != cache.end() && it->first.compare(0, prefix.size(), prefix) == 0; ++it)
			if (!it->second.expired()) return true;
		return false;
	}

	bool Texture::IsSharedLoaded(const std::string& Filename, const bool clampToEdge)
	{
		std::lock_guard<std::mutex> lock(SharedTextureCacheMutex());
		std::map<std::string, std::weak_ptr<Texture> > &cache = SharedTextureCache();
		const std::string prefix = SharedTextureKey(Filename) + "|";
		for (std::map<std::string, std::weak_ptr<Texture> >::const_iterator it = cache.lower_bound(prefix);
			it != cache.end() && it->first.compare(0, prefix.size(), prefix) == 0; ++it)
		{
			// ("|c" a part of the key's tail: "...|c" or "...|c|bc")
			const std::string &k = it->first;
			const bool clamped = (k.size() >= 2 && k.compare(k.size() - 2, 2, "|c") == 0) || k.find("|c|") != std::string::npos;
			if (clamped == clampToEdge && !it->second.expired()) return true;
		}
		return false;
	}

	void Texture::PurgeSharedCache()
	{
		std::lock_guard<std::mutex> lock(SharedTextureCacheMutex());
		std::map<std::string, std::weak_ptr<Texture> > &cache = SharedTextureCache();
		std::map<std::string, std::weak_ptr<Texture> >::iterator i = cache.begin();
		while (i != cache.end())
		{
			if (i->second.expired()) cache.erase(i++);
			else ++i;
		}
	}

	void Texture::SetTransparency(const f32 Transparency)
	{

		this->Transparency = Transparency;

	}

	void Texture::SetBorderColor(const Vec4 &Color)
	{
		borderColor = Color;
	}

	void Texture::Resize(const uint32 Width, const uint32 Height, const uint32 level)
	{
		// (a new size: the pictures it had are not of it)
		if (this->Width.size() <= level) { this->Width.resize(level + 1, 0); this->Height.resize(level + 1, 0); }     // (a level never made for it)
		this->Width[level] = Width; this->Height[level] = Height;
		cpuPixels.erase(std::make_pair(Type, level));
		cpuRevision++;
	}

	void Texture::UpdateData(void* srcPTR, const uint32 level)
	{
		{
			// (as DataType lays it out: the formats a picture or a float image comes in)
			uint32 bpp = 0;
			switch (DataType)
			{
			case TextureDataType::RGBA: case TextureDataType::BGRA: bpp = 4; break;
			case TextureDataType::R8: bpp = 1; break;
			case TextureDataType::RG8: bpp = 2; break;
			case TextureDataType::R32F: bpp = 4; break;
			case TextureDataType::RG32F: bpp = 8; break;
			case TextureDataType::RGBA16F: bpp = 8; break;
			case TextureDataType::RGBA32F: bpp = 16; break;
			default: break;
			}
			if (bpp && srcPTR && level < Width.size())
			{
				const uchar* p = (const uchar*)srcPTR;
				KeepCpuPixels(level, std::vector<uchar>(p, p + (size_t)Width[level] * Height[level] * bpp));
			}
		}
	}

	const uint32 Texture::GetWidth(const uint32 level) const
	{
		return level < Width.size() ? Width[level] : 0;
	}

	const uint32 Texture::GetHeight(const uint32 level) const
	{
		return level < Height.size() ? Height[level] : 0;
	}

	std::vector<uchar> Texture::GetTextureData(const uint32 level, const int32 faceType)
	{
		std::vector<uchar> pixels;
		const std::vector<uchar>* kept = GetCpuPixels(faceType >= 0 ? (uint32)faceType : Type, level);
		if (kept) pixels = *kept;
		return pixels;
	}


}
