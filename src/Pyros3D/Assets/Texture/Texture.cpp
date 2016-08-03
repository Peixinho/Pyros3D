//============================================================================
// Name        : Texture.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Texture
//============================================================================

#include <Pyros3D/Assets/Texture/Texture.h>
#include <Pyros3D/Ext/StringIDs/StringID.hpp>
#include <Pyros3D/Other/PyrosGL.h>

#if defined(LODEPNG)
#include "../../Ext/lodepng/lodepng.h"
#else
#include <SFML/Graphics.hpp>
#include <string.h>
#endif
#include <Pyros3D/Resources/Resources.h>

namespace p3d {

	uint32 Texture::UnitBinded = 0;
	
	uint32 Texture::LastUnitBinded = 0;

	Texture::Texture() : GL_ID(-1), haveImage(false), isMipMap(false), isMipMapManual(false), Anysotropic(0), cubemapFaces(0) {}

	Texture::~Texture()
	{
		if (GL_ID != -1)
		{
			GLCHECKER(glDeleteTextures(1, (GLuint*)&GL_ID));
		}
	}

	void Texture::DeleteTexture() {}

	void Texture::GetInternalFormat()
	{
		switch (DataType)
		{

#if defined(GLES2)
		case TextureDataType::DepthComponent:
		case TextureDataType::DepthComponent16:
		case TextureDataType::DepthComponent24:
		case TextureDataType::DepthComponent32:
			internalFormat = GL_RGBA;
			internalFormat2 = GL_RGBA;
			internalFormat3 = GL_UNSIGNED_BYTE;
			break;
#else
		case TextureDataType::DepthComponent:
		case TextureDataType::DepthComponent24:
			internalFormat = GL_DEPTH_COMPONENT24;
			internalFormat2 = GL_DEPTH_COMPONENT;
			internalFormat3 = GL_FLOAT;
			break;
		case TextureDataType::DepthComponent16:
			internalFormat = GL_DEPTH_COMPONENT16;
			internalFormat2 = GL_DEPTH_COMPONENT;
			internalFormat3 = GL_FLOAT;
			break;
		case TextureDataType::DepthComponent32:
			internalFormat = GL_DEPTH_COMPONENT32;
			internalFormat2 = GL_DEPTH_COMPONENT;
			internalFormat3 = GL_FLOAT;
			break;
		case TextureDataType::R16F:
			internalFormat = GL_R16F;
			internalFormat2 = GL_RED;
			internalFormat3 = GL_FLOAT;
			break;
		case TextureDataType::R32F:
			internalFormat = GL_R32F;
			internalFormat2 = GL_RED;
			internalFormat3 = GL_FLOAT;
			break;
		case TextureDataType::R16I:
			internalFormat = GL_R16I;
			internalFormat2 = GL_R;
			internalFormat3 = GL_UNSIGNED_BYTE;
			break;
		case TextureDataType::R32I:
			internalFormat = GL_R32I;
			internalFormat2 = GL_R;
			internalFormat3 = GL_UNSIGNED_BYTE;
			break;
		case TextureDataType::RG:
			internalFormat = GL_RG8;
			internalFormat2 = GL_RG;
			internalFormat3 = GL_UNSIGNED_BYTE;
			break;
		case TextureDataType::RG16F:
			internalFormat = GL_RG16F;
			internalFormat2 = GL_RG;
			internalFormat3 = GL_FLOAT;
			break;
		case TextureDataType::RG32F:
			internalFormat = GL_RG32F;
			internalFormat2 = GL_RG;
			internalFormat3 = GL_FLOAT;
			break;
		case TextureDataType::RG16I:
			internalFormat = GL_RG16I;
			internalFormat2 = GL_RG;
			internalFormat3 = GL_UNSIGNED_BYTE;
			break;
		case TextureDataType::RG32I:
			internalFormat = GL_RG32I;
			internalFormat2 = GL_RG;
			internalFormat3 = GL_UNSIGNED_BYTE;
			break;
		case TextureDataType::RGB:
			internalFormat = GL_RGB8;
			internalFormat2 = GL_RGB;
			internalFormat3 = GL_UNSIGNED_BYTE;
			break;
		case TextureDataType::RGB16F:
			internalFormat = GL_RGB16F;
			internalFormat2 = GL_RGB;
			internalFormat3 = GL_FLOAT;
			break;
		case TextureDataType::RGB32F:
			internalFormat = GL_RGB32F;
			internalFormat2 = GL_RGB;
			internalFormat3 = GL_FLOAT;
			break;
		case TextureDataType::RGB16I:
			internalFormat = GL_RGB16I;
			internalFormat2 = GL_RGB;
			internalFormat3 = GL_UNSIGNED_BYTE;
			break;
		case TextureDataType::RGB32I:
			internalFormat = GL_RGB32I;
			internalFormat2 = GL_RGB;
			internalFormat3 = GL_UNSIGNED_BYTE;
			break;
		case TextureDataType::RGBA16F:
			internalFormat = GL_RGBA16F;
			internalFormat2 = GL_RGBA;
			internalFormat3 = GL_FLOAT;
			break;
		case TextureDataType::RGBA32F:
			internalFormat = GL_RGBA32F;
			internalFormat2 = GL_RGBA;
			internalFormat3 = GL_FLOAT;
			break;
		case TextureDataType::RGBA16I:
			internalFormat = GL_RGBA16I;
			internalFormat2 = GL_RGBA;
			internalFormat3 = GL_UNSIGNED_BYTE;
			break;
		case TextureDataType::RGBA32I:
			internalFormat = GL_RGBA32I;
			internalFormat2 = GL_RGBA;
			internalFormat3 = GL_UNSIGNED_BYTE;
			break;
		case TextureDataType::R:
			internalFormat = GL_R8;
			internalFormat2 = GL_R;
			internalFormat3 = GL_UNSIGNED_BYTE;
			break;
		case TextureDataType::BGR:
			internalFormat = GL_RGB8;
			internalFormat2 = GL_BGR;
			internalFormat3 = GL_UNSIGNED_BYTE;
			break;
		case TextureDataType::BGRA:
			internalFormat = GL_RGBA8;
			internalFormat2 = GL_BGRA;
			internalFormat3 = GL_UNSIGNED_BYTE;
			break;
#endif
		case TextureDataType::ALPHA:
			internalFormat = GL_ALPHA;
			internalFormat2 = GL_ALPHA;
			internalFormat3 = GL_UNSIGNED_BYTE;
			break;
		case TextureDataType::LUMINANCE:
			internalFormat = GL_LUMINANCE;
			internalFormat2 = GL_LUMINANCE;
			internalFormat3 = GL_UNSIGNED_BYTE;
			break;
			break;
		case TextureDataType::LUMINANCE_ALPHA:
			internalFormat = GL_LUMINANCE_ALPHA;
			internalFormat2 = GL_LUMINANCE_ALPHA;
			internalFormat3 = GL_UNSIGNED_BYTE;
			break;
			break;
		case TextureDataType::RGBA:
		default:
			internalFormat = GL_RGBA;
			internalFormat2 = GL_RGBA;
			internalFormat3 = GL_UNSIGNED_BYTE;
			break;
		}
	}
	
	void Texture::GetGLModes()
	{
		// default texture
		switch (Type) {
		case TextureType::CubemapNegative_X:
			GLMode = GL_TEXTURE_CUBE_MAP_NEGATIVE_X;
			GLSubMode = GL_TEXTURE_CUBE_MAP;
			break;
		case TextureType::CubemapNegative_Y:
			GLMode = GL_TEXTURE_CUBE_MAP_NEGATIVE_Y;
			GLSubMode = GL_TEXTURE_CUBE_MAP;
			break;
		case TextureType::CubemapNegative_Z:
			GLMode = GL_TEXTURE_CUBE_MAP_NEGATIVE_Z;
			GLSubMode = GL_TEXTURE_CUBE_MAP;
			break;
		case TextureType::CubemapPositive_X:
			GLMode = GL_TEXTURE_CUBE_MAP_POSITIVE_X;
			GLSubMode = GL_TEXTURE_CUBE_MAP;
			break;
		case TextureType::CubemapPositive_Y:
			GLMode = GL_TEXTURE_CUBE_MAP_POSITIVE_Y;
			GLSubMode = GL_TEXTURE_CUBE_MAP;
			break;
		case TextureType::CubemapPositive_Z:
			GLMode = GL_TEXTURE_CUBE_MAP_POSITIVE_Z;
			GLSubMode = GL_TEXTURE_CUBE_MAP;
			break;
		case TextureType::Texture:
		default:
			GLMode = GL_TEXTURE_2D;
			GLSubMode = GL_TEXTURE_2D;
			break;
		}
	}

	bool Texture::LoadTexture(const std::string& Filename, const uint32 Type, bool Mipmapping, const uint32 level)
	{
		File* file = new File();
		bool status;

		if (this->GL_ID == -1)
			GLCHECKER(glGenTextures(1, (GLuint*)&this->GL_ID));

		this->Type = Type;

		if (file->Open(Filename))
		{

#if !defined(GLES2)
			// DDS
			if (Filename.substr(Filename.find_last_of(".") + 1) == "DDS" || Filename.substr(Filename.find_last_of(".") + 1) == "dds")
			{
				status = LoadDDS(&file->GetData()[0]);
			}
#else
			if (Filename.substr(Filename.find_last_of(".") + 1) == "PKM" || Filename.substr(Filename.find_last_of(".") + 1) == "pkm")
			{
				status = LoadETC1(&file->GetData()[0]);
			}
#endif
			else {
					status = LoadTextureFromMemory(file->GetData(), file->Size(), Type, Mipmapping, level);
			}

			file->Close();
		}
		if (!file->Open(Filename) || !status) {
			echo("ERROR: Couldn't find texture file or failed to load");
#if !defined(GLES2)
			status = LoadDDS((uchar*)MISSING_TEXTURE);
#else

#endif
		}
		delete file;
		return status;
	}

	bool Texture::LoadTextureFromMemory(std::vector<uchar> data, const uint32 length, const uint32 Type, bool Mipmapping, const uint32 level)
	{
		bool failed = false;
		bool ImageLoaded = false;

		if (this->GL_ID == -1) {
			GLCHECKER(glGenTextures(1, (GLuint*)&this->GL_ID));
		}

		uint32 w, h;
		std::vector<uchar> pixels;

#if !defined(LODEPNG)
		// USING SFML
		sf::Image sfImage;
		ImageLoaded = sfImage.loadFromMemory(&data[0], length);
		w = sfImage.getSize().x;
		h = sfImage.getSize().y;
		// Copy Pixels
		pixels.resize(w * h * 4 * sizeof(uchar));
		memcpy(&pixels[0], sfImage.getPixelsPtr(), w * h * 4 * sizeof(uchar));
#else
		//USING LODEPNG ( USEFUL FOR EMSCRIPTEN AND ANDROID )
		uchar* imagePTR;
		ImageLoaded = (lodepng_decode32(&imagePTR, &w, &h, &data[0], length) != 0 ? false : true);
		pixels.resize(w * h * 4 * sizeof(uchar));
		memcpy(&pixels[0], imagePTR, w * h * 4 * sizeof(uchar));
#endif
		
		if (!ImageLoaded) {
			echo("ERROR: Failed to Open Texture");
			return false;
		}

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

	bool Texture::CreateTexture(uchar* data, bool Mipmapping, const uint32 level)
	{

		GetGLModes();
		GetInternalFormat();

		// bind
		GLCHECKER(glBindTexture(GLSubMode, GL_ID));

		if (Mipmapping)
		{
#if defined(GLES2)
			GLCHECKER(glTexImage2D(GLMode, level, internalFormat, Width[level], Height[level], 0, internalFormat2, internalFormat3, (haveImage == false ? NULL : data)));
			GLCHECKER(glGenerateMipmap(GLMode));
#else

#if defined(GLEW_VERSION_2_1)
				GLCHECKER(glTexImage2D(GLMode, level, internalFormat, Width[level], Height[level], 0, internalFormat2, internalFormat3, (haveImage == false ? NULL : data)));
				if (GLSubMode == GL_TEXTURE_CUBE_MAP)
				{
					cubemapFaces++;
					if (cubemapFaces == 6)
						GLCHECKER(glGenerateMipmap(GLSubMode));

				} else GLCHECKER(glGenerateMipmap(GLSubMode));
#else
				GLCHECKER(gluBuild2DMipmaps(GLMode, internalFormat, Width[level], Height[level], internalFormat2, internalFormat3, (haveImage == false ? NULL : data)));
#endif
#endif
			isMipMap = true;

		}
		else {

#if !defined(GLES2)
			// setting manual mipmaps
			// No gles :|
			GLCHECKER(glTexParameteri(GLSubMode, GL_TEXTURE_BASE_LEVEL, 0));
			GLCHECKER(glTexParameteri(GLSubMode, GL_TEXTURE_MAX_LEVEL, level));
#endif
			GLCHECKER(glTexImage2D(GLMode, level, internalFormat, Width[level], Height[level], 0, internalFormat2, internalFormat3, (haveImage == false ? NULL : data)));

			if (level>0)
				isMipMapManual = true;
		}

		// unbind
		GLCHECKER(glBindTexture(GLSubMode, 0));

		// default values
		SetRepeat(TextureRepeat::Repeat, TextureRepeat::Repeat);
		SetMinMagFilter(TextureFilter::Linear, TextureFilter::Linear);

		return true;
	}

	bool Texture::CreateEmptyTexture(const uint32 Type, const uint32 TextureDataType, const int32 width, const int32 height, bool Mipmapping, const uint32 level)
	{
		if (this->Width.size()<level + 1)
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
			GLCHECKER(glGenTextures(1, (GLuint*)&GL_ID));
		}
		if (GL_ID == -1)
		{
			echo("ERROR: Couldn't Create Texture");
			return false;
		}

		// Create Texture
		return CreateTexture(NULL, Mipmapping);
	}

	void Texture::SetAnysotropy(const uint32 Anysotropic)
	{
		// bind
		GLCHECKER(glBindTexture(GLSubMode, GL_ID));

		this->Anysotropic = Anysotropic;

		if (Anysotropic>0)
		{
			f32 AnysotropicMax;
			GLCHECKER(glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &AnysotropicMax));
			if (AnysotropicMax > Anysotropic) {
				GLCHECKER(glTexParameteri(GLSubMode, GL_TEXTURE_MAX_ANISOTROPY_EXT, Anysotropic));
			}
			else if (AnysotropicMax>0)
				GLCHECKER(glTexParameteri(GLSubMode, GL_TEXTURE_MAX_ANISOTROPY_EXT, AnysotropicMax));
		}
		else {
			GLCHECKER(glTexParameteri(GLSubMode, GL_TEXTURE_MAX_ANISOTROPY_EXT, 0));
		}

		// unbind
		GLCHECKER(glBindTexture(GLSubMode, 0));

	}

	void Texture::SetRepeat(const uint32 WrapS, const uint32 WrapT, const int32 WrapR)
	{

		SRepeat = WrapS;
		TRepeat = WrapT;
		if (WrapR>-1) RRepeat = WrapR;

		// bind
		GLCHECKER(glBindTexture(GLSubMode, GL_ID));

		switch (SRepeat)
		{
#if defined(GLES2)
		case TextureRepeat::ClampToEdge:
			GLCHECKER(glTexParameteri(GLSubMode, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
			break;
		case TextureRepeat::Repeat:
		default:
			GLCHECKER(glTexParameteri(GLSubMode, GL_TEXTURE_WRAP_S, GL_REPEAT));
			break;
#else
		case TextureRepeat::ClampToEdge:
			GLCHECKER(glTexParameteri(GLSubMode, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
			break;
		case TextureRepeat::ClampToBorder:
			GLCHECKER(glTexParameteri(GLSubMode, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER));
			break;
		case TextureRepeat::Clamp:
			GLCHECKER(glTexParameteri(GLSubMode, GL_TEXTURE_WRAP_S, GL_CLAMP));
			break;
		case TextureRepeat::Repeat:
		default:
			GLCHECKER(glTexParameteri(GLSubMode, GL_TEXTURE_WRAP_S, GL_REPEAT));
			break;
#endif
		};

		switch (TRepeat)
		{
#if defined(GLES2)
		case TextureRepeat::ClampToEdge:
			GLCHECKER(glTexParameteri(GLSubMode, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));
			break;
		case TextureRepeat::Repeat:
		default:
			GLCHECKER(glTexParameteri(GLSubMode, GL_TEXTURE_WRAP_T, GL_REPEAT));
			break;
#else
		case TextureRepeat::ClampToEdge:
			GLCHECKER(glTexParameteri(GLSubMode, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));
			break;

		case TextureRepeat::ClampToBorder:
			GLCHECKER(glTexParameteri(GLSubMode, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER));
			break;
		case TextureRepeat::Clamp:
			GLCHECKER(glTexParameteri(GLSubMode, GL_TEXTURE_WRAP_T, GL_CLAMP));
			break;
		case TextureRepeat::Repeat:
		default:
			GLCHECKER(glTexParameteri(GLSubMode, GL_TEXTURE_WRAP_T, GL_REPEAT));
			break;
#endif
		};

#if !defined(GLES2)
		if (WrapR>-1 && GLSubMode == GL_TEXTURE_CUBE_MAP)
		{
			switch (RRepeat)
			{
			case TextureRepeat::ClampToEdge:
				GLCHECKER(glTexParameteri(GLSubMode, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE));
				break;
			case TextureRepeat::ClampToBorder:
				GLCHECKER(glTexParameteri(GLSubMode, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_BORDER));
				break;
			case TextureRepeat::Clamp:
				GLCHECKER(glTexParameteri(GLSubMode, GL_TEXTURE_WRAP_R, GL_CLAMP));
				break;
			case TextureRepeat::Repeat:
			default:
				GLCHECKER(glTexParameteri(GLSubMode, GL_TEXTURE_WRAP_R, GL_REPEAT));
				break;
			};
		}
#endif
		// unbind
		GLCHECKER(glBindTexture(GLSubMode, 0));

	}

	void Texture::SetMinMagFilter(const uint32 MinFilter, const uint32 MagFilter)
	{

		this->MinFilter = MinFilter;
		this->MagFilter = MagFilter;

		// bind
		GLCHECKER(glBindTexture(GLSubMode, GL_ID));

		switch (MagFilter)
		{
		case TextureFilter::Nearest:
			GLCHECKER(glTexParameteri(GLSubMode, GL_TEXTURE_MAG_FILTER, GL_NEAREST));
			break;
		default:
			GLCHECKER(glTexParameteri(GLSubMode, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
			break;
		}

		switch (MinFilter)
		{
		case TextureFilter::Nearest:
		case TextureFilter::NearestMipmapNearest:
			if (isMipMap || isMipMapManual) {
				GLCHECKER(glTexParameteri(GLSubMode, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST));
			}
			else {
				GLCHECKER(glTexParameteri(GLSubMode, GL_TEXTURE_MIN_FILTER, GL_NEAREST));
			}
			break;
		case TextureFilter::NearestMipmapLinear:
			if (isMipMap || isMipMapManual) {
				GLCHECKER(glTexParameteri(GLSubMode, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_LINEAR));
			}
			else {
				GLCHECKER(glTexParameteri(GLSubMode, GL_TEXTURE_MIN_FILTER, GL_NEAREST));
			}
			break;
		case TextureFilter::LinearMipmapNearest:
			if (isMipMap || isMipMapManual) {
				GLCHECKER(glTexParameteri(GLSubMode, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_NEAREST));
			}
			else {
				GLCHECKER(glTexParameteri(GLSubMode, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
			}
			break;
		case TextureFilter::Linear:
		case TextureFilter::LinearMipmapLinear:
		default:
			if (isMipMap || isMipMapManual)
			{
				GLCHECKER(glTexParameteri(GLSubMode, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR));
			}
			else {
				GLCHECKER(glTexParameteri(GLSubMode, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
			}
			break;
		}

		// unbind
		GLCHECKER(glBindTexture(GLSubMode, 0));

	}
	
	void Texture::EnableCompareMode()
	{
#if !defined(GLES2)
		// USED ONLY FOR DEPTH MAPS
		// Bind
		GLCHECKER(glBindTexture(GLSubMode, GL_ID));

		GLfloat l_ClampColor[] = { 1.0, 1.0, 1.0, 1.0 };
		GLCHECKER(glTexParameterfv(GLSubMode, GL_TEXTURE_BORDER_COLOR, l_ClampColor));

		// This is to allow usage of shadow2DProj function in the shader
		GLCHECKER(glTexParameteri(GLSubMode, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_R_TO_TEXTURE));
		GLCHECKER(glTexParameteri(GLSubMode, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL));
		// glTexParameteri(GLSubMode, GL_DEPTH_TEXTURE_MODE, GL_INTENSITY); // Not used Anymore
		GLCHECKER(glTexParameteri(GLSubMode, GL_DEPTH_TEXTURE_MODE, GL_LUMINANCE));

		// Unbind
		GLCHECKER(glBindTexture(GLSubMode, 0));
#endif
	}
	
	void Texture::SetTransparency(const f32 Transparency)
	{

		this->Transparency = Transparency;

	}

	void Texture::Resize(const uint32 Width, const uint32 Height, const uint32 level)
	{
		GLCHECKER(glBindTexture(GLSubMode, GL_ID));
		this->Width[level] = Width;
		this->Height[level] = Height;
		GLCHECKER(glTexImage2D(GLSubMode, level, internalFormat, Width, Height, 0, internalFormat2, internalFormat3, NULL));

		if (isMipMap)
		{
#if defined(GLES2)
			GLCHECKER(glGenerateMipmap(GLSubMode));
#else
#if defined(GLEW_VERSION_2_1)
			
				GLCHECKER(glGenerateMipmap(GLSubMode));
#else
				GLCHECKER(gluBuild2DMipmaps(GLSubMode, internalFormat, Width, Height, internalFormat2, internalFormat3, NULL));
#endif
#endif
		}

		GLCHECKER(glBindTexture(GLSubMode, 0));
	}

	void Texture::SetTextureByteAlignment(const uint32 Value)
	{
		GLCHECKER(glBindTexture(GLSubMode, GL_ID));

		GLCHECKER(glPixelStorei(GL_UNPACK_ALIGNMENT, Value));

		GLCHECKER(glBindTexture(GLSubMode, 0));
	}

	void Texture::UpdateData(void* srcPTR, const uint32 level)
	{
		if (GL_ID>0)
		{
			// bind
			GLCHECKER(glBindTexture(GLSubMode, GL_ID));
			GLCHECKER(glTexImage2D(GLSubMode, level, internalFormat, Width[level], Height[level], 0, internalFormat2, internalFormat3, srcPTR));

			UpdateMipmap();

			// unbind
			GLCHECKER(glBindTexture(GLSubMode, 0));
		}
	}

	void Texture::UpdateMipmap()
	{
		if (isMipMap)
		{
			// bind
			GLCHECKER(glBindTexture(GLSubMode, GL_ID));

#if defined(GLES2)
			GLCHECKER(glGenerateMipmap(GLSubMode));
#else
			if (GLEW_VERSION_2_1)
			{
				GLCHECKER(glGenerateMipmap(GLSubMode));
			}
			else {
				// No way for old stupid devices without uploading texture again
				//GLCHECKER(gluBuild2DMipmaps(GLSubMode, internalFormat, Width[0], Height[0], internalFormat2, internalFormat3, (haveImage == false ? NULL : &pixels[0][0]))); // its 0 hardcoded because otherwise there won't mipmaps created on the fly
			}
#endif            
			// unbind
			GLCHECKER(glBindTexture(GLSubMode, 0));
		}
	}

	void Texture::Bind()
	{
		GLCHECKER(glActiveTexture(GL_TEXTURE0 + UnitBinded));
		GLCHECKER(glBindTexture(GLSubMode, GL_ID));

		// For Transparency
		if (Transparency == TextureTransparency::Transparent)
		{
			GLCHECKER(glEnable(GL_BLEND));
			GLCHECKER(glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA));
		}
		// Save Last Unit Binded
		LastUnitBinded = UnitBinded;
		// Set New Unit value
		UnitBinded++;
	}

	void Texture::Unbind()
	{
		UnitBinded--;
		if (Transparency == TextureTransparency::Transparent) GLCHECKER(glDisable(GL_BLEND));
		GLCHECKER(glActiveTexture(GL_TEXTURE0 + UnitBinded));
		GLCHECKER(glBindTexture(GLSubMode, 0));

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

	std::vector<uchar> Texture::GetTextureData(const uint32 level)
	{
		std::vector<uchar> pixels;
#if !defined(GLES2)

		switch (internalFormat)
		{
			case GL_DEPTH_COMPONENT16:
				pixels.resize(sizeof(uchar) * 2 * Width[level] * Height[level]);
				break;
			case GL_DEPTH_COMPONENT24:
				pixels.resize(sizeof(uchar) * 3 * Width[level] * Height[level]);
				break;
			case GL_DEPTH_COMPONENT32:
				pixels.resize(sizeof(f32)*Width[level] * Height[level]);
				break;
			case GL_R16F:
				pixels.resize(sizeof(uchar) * 2 * Width[level] * Height[level]);
				break;
			case GL_R32F:
				pixels.resize(sizeof(f32)*Width[level] * Height[level]);
				break;
			case GL_RG8:
				pixels.resize(sizeof(uchar)*Width[level] * Height[level] * 2);
				break;
			case GL_R16I:
				pixels.resize(sizeof(uchar) * 2 * Width[level] * Height[level]);
				break;
			case GL_R32I:
				pixels.resize(sizeof(int32)*Width[level] * Height[level]);
				break;
			case GL_RG16F:
				pixels.resize(sizeof(uchar) * 2 * Width[level] * Height[level] * 2);
				break;
			case GL_RG32F:
				pixels.resize(sizeof(f32)*Width[level] * Height[level] * 2);
				break;
			case GL_RG16I:
				pixels.resize(sizeof(uchar) * 2 * Width[level] * Height[level]);
				break;
			case GL_RG32I:
				pixels.resize(sizeof(int32)*Width[level] * Height[level] * 2);
				break;
			case GL_RGB8:
				pixels.resize(sizeof(uchar)*Width[level] * Height[level] * 2);
				break;
			case GL_RGB16F:
				pixels.resize(sizeof(uchar) * 2 * Width[level] * Height[level] * 3);
				break;
			case GL_RGB32F:
				pixels.resize(sizeof(f32)*Width[level] * Height[level] * 3);
				break;
			case GL_RGB16I:
				pixels.resize(sizeof(uchar) * 2 * Width[level] * Height[level] * 3);
				break;
			case GL_RGB32I:
				pixels.resize(sizeof(int32)*Width[level] * Height[level] * 3);
				break;
			case GL_RGBA16F:
				pixels.resize(sizeof(uchar) * 2 * Width[level] * Height[level] * 4);
				break;
			case GL_RGBA32F:
				pixels.resize(sizeof(f32)*Width[level] * Height[level] * 4);
				break;
			case GL_RGBA16I:
				pixels.resize(sizeof(uchar) * 2 * Width[level] * Height[level] * 4);
				break;
			case GL_RGBA32I:
				pixels.resize(sizeof(int32)*Width[level] * Height[level] * 4);
				break;
			case GL_R8:
				pixels.resize(sizeof(uchar)*Width[level] * Height[level] * 4);
				break;
			case GL_ALPHA:
				pixels.resize(sizeof(uchar)*Width[level] * Height[level]);
				break;
			default:
				pixels.resize(sizeof(uchar)*Width[level] * Height[level] * 4);
				break;
		}
		GLCHECKER(glBindTexture(GLSubMode, GL_ID));
		GLCHECKER(glGetTexImage(GLSubMode, level, internalFormat2, internalFormat3, &pixels[0]));
		GLCHECKER(glBindTexture(GLSubMode, 0));
#endif

		return pixels;
	}

#if !defined(GLES2)
	bool Texture::LoadDDS(uchar* data, bool Mipmapping, const uint32 level)
	{

#define FOURCC_DXT1 0x31545844 // Equivalent to "DXT1" in ASCII
#define FOURCC_DXT3 0x33545844 // Equivalent to "DXT3" in ASCII
#define FOURCC_DXT5 0x35545844 // Equivalent to "DXT5" in ASCII

		uint32 w, h, mipMapCount;

		h = *(unsigned int*)&(data[12]);
		w = *(unsigned int*)&(data[16]);
		mipMapCount = *(unsigned int*)&(data[28]);
		uint32 fourCC = *(unsigned int*)&(data[84]);
		uint32 format = GL_COMPRESSED_RGBA_S3TC_DXT1_EXT;

		switch (fourCC)
		{
		case FOURCC_DXT1:
			format = GL_COMPRESSED_RGBA_S3TC_DXT1_EXT;
			break;
		case FOURCC_DXT3:
			format = GL_COMPRESSED_RGBA_S3TC_DXT3_EXT;
			break;
		case FOURCC_DXT5:
			format = GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
			this->DataType = TextureDataType::RGB;
			break;
		}

		GetGLModes();
		GetInternalFormat();

		Bind();

		this->haveImage = true;
		this->Transparency = TextureTransparency::Opaque;

		uint32 blockSize = (format == GL_COMPRESSED_RGBA_S3TC_DXT1_EXT) ? 8 : 16;
		uint32 offset = 124 + 4;

		if (this->Width.size() < mipMapCount)
		{
			this->Width.resize(mipMapCount);
			this->Height.resize(mipMapCount);
		}

		if (mipMapCount == 1 && !Mipmapping)
		{
			uint32 size = ((w + 3) / 4) * ((h + 3) / 4) * blockSize;
			GLCHECKER(glCompressedTexImage2D(GL_TEXTURE_2D, level, format, w, h, 0, size, &data[offset]));
			this->Width[level] = w;
			this->Height[level] = h;

			offset += size;
			w = Max(1, w * 0.5f);
			h = Max(1, h * 0.5f);
		}
		else {
			for (uint32 lvl = 0; lvl < mipMapCount && (w || h); ++lvl)
			{
				uint32 size = ((w + 3) / 4) * ((h + 3) / 4) * blockSize;

				GLCHECKER(glCompressedTexImage2D(GL_TEXTURE_2D, lvl, format, w, h, 0, size, &data[offset]));
				this->Width[lvl] = w;
				this->Height[lvl] = h;

				offset += size;
				w = Max(1, w * 0.5f);
				h = Max(1, h * 0.5f);
			}
		}

		if (mipMapCount == 1 && Mipmapping)
			GLCHECKER(glGenerateMipmap(GLSubMode));

		Unbind();

		// default values
		SetRepeat(TextureRepeat::Repeat, TextureRepeat::Repeat);
		SetMinMagFilter(TextureFilter::Linear, TextureFilter::Linear);

		return true;
	}

#else

	uint16 Texture::swapBytes(uint16 aData)
	{
		return (aData << 8) | (aData >> 8);
	}

	bool Texture::LoadETC1(uchar* data, bool Mipmapping, const uint32 level)
	{
#if defined(EMSCRIPTEN)
		return false;
#else
		uint16 r, w, h, mipMapCount;
		uint16 extWidth, extHeight;

		memcpy(&r, &data[8], 2);
		extWidth = swapBytes(r);
		memcpy(&r, &data[10], 2);
		extHeight = swapBytes(r);
		memcpy(&r, &data[12], 2);
		w = swapBytes(r);
		memcpy(&r, &data[14], 2);
		h = swapBytes(r);

		Bind();

		this->haveImage = true;
		this->Transparency = TextureTransparency::Opaque;

		GetGLModes();
		GetInternalFormat();

		uint32 size = ((extWidth >> 2) * (extHeight >> 2)) << 3;

		GLCHECKER(glCompressedTexImage2D(GL_TEXTURE_2D, 0, GL_ETC1_RGB8_OES, extWidth, extHeight, 0, size, &data[16]));

		if (this->Width.size() < level + 1)
		{
			this->Width.resize(level + 1);
			this->Height.resize(level + 1);
		}

		this->Width[level] = extWidth;
		this->Height[level] = extHeight;

		if (Mipmapping)
			GLCHECKER(glGenerateMipmap(GLSubMode));

		Unbind();

		// default values
		SetRepeat(TextureRepeat::Repeat, TextureRepeat::Repeat);
		SetMinMagFilter(TextureFilter::Linear, TextureFilter::Linear);

		return true;
#endif
	}
#endif

}
