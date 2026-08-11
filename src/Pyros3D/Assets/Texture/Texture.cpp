//============================================================================
// Name        : Texture.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Texture
//============================================================================

#include <Pyros3D/Assets/Texture/Texture.h>
#include <Pyros3D/Ext/StringIDs/StringID.hpp>
#include <Pyros3D/Rendering/Device/GLRenderDevice.h>
#include <string.h>
#include <Pyros3D/Resources/Resources.h>
#define STB_IMAGE_IMPLEMENTATION
#include <Pyros3D/Ext/stb/stb_image.h>

namespace p3d {

	// Every Texture shares whichever backend is currently active (see
	// GetActiveRenderDevice() in IRenderDevice.h) rather than each Texture
	// needing an injected IRenderDevice* - avoids plumbing one through
	// every call site that constructs a Texture, while still respecting
	// the actual backend in use (GL vs Vulkan) - same pattern as
	// GeometryBuffer.cpp/Shaders.cpp/RenderingComponent.cpp. Previously
	// hardcoded to a static GLRenderDevice (deliberately, before this
	// backend had any real texture support) - calling any GL function
	// with no current GL context (true for a Vulkan-only window) is
	// undefined behavior, and crashed the hard way through an unloaded/
	// null GLAD function pointer once a real Vulkan texture load was
	// actually attempted.
	static IRenderDevice& Device()
	{
		return GetActiveRenderDevice();
	}

	uint32 Texture::UnitBinded = 0;

	uint32 Texture::LastUnitBinded = 0;

	Texture::Texture() : GL_ID(-1), haveImage(false), isMipMap(false), isMipMapManual(false), Anysotropic(0), cubemapFaces(0), storedSamples(0) {}

	Texture::~Texture()
	{
		// See FrameBuffer::~FrameBuffer() for why the device has to still be
		// there: without a real one, Device() hands back a static
		// GLRenderDevice and this frees a handle that backend never created,
		// through GL entry points a Vulkan build never loaded. Lua-owned
		// textures hit this on every shutdown, since sol finalizes its
		// userdata after the context has torn the device down.
		if (GL_ID != -1 && IsActiveRenderDeviceSet())
		{
			Device().DestroyTextureObject(GL_ID);
		}
	}

	void Texture::DeleteTexture() {}

	void Texture::GetInternalFormat()
	{
		Device().TranslateTextureFormat(DataType, internalFormat, internalFormat2, internalFormat3);
	}

	void Texture::GetGLModes()
	{
		Device().TranslateTextureTarget(Type, GLMode, GLSubMode);
	}

	bool Texture::LoadTexture(const std::string& Filename, const uint32 Type, bool Mipmapping, const uint32 level)
	{
		File* file = new File();
		bool status;

		if (this->GL_ID == -1)
			this->GL_ID = Device().CreateTextureObject();

		this->Type = Type;

		status = file->Open(Filename);
		if (status)
		{
			status = LoadTextureFromMemory(file->GetData(), file->Size(), Type, Mipmapping, level);
			file->Close();
		}
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
		delete file;
		return status;
	}

	bool Texture::LoadTextureFromMemory(std::vector<uchar> data, const uint32 length, const uint32 Type, bool Mipmapping, const uint32 level)
	{
		bool failed = false;
		bool ImageLoaded = false;

		// Cache the raw compressed bytes as a fallback source for scene
		// serialization - LoadTexture() (which calls this internally)
		// clears it right after setting a real Filename, since a path is
		// strictly better than embedding bytes; this only actually
		// survives for a Texture built directly via this method (no
		// path available at all).
		this->RawData = data;

		if (this->GL_ID == -1) {
			this->GL_ID = Device().CreateTextureObject();
		}

		int32 w, h, bpp;
		std::vector<uchar> pixels;

		uchar* imagePTR;
		imagePTR = stbi_load_from_memory(&data[0], length, &w, &h, &bpp ,4);

		// Check the result *before* touching w/h/imagePTR. On failure stb
		// returns NULL and leaves w/h uninitialized, so the resize() below
		// was handed a garbage size - std::length_error escaping as far as
		// terminate() - and the memcpy() read from NULL. The check existed
		// already; it just ran three lines too late to do its job. Any
		// format stb cannot decode reaches this (it has no DDS support at
		// all), so an unreadable texture killed the process instead of
		// returning false.
		ImageLoaded = imagePTR!=NULL;
		if (!ImageLoaded) {
			echo("ERROR: Failed to Open Texture");
			return false;
		}

		pixels.resize(w * h * 4 * sizeof(uchar));
		memcpy(&pixels[0], imagePTR, w * h * 4 * sizeof(uchar));
		stbi_image_free(imagePTR);

		if (this->Width.size() < level + 1)
		{
			this->Width.resize(level + 1);
			this->Height.resize(level + 1);
		}

		this->Width[level] = w;
		this->Height[level] = h;
		this->haveImage = true;
		this->Transparency = TextureTransparency::Opaque;
		this->DataType = TextureDataType::RGBA;

		if (this->GL_ID == -1)
		{
			return false;
		}

		// create default texture
		return CreateTexture(&pixels[0], Mipmapping, level);
	}

	bool Texture::CreateTexture(uchar* data, bool Mipmapping, const uint32 level, const uint32 msaa)
	{

		GetGLModes();
		GetInternalFormat();

		bool isMultisample = (Type == TextureType::Texture_Multisample);
		if (isMultisample)
			storedSamples = msaa;

		// bind
		Device().BindTextureToTarget(GLSubMode, GL_ID);

		if (Mipmapping)
		{
			Device().UploadTexture2D(GLMode, level, internalFormat, Width[level], Height[level], internalFormat2, internalFormat3, (haveImage == false ? NULL : data), true);
#if defined(GLES3)
			Device().GenerateMipmap(GLMode);
#else
			if (Type <= TextureType::CubemapNegative_Z)
			{
				cubemapFaces++;
				if (cubemapFaces == 6)
					Device().GenerateMipmap(GLSubMode);

			}
			else Device().GenerateMipmap(GLSubMode);
#endif
			isMipMap = true;

		}
		else {

#if !defined(GLES3)
			// setting manual mipmaps
			// No gles :|
			// GL_TEXTURE_2D_MULTISAMPLE doesn't support GL_TEXTURE_BASE_LEVEL/
			// MAX_LEVEL at all (same restriction as SetRepeat()/
			// SetMinMagFilter() below, which already gate on isMultisample -
			// this call was missing the same gate) - GL_INVALID_ENUM the
			// moment a real multisample texture with Mipmapping=false
			// actually exercised this path under a GLCHECKER build (every
			// multisample texture, in fact - CreateEmptyTexture()'s own
			// Mipmapping parameter is meaningless for a type GL never lets
			// mipmap in the first place).
			if (!isMultisample)
				Device().SetTextureBaseMaxLevel(GLSubMode, 0, level);

			if (!isMultisample)
			{
				Device().UploadTexture2D(GLMode, level, internalFormat, Width[level], Height[level], internalFormat2, internalFormat3, (haveImage == false ? NULL : data), false);
			}
			else {
				Device().UploadTexture2DMultisample(GLMode, msaa, internalFormat, Width[level], Height[level]);
			}
#else
			Device().UploadTexture2D(GLMode, level, internalFormat, Width[level], Height[level], internalFormat2, internalFormat3, (haveImage == false ? NULL : data), false);
#endif
			if (level > 0)
				isMipMapManual = true;
		}

		// unbind
		Device().BindTextureToTarget(GLSubMode, 0);

		// default values
#if !defined(GLES3)
		if (!isMultisample)
#endif
		{
			SetRepeat(TextureRepeat::Repeat, TextureRepeat::Repeat);
			SetMinMagFilter(TextureFilter::Linear, TextureFilter::Linear);
		}

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

		if (GL_ID == -1) {
			GL_ID = Device().CreateTextureObject();
		}
		if (GL_ID == -1)
		{
			echo("ERROR: Couldn't Create Texture");
			return false;
		}

		// Create Texture
		return CreateTexture(NULL, Mipmapping, 0, msaa);
	}

	void Texture::SetRepeat(const uint32 WrapS, const uint32 WrapT, const int32 WrapR)
	{

		SRepeat = WrapS;
		TRepeat = WrapT;
		if (WrapR > -1) RRepeat = WrapR;

		// bind
		Device().BindTextureToTarget(GLSubMode, GL_ID);

		Device().SetTextureWrapS(GLSubMode, SRepeat);
		Device().SetTextureWrapT(GLSubMode, TRepeat);

#if !defined(GLES3)
		if (WrapR > -1 && Type <= TextureType::CubemapNegative_Z)
		{
			Device().SetTextureWrapR(GLSubMode, RRepeat);
		}
#endif
		// unbind
		Device().BindTextureToTarget(GLSubMode, 0);

	}

	void Texture::SetMinMagFilter(const uint32 MinFilter, const uint32 MagFilter)
	{

		this->MinFilter = MinFilter;
		this->MagFilter = MagFilter;

		// bind
		Device().BindTextureToTarget(GLSubMode, GL_ID);

		Device().SetTextureMagFilter(GLSubMode, MagFilter);
		Device().SetTextureMinFilter(GLSubMode, MinFilter, isMipMap || isMipMapManual);

		// unbind
		Device().BindTextureToTarget(GLSubMode, 0);

	}

	void Texture::EnableCompareMode()
	{
#if !defined(GLES3)
		// USED ONLY FOR DEPTH MAPS
		// Bind
		Device().BindTextureToTarget(GLSubMode, GL_ID);

		Device().SetTextureBorderColor(GLSubMode, Vec4(1.0f, 1.0f, 1.0f, 1.0f));

		// This is to allow usage of shadow2DProj function in the shader
		Device().SetTextureCompareMode(GLSubMode);

		// Unbind
		Device().BindTextureToTarget(GLSubMode, 0);
#endif
	}

	void Texture::SetTransparency(const f32 Transparency)
	{

		this->Transparency = Transparency;

	}

	void Texture::SetBorderColor(const Vec4 &Color)
	{
#if !defined(GLES3)
		borderColor = Color;
		Device().BindTextureToTarget(GLSubMode, GL_ID);
		Device().SetTextureBorderColor(GLSubMode, borderColor);
		Device().BindTextureToTarget(GLSubMode, 0);
#endif
	}

	void Texture::Resize(const uint32 Width, const uint32 Height, const uint32 level)
	{
		Device().BindTextureToTarget(GLSubMode, GL_ID);
		this->Width[level] = Width;
		this->Height[level] = Height;

		// GL_TEXTURE_2D_MULTISAMPLE isn't a valid glTexImage2D target (only
		// glTexImage2DMultisample accepts it) - same isMultisample split as
		// CreateTexture(), which Resize() had never mirrored since nothing
		// resized a multisample texture before MSAATest.
		bool isMultisample = (Type == TextureType::Texture_Multisample);
		if (!isMultisample)
		{
			Device().UploadTexture2D(GLSubMode, level, internalFormat, Width, Height, internalFormat2, internalFormat3, NULL, isMipMap);

			if (isMipMap)
			{
				Device().GenerateMipmap(GLSubMode);
			}
		}
		else
		{
			Device().UploadTexture2DMultisample(GLSubMode, storedSamples, internalFormat, Width, Height);
		}

		Device().BindTextureToTarget(GLSubMode, 0);
	}

	void Texture::SetTextureByteAlignment(const uint32 Value)
	{
		Device().BindTextureToTarget(GLSubMode, GL_ID);

		Device().SetPixelUnpackAlignment(Value);

		Device().BindTextureToTarget(GLSubMode, 0);
	}

	void Texture::UpdateData(void* srcPTR, const uint32 level)
	{
		if (GL_ID > 0)
		{
			// bind
			Device().BindTextureToTarget(GLSubMode, GL_ID);
			Device().UploadTexture2D(GLSubMode, level, internalFormat, Width[level], Height[level], internalFormat2, internalFormat3, srcPTR, isMipMap);

			UpdateMipmap();

			// unbind
			Device().BindTextureToTarget(GLSubMode, 0);
		}
	}

	void Texture::UpdateMipmap()
	{
		if (isMipMap)
		{
			// bind
			Device().BindTextureToTarget(GLSubMode, GL_ID);

			Device().GenerateMipmap(GLSubMode);

			// unbind
			Device().BindTextureToTarget(GLSubMode, 0);
		}
	}

	void Texture::Bind()
	{
		Device().ActivateTextureUnit(UnitBinded);
		Device().BindTextureToTarget(GLSubMode, GL_ID);

		// For Transparency
		if (Transparency == TextureTransparency::Transparent)
		{
			Device().SetBlendingEnabled(true);
			Device().SetBlendFunction(BlendFunc::Src_Alpha, BlendFunc::One_Minus_Src_Alpha);
		}
		// Save Last Unit Binded
		LastUnitBinded = UnitBinded;
		// Set New Unit value
		UnitBinded++;
	}

	void Texture::Unbind()
	{
		UnitBinded--;
		if (Transparency == TextureTransparency::Transparent) Device().SetBlendingEnabled(false);
		Device().ActivateTextureUnit(UnitBinded);
		Device().BindTextureToTarget(GLSubMode, 0);

		// Save Last Unit Binded
		LastUnitBinded = UnitBinded;
	}

	uint32 Texture::GetLastBindedUnit()
	{
		return LastUnitBinded;
	}

	const uint32 Texture::GetWidth(const uint32 level) const
	{
		return Width[level];
	}

	const uint32 Texture::GetHeight(const uint32 level) const
	{
		return Height[level];
	}

	const uint32 Texture::GetBindID() const
	{
		return GL_ID;
	}

	std::vector<uchar> Texture::GetTextureData(const uint32 level, const int32 faceType)
	{
		std::vector<uchar> pixels;
#if !defined(GLES3)
		pixels.resize(Device().GetTextureDataSize(internalFormat, Width[level], Height[level]));

		// See the header - a cubemap needs its face named explicitly, or
		// GLSubMode (whatever was bound last) decides for us.
		// Bind target and read target are NOT the same thing for a
		// cubemap: glBindTexture() only accepts GL_TEXTURE_CUBE_MAP while
		// glGetTexImage() needs the individual face
		// (GL_TEXTURE_CUBE_MAP_POSITIVE_X + n). Binding with the face enum
		// is invalid and leaves nothing bound, which reads back as all
		// zeroes.
		//
		// Note which is which: TranslateTextureTarget() puts the *face* in
		// `mode` and the bindable GL_TEXTURE_CUBE_MAP in `subMode` - the
		// opposite of what the names suggest. The rest of this class
		// already relies on that (it binds GLSubMode and uploads to
		// GLMode, just above), so matching it here keeps one convention
		// rather than two.
		uint32 bindTarget = GLSubMode;
		uint32 readTarget = GLMode;
		if (faceType >= 0)
		{
			uint32 mode = 0, subMode = 0;
			Device().TranslateTextureTarget((uint32)faceType, mode, subMode);
			bindTarget = subMode;
			readTarget = mode;
		}

		Device().BindTextureToTarget(bindTarget, GL_ID);
		Device().ReadTexturePixels(readTarget, level, internalFormat2, internalFormat3, &pixels[0]);
		Device().BindTextureToTarget(bindTarget, 0);
#endif

		return pixels;
	}


}
