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
#include <string.h>
#include <Pyros3D/Resources/Resources.h>
#define STB_IMAGE_IMPLEMENTATION
#include <Pyros3D/Ext/stb/stb_image.h>

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
			#if defined(GLES3)
			internalFormat3 = GL_UNSIGNED_INT;
			#else
			internalFormat3 = GL_FLOAT;
			#endif
			break;
		case TextureDataType::DepthComponent16:
			internalFormat = GL_DEPTH_COMPONENT16;
			internalFormat2 = GL_DEPTH_COMPONENT;
			#if defined(GLES3)
			internalFormat3 = GL_UNSIGNED_INT;
			#else
			internalFormat3 = GL_FLOAT;
			#endif
			break;
		case TextureDataType::DepthComponent32:
			internalFormat = GL_DEPTH_COMPONENT32F;
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
			internalFormat2 = GL_R8;
			internalFormat3 = GL_UNSIGNED_BYTE;
			break;
		case TextureDataType::R32I:
			internalFormat = GL_R32I;
			internalFormat2 = GL_R8;
			internalFormat3 = GL_UNSIGNED_BYTE;
			break;
		case TextureDataType::RG8:
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
		case TextureDataType::RGB8:
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
		case TextureDataType::R8:
			internalFormat = GL_R8;
			internalFormat2 = GL_RED;
			internalFormat3 = GL_UNSIGNED_BYTE;
			break;
#if !defined(GLES3)
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
#endif
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
#if !defined(GLES2) && !defined(GLES3)
		case TextureType::Texture_Multisample:
			GLMode = GL_TEXTURE_2D_MULTISAMPLE;
			GLSubMode = GL_TEXTURE_2D_MULTISAMPLE;
			break;
#endif
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

		status = file->Open(Filename);
		if (status)
		{
			status = LoadTextureFromMemory(file->GetData(), file->Size(), Type, Mipmapping, level);
			file->Close();
		}
		if (!status) {
			echo("ERROR: Couldn't find texture file or failed to load, loading default one...");

			int i, j, c;
			GLubyte checkImage[4][4][4];
			for (i = 0; i < 4; i++) {
				for (j = 0; j < 4; j++) {
					if ((i%2==0 && j%2!=0) || (i%2!=0 && j%2==0)) c=255;
					else c=0;
					checkImage[i][j][0] = c;
					checkImage[i][j][1] = c;
					checkImage[i][j][2] = c;
					checkImage[i][j][3] = 255;
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

		if (this->GL_ID == -1) {
			GLCHECKER(glGenTextures(1, (GLuint*)&this->GL_ID));
		}

		int32 w, h, bpp;
		std::vector<uchar> pixels;

		uchar* imagePTR;
		imagePTR = stbi_load_from_memory(&data[0], length, &w, &h, &bpp ,4);
		pixels.resize(w * h * 4 * sizeof(uchar));
		memcpy(&pixels[0], imagePTR, w * h * 4 * sizeof(uchar));
		stbi_image_free(imagePTR);
		ImageLoaded = imagePTR!=NULL;
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

	bool Texture::CreateTexture(uchar* data, bool Mipmapping, const uint32 level, const uint32 msaa)
	{

		GetGLModes();
		GetInternalFormat();

		// bind
		GLCHECKER(glBindTexture(GLSubMode, GL_ID));

		if (Mipmapping)
		{
#if defined(GLES2) || defined(GLES3)
			GLCHECKER(glTexImage2D(GLMode, level, internalFormat, Width[level], Height[level], 0, internalFormat2, internalFormat3, (haveImage == false ? NULL : data)));
			GLCHECKER(glGenerateMipmap(GLMode));
#else

			GLCHECKER(glTexImage2D(GLMode, level, internalFormat, Width[level], Height[level], 0, internalFormat2, internalFormat3, (haveImage == false ? NULL : data)));
			if (GLSubMode == GL_TEXTURE_CUBE_MAP)
			{
				cubemapFaces++;
				if (cubemapFaces == 6)
					GLCHECKER(glGenerateMipmap(GLSubMode));

			}
			else GLCHECKER(glGenerateMipmap(GLSubMode));
#endif
			isMipMap = true;

		}
		else {

#if !defined(GLES2) && !defined(GLES3)
			// setting manual mipmaps
			// No gles :|
			GLCHECKER(glTexParameteri(GLSubMode, GL_TEXTURE_BASE_LEVEL, 0));
			GLCHECKER(glTexParameteri(GLSubMode, GL_TEXTURE_MAX_LEVEL, level));

			if (GLMode != GL_TEXTURE_2D_MULTISAMPLE)
			{
				GLCHECKER(glTexImage2D(GLMode, level, internalFormat, Width[level], Height[level], 0, internalFormat2, internalFormat3, (haveImage == false ? NULL : data)));
			}
			else {
				GLCHECKER(glTexImage2DMultisample(GLMode, msaa, internalFormat, Width[level], Height[level], true));
			}
#else
			GLCHECKER(glTexImage2D(GLMode, level, internalFormat, Width[level], Height[level], 0, internalFormat2, internalFormat3, (haveImage == false ? NULL : data)));
#endif
			if (level > 0)
				isMipMapManual = true;
		}

		// unbind
		GLCHECKER(glBindTexture(GLSubMode, 0));

		// default values
#if !defined(GLES2) && !defined(GLES3)
		if (GLMode != GL_TEXTURE_2D_MULTISAMPLE)
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
			GLCHECKER(glGenTextures(1, (GLuint*)&GL_ID));
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
		GLCHECKER(glBindTexture(GLSubMode, GL_ID));

		switch (SRepeat)
		{
#if defined(GLES2) || defined(GLES3)
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
		case TextureRepeat::Repeat:
		default:
			GLCHECKER(glTexParameteri(GLSubMode, GL_TEXTURE_WRAP_S, GL_REPEAT));
			break;
#endif
		};

		switch (TRepeat)
		{
#if defined(GLES2) || defined(GLES3)
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
		case TextureRepeat::Repeat:
		default:
			GLCHECKER(glTexParameteri(GLSubMode, GL_TEXTURE_WRAP_T, GL_REPEAT));
			break;
#endif
		};

#if !defined(GLES2) && !defined(GLES3)
		if (WrapR > -1 && GLSubMode == GL_TEXTURE_CUBE_MAP)
		{
			switch (RRepeat)
			{
			case TextureRepeat::ClampToEdge:
				GLCHECKER(glTexParameteri(GLSubMode, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE));
				break;
			case TextureRepeat::ClampToBorder:
				GLCHECKER(glTexParameteri(GLSubMode, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_BORDER));
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
#if !defined(GLES2) and !defined(GLES3)
		// USED ONLY FOR DEPTH MAPS
		// Bind
		GLCHECKER(glBindTexture(GLSubMode, GL_ID));

		GLfloat l_ClampColor[] = { 1.0, 1.0, 1.0, 1.0 };
		GLCHECKER(glTexParameterfv(GLSubMode, GL_TEXTURE_BORDER_COLOR, l_ClampColor));

		// This is to allow usage of shadow2DProj function in the shader
		GLCHECKER(glTexParameteri(GLSubMode, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE));
		GLCHECKER(glTexParameteri(GLSubMode, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL));

		// Unbind
		GLCHECKER(glBindTexture(GLSubMode, 0));
#endif
	}

	void Texture::SetTransparency(const f32 Transparency)
	{

		this->Transparency = Transparency;

	}

	void Texture::SetBorderColor(const Vec4 &Color)
	{
#if !defined(GLES2) and !defined(GLES3)
		borderColor = Color;
		GLCHECKER(glBindTexture(GLSubMode, GL_ID));
		glTexParameterfv(GLSubMode, GL_TEXTURE_BORDER_COLOR, (GLfloat*)&borderColor);
		GLCHECKER(glBindTexture(GLSubMode, 0));
#endif
	}

	void Texture::Resize(const uint32 Width, const uint32 Height, const uint32 level)
	{
		GLCHECKER(glBindTexture(GLSubMode, GL_ID));
		this->Width[level] = Width;
		this->Height[level] = Height;
		GLCHECKER(glTexImage2D(GLSubMode, level, internalFormat, Width, Height, 0, internalFormat2, internalFormat3, NULL));

		if (isMipMap)
		{
#if defined(GLES2) || defined(GLES3)
			GLCHECKER(glGenerateMipmap(GLSubMode));
#else

			GLCHECKER(glGenerateMipmap(GLSubMode));
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
		if (GL_ID > 0)
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

#if defined(GLES2) || defined(GLES3)
			GLCHECKER(glGenerateMipmap(GLSubMode));
#else
			GLCHECKER(glGenerateMipmap(GLSubMode));
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
#if !defined(GLES2) && !defined(GLES3)

		switch (internalFormat)
		{
		case GL_DEPTH_COMPONENT16:
			pixels.resize(sizeof(uchar) * 2 * Width[level] * Height[level]);
			break;
		case GL_DEPTH_COMPONENT24:
			pixels.resize(sizeof(uchar) * 3 * Width[level] * Height[level]);
			break;
		case GL_DEPTH_COMPONENT32F:
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


}
