//============================================================================
// Name        : ProjectManager.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Editor project create / open / paths / asset listing
//============================================================================

#include "ProjectManager.h"
#include <Pyros3D/Utils/Json/json.hpp>
#include <Pyros3D/Core/Logs/Log.h>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <map>
#include <iterator>
#include <filesystem>
#include <chrono>

using nlohmann::json;
namespace fs = std::filesystem;

namespace {

	std::string ToLower(std::string s)
	{
		for (size_t i = 0; i < s.size(); ++i)
			s[i] = (char)std::tolower((unsigned char)s[i]);
		return s;
	}

	std::string ExtensionLower(const std::string& path)
	{
		const size_t dot = path.find_last_of('.');
		if (dot == std::string::npos) return std::string();
		return ToLower(path.substr(dot + 1));
	}

	bool RunProcess(const std::string& command, std::string* errorOut)
	{
		const int code = std::system(command.c_str());
		if (code != 0)
		{
			if (errorOut)
				*errorOut = "Command failed (" + std::to_string(code) + "): " + command;
			return false;
		}
		return true;
	}

	std::string ShellQuote(const std::string& s)
	{
#ifdef _WIN32
		return "\"" + s + "\"";
#else
		std::string out = "'";
		for (size_t i = 0; i < s.size(); ++i)
		{
			if (s[i] == '\'') out += "'\\''";
			else out += s[i];
		}
		out += "'";
		return out;
#endif
	}

}

ProjectManager::ProjectManager()
	: projectDirty(false)
{
}

bool ProjectManager::Create(const std::string& parentDir, const std::string& name, std::string* errorOut)
{
	if (parentDir.empty() || name.empty())
	{
		if (errorOut) *errorOut = "Directory and project name are required";
		return false;
	}

	std::error_code ec;
	fs::path base = fs::path(parentDir) / name;
	if (fs::exists(base, ec))
	{
		if (errorOut) *errorOut = "Folder already exists: " + base.string();
		return false;
	}

	projectPath = fs::absolute(base, ec).string();
	projectName = name;
	activeSceneRel = "scenes/Default.json";

	if (!EnsureDirectories(errorOut))
	{
		projectPath.clear();
		projectName.clear();
		activeSceneRel.clear();
		return false;
	}

	const std::string defaultScene = AbsolutePath(activeSceneRel);
	if (!WriteEmptySceneFile(defaultScene, "Default", errorOut))
	{
		projectPath.clear();
		projectName.clear();
		activeSceneRel.clear();
		return false;
	}
	{
		std::string sceneScriptAbs;
		std::string scriptErr;
		if (!EnsureSceneCompanionScript(defaultScene, sceneScriptAbs, &scriptErr))
		{
			if (errorOut) *errorOut = scriptErr;
			projectPath.clear();
			projectName.clear();
			activeSceneRel.clear();
			return false;
		}
	}

	if (!WriteProjectJson(errorOut))
	{
		projectPath.clear();
		projectName.clear();
		activeSceneRel.clear();
		return false;
	}
	projectDirty = false;
	return true;
}

bool ProjectManager::Open(const std::string& projectJsonOrFolder, std::string* errorOut)
{
	if (projectJsonOrFolder.empty())
	{
		if (errorOut) *errorOut = "No path given";
		return false;
	}

	std::error_code ec;
	fs::path p = fs::absolute(projectJsonOrFolder, ec);
	fs::path jsonPath;
	if (fs::is_directory(p, ec))
		jsonPath = p / "project.json";
	else if (p.filename() == "project.json")
		jsonPath = p;
	else
	{
		if (errorOut) *errorOut = "Select a project.json (or its folder)";
		return false;
	}

	if (!fs::exists(jsonPath, ec))
	{
		if (errorOut) *errorOut = "Missing project.json: " + jsonPath.string();
		return false;
	}

	projectPath = jsonPath.parent_path().string();
	if (!LoadProjectJson(jsonPath.string(), errorOut))
	{
		projectPath.clear();
		projectName.clear();
		activeSceneRel.clear();
		return false;
	}

	EnsureDirectories(NULL);
	projectDirty = false;
	return true;
}

bool ProjectManager::Close()
{
	projectPath.clear();
	projectName.clear();
	activeSceneRel.clear();
	settings = ProjectSettings();
	projectDirty = false;
	return true;
}

bool ProjectManager::Save(std::string* errorOut)
{
	if (!IsOpen())
	{
		if (errorOut) *errorOut = "No project open";
		return false;
	}
	RefreshSceneListInJson();
	if (!WriteProjectJson(errorOut))
		return false;
	projectDirty = false;
	return true;
}

std::string ProjectManager::AssetsPath() const { return AbsolutePath("assets"); }
std::string ProjectManager::ModelsPath() const { return AbsolutePath("assets/models"); }
std::string ProjectManager::SoundsPath() const { return AbsolutePath("assets/sounds"); }
std::string ProjectManager::TexturesPath() const { return AbsolutePath("assets/textures"); }
std::string ProjectManager::ShadersPath() const { return AbsolutePath("assets/shaders"); }
std::string ProjectManager::LuaPath() const { return AbsolutePath("assets/lua"); }
std::string ProjectManager::MaterialsPath() const { return AbsolutePath("assets/materials"); }
std::string ProjectManager::ScenesPath() const { return AbsolutePath("scenes"); }

std::string ProjectManager::AbsolutePath(const std::string& relative) const
{
	if (!IsOpen()) return relative;
	if (relative.empty()) return projectPath;
	return (fs::path(projectPath) / relative).lexically_normal().generic_string();
}

std::string ProjectManager::RelativePath(const std::string& absolute) const
{
	if (!IsOpen() || absolute.empty()) return std::string();
	std::error_code ec;
	fs::path abs = fs::absolute(absolute, ec);
	fs::path root = fs::absolute(projectPath, ec);
	fs::path rel = fs::relative(abs, root, ec);
	if (ec || rel.empty() || *rel.begin() == "..")
		return std::string();
	return rel.generic_string();
}

void ProjectManager::ListScenes(std::vector<std::string>& outSceneRelPaths) const
{
	outSceneRelPaths.clear();
	if (!IsOpen()) return;
	std::error_code ec;
	fs::path dir = ScenesPath();
	if (!fs::exists(dir, ec)) return;
	for (fs::directory_iterator it(dir, ec); !ec && it != fs::directory_iterator(); it.increment(ec))
	{
		if (!it->is_regular_file(ec)) continue;
		const std::string name = it->path().filename().string();
		const std::string lower = ToLower(name);
		if (lower.size() < 5 || lower.compare(lower.size() - 5, 5, ".json") != 0) continue;
		// Sidecars are scene.json.editor.json — never treat them as scenes.
		if (lower.find(".editor.json") != std::string::npos) continue;
		outSceneRelPaths.push_back(("scenes/" + name));
	}
	std::sort(outSceneRelPaths.begin(), outSceneRelPaths.end());
}

void ProjectManager::ListAssets(const std::string& underRelative, std::vector<ProjectAssetEntry>& out, bool recursive) const
{
	out.clear();
	if (!IsOpen()) return;
	std::error_code ec;
	fs::path root = AbsolutePath(underRelative.empty() ? "assets" : underRelative);
	if (!fs::exists(root, ec)) return;

	auto pushEntry = [&](const fs::path& p, bool isDir)
	{
		const std::string rel = RelativePath(p.string());
		if (rel.empty()) return;
		if (IsInternalAssetPath(rel)) return;
		ProjectAssetEntry e;
		e.relativePath = rel;
		e.name = p.filename().string();
		e.isDirectory = isDir;
		out.push_back(e);
	};

	if (recursive)
	{
		for (fs::recursive_directory_iterator it(root, ec); !ec && it != fs::recursive_directory_iterator(); it.increment(ec))
		{
			const bool isDir = it->is_directory(ec);
			const bool isFile = it->is_regular_file(ec);
			if (!isDir && !isFile) continue;
			pushEntry(it->path(), isDir);
		}
	}
	else
	{
		for (fs::directory_iterator it(root, ec); !ec && it != fs::directory_iterator(); it.increment(ec))
		{
			const bool isDir = it->is_directory(ec);
			const bool isFile = it->is_regular_file(ec);
			if (!isDir && !isFile) continue;
			pushEntry(it->path(), isDir);
		}
	}

	std::sort(out.begin(), out.end(), [](const ProjectAssetEntry& a, const ProjectAssetEntry& b)
	{
		if (a.isDirectory != b.isDirectory) return a.isDirectory > b.isDirectory;
		return a.relativePath < b.relativePath;
	});
}

bool ProjectManager::IsP3dm(const std::string& path)
{
	return ExtensionLower(path) == "p3dm";
}

bool ProjectManager::IsTextureExtension(const std::string& path)
{
	const std::string ext = ExtensionLower(path);
	return ext == "png" || ext == "jpg" || ext == "jpeg" || ext == "tga"
		|| ext == "bmp" || ext == "gif" || ext == "hdr" || ext == "exr" || ext == "webp";
}

bool ProjectManager::IsSoundExtension(const std::string& path)
{
	const std::string ext = ExtensionLower(path);
	return ext == "wav" || ext == "ogg" || ext == "mp3" || ext == "flac" || ext == "aiff";
}

bool ProjectManager::IsShaderExtension(const std::string& path)
{
	const std::string ext = ExtensionLower(path);
	return ext == "glsl" || ext == "vert" || ext == "frag" || ext == "comp"
		|| ext == "hlsl" || ext == "metal" || ext == "spv";
}

bool ProjectManager::IsLuaExtension(const std::string& path)
{
	return ExtensionLower(path) == "lua";
}

bool ProjectManager::IsMaterialExtension(const std::string& path)
{
	const std::string ext = ExtensionLower(path);
	return ext == "mat" || ext == "material";
}

bool ProjectManager::IsSceneExtension(const std::string& path)
{
	return ExtensionLower(path) == "json";
}

bool ProjectManager::IsModelSourceExtension(const std::string& path)
{
	const std::string ext = ExtensionLower(path);
	return ext == "obj" || ext == "fbx" || ext == "dae" || ext == "gltf" || ext == "glb"
		|| ext == "blend" || ext == "3ds" || ext == "ase" || ext == "ifc" || ext == "x"
		|| ext == "md5mesh" || ext == "smd" || ext == "stl" || ext == "ply";
}

bool ProjectManager::IsModelCompanionExtension(const std::string& path)
{
	const std::string ext = ExtensionLower(path);
	return ext == "mtl" || ext == "bin" || ext == "gltf" || ext == "glb"
		|| IsTextureExtension(path);
}

bool ProjectManager::IsInternalAssetPath(const std::string& relativePath)
{
	if (relativePath.empty()) return false;
	std::string rel = relativePath;
	for (size_t i = 0; i < rel.size(); ++i)
		if (rel[i] == '\\') rel[i] = '/';

	// Thumbnail cache next to models
	if (rel.find("/.thumbnails/") != std::string::npos
		|| rel.find(".thumbnails/") == 0
		|| (rel.size() >= 12 && rel.compare(rel.size() - 12, 12, "/.thumbnails") == 0)
		|| rel == ".thumbnails")
		return true;

	// Everything under a model package except the .p3dm itself (textures/,
	// staged .obj/.fbx, companions, etc.) stays with the model.
	if (rel.find("assets/models/") == 0 && !IsP3dm(rel))
		return true;

	// The Material Editor's codegen output. <name>.generated.glsl sits beside
	// the .mat that produced it, is rewritten wholesale on every Apply, and
	// nothing in the editor can open it - so it is an artifact of the
	// material, not an asset in its own right, and listing it only showed
	// every material twice. Deliberately scoped to assets/materials/: a
	// hand-written shader imported into assets/shaders/ is a real asset and
	// still shows.
	if (rel.find("assets/materials/") == 0 && IsShaderExtension(rel))
		return true;

	return false;
}

bool ProjectManager::IsSceneLuaScript(const std::string& relativePath)
{
	if (!IsLuaExtension(relativePath)) return false;
	std::string rel = relativePath;
	for (size_t i = 0; i < rel.size(); ++i)
		if (rel[i] == '\\') rel[i] = '/';
	return rel.find("scenes/") == 0;
}

void ProjectManager::CopyModelPackageSidecars(const std::string& sourceFile, const std::string& modelDir)
{
	std::error_code ec;
	const fs::path srcFile = fs::absolute(sourceFile, ec);
	const fs::path srcDir = srcFile.parent_path();
	const fs::path destDir = fs::path(modelDir);
	const std::string stem = srcFile.stem().string();

	fs::create_directories(destDir, ec);

	auto copyFileTo = [&](const fs::path& from, const fs::path& to) {
		if (!fs::exists(from, ec) || !fs::is_regular_file(from, ec)) return;
		fs::create_directories(to.parent_path(), ec);
		fs::copy_file(from, to, fs::copy_options::overwrite_existing, ec);
	};

	// Companions Assimp often needs beside the staged source (.mtl / .bin).
	for (fs::directory_iterator it(srcDir, ec); !ec && it != fs::directory_iterator(); it.increment(ec))
	{
		if (!it->is_regular_file(ec)) continue;
		const fs::path p = it->path();
		if (p == srcFile) continue;
		const std::string ext = ExtensionLower(p.string());
		if (ext != "mtl" && ext != "bin") continue;
		if (p.stem().string() == stem || p.filename().string().find(stem) == 0)
			copyFileTo(p, destDir / p.filename());
	}

	// Stage common texture folders so Assimp relative paths like
	// "textures/foo.png" still resolve during convert; PackageReferenced*
	// then copies only referenced files into textures/ and rewrites the p3dm.
	const char* subdirs[] = {
		"textures", "Textures", "texture", "Texture",
		"maps", "Maps", "materials", "Materials", "images", "Images"
	};
	for (size_t i = 0; i < sizeof(subdirs) / sizeof(subdirs[0]); ++i)
	{
		fs::path sub = srcDir / subdirs[i];
		if (!fs::exists(sub, ec) || !fs::is_directory(sub, ec)) continue;
		fs::copy(sub, destDir / subdirs[i],
			fs::copy_options::recursive | fs::copy_options::overwrite_existing, ec);
	}
}

namespace {

	bool ReadBytes(const std::vector<unsigned char>& data, size_t& pos, void* dst, size_t n)
	{
		if (pos + n > data.size()) return false;
		memcpy(dst, &data[pos], n);
		pos += n;
		return true;
	}

	bool ReadString(const std::vector<unsigned char>& data, size_t& pos, std::string& out)
	{
		int32_t size = 0;
		if (!ReadBytes(data, pos, &size, sizeof(int32_t))) return false;
		if (size < 0 || pos + (size_t)size > data.size()) return false;
		out.assign(reinterpret_cast<const char*>(&data[pos]), (size_t)size);
		pos += (size_t)size;
		return true;
	}

	void AppendBytes(std::vector<unsigned char>& out, const void* src, size_t n)
	{
		const unsigned char* p = reinterpret_cast<const unsigned char*>(src);
		out.insert(out.end(), p, p + n);
	}

	void AppendString(std::vector<unsigned char>& out, const std::string& s)
	{
		int32_t size = (int32_t)s.size();
		AppendBytes(out, &size, sizeof(int32_t));
		if (size > 0) AppendBytes(out, s.data(), (size_t)size);
	}

	std::string NormalizePathSeparators(std::string s)
	{
		for (size_t i = 0; i < s.size(); ++i)
			if (s[i] == '\\') s[i] = '/';
		return s;
	}

	fs::path ResolveReferencedTexture(const std::string& storedRaw,
		const std::vector<fs::path>& searchRoots)
	{
		std::error_code ec;
		std::string stored = NormalizePathSeparators(storedRaw);
		while (stored.size() >= 2 && stored[0] == '.' && stored[1] == '/')
			stored = stored.substr(2);
		if (stored.empty()) return fs::path();
		if (stored[0] == '*') return fs::path();

		fs::path asPath(stored);
		if (asPath.is_absolute() && fs::exists(asPath, ec) && fs::is_regular_file(asPath, ec))
			return fs::weakly_canonical(asPath, ec);
		if (!stored.empty() && stored[0] == '/' && fs::exists(asPath, ec) && fs::is_regular_file(asPath, ec))
			return fs::weakly_canonical(asPath, ec);

		for (size_t i = 0; i < searchRoots.size(); ++i)
		{
			const fs::path& root = searchRoots[i];
			if (root.empty()) continue;

			fs::path cand = (root / asPath).lexically_normal();
			if (fs::exists(cand, ec) && fs::is_regular_file(cand, ec))
				return fs::weakly_canonical(cand, ec);

			cand = root / asPath.filename();
			if (fs::exists(cand, ec) && fs::is_regular_file(cand, ec))
				return fs::weakly_canonical(cand, ec);

			// Search one level of common subfolders under each root.
			const char* subdirs[] = { "textures", "Textures", "maps", "Maps", "images", "Images" };
			for (size_t s = 0; s < sizeof(subdirs) / sizeof(subdirs[0]); ++s)
			{
				cand = root / subdirs[s] / asPath.filename();
				if (fs::exists(cand, ec) && fs::is_regular_file(cand, ec))
					return fs::weakly_canonical(cand, ec);
				cand = (root / subdirs[s] / asPath).lexically_normal();
				if (fs::exists(cand, ec) && fs::is_regular_file(cand, ec))
					return fs::weakly_canonical(cand, ec);
			}
		}
		return fs::path();
	}

	std::string UniqueTextureDestName(const fs::path& texturesDir, const fs::path& sourceFile)
	{
		std::error_code ec;
		std::string base = sourceFile.filename().string();
		if (base.empty()) base = "texture.png";

		if (!fs::exists(texturesDir / base, ec))
			return base;

		const std::string stem = sourceFile.stem().string();
		const std::string ext = sourceFile.extension().string();
		for (int n = 1; n < 10000; ++n)
		{
			char buf[32];
			snprintf(buf, sizeof(buf), "_%d", n);
			std::string candidate = stem + buf + ext;
			if (!fs::exists(texturesDir / candidate, ec))
				return candidate;
		}
		return stem + "_copy" + ext;
	}

}

bool ProjectManager::PackageReferencedModelTextures(const std::string& p3dmPath,
	const std::string& originalSourcePath, const std::string& modelDir,
	std::string* errorOut)
{
	std::error_code ec;
	if (!fs::exists(p3dmPath, ec))
	{
		if (errorOut) *errorOut = "p3dm not found for texture packaging";
		return false;
	}

	std::ifstream in(p3dmPath, std::ios::binary);
	if (!in)
	{
		if (errorOut) *errorOut = "Failed to open p3dm for texture packaging";
		return false;
	}
	std::vector<unsigned char> data((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
	in.close();
	if (data.size() < sizeof(int32_t))
	{
		if (errorOut) *errorOut = "p3dm too small";
		return false;
	}

	std::vector<fs::path> roots;
	roots.push_back(fs::path(modelDir));
	if (!originalSourcePath.empty())
		roots.push_back(fs::path(originalSourcePath).parent_path());
	roots.push_back(fs::path(p3dmPath).parent_path());

	const fs::path texturesDir = fs::path(modelDir) / "textures";
	fs::create_directories(texturesDir, ec);

	// sourceAbs -> relative path stored in p3dm (textures/foo.png)
	std::map<std::string, std::string> remapped;

	auto remapOne = [&](const std::string& stored) -> std::string {
		if (stored.empty()) return stored;
		if (!stored.empty() && stored[0] == '*')
			return stored; // embedded — cannot copy as a file path

		const fs::path resolved = ResolveReferencedTexture(stored, roots);
		if (resolved.empty())
		{
			echo("WARNING: model texture not found: " + stored);
			return stored;
		}

		const std::string absKey = fs::weakly_canonical(resolved, ec).string();
		std::map<std::string, std::string>::iterator it = remapped.find(absKey);
		if (it != remapped.end())
			return it->second;

		const std::string destName = UniqueTextureDestName(texturesDir, resolved);
		const fs::path dest = texturesDir / destName;
		fs::copy_file(resolved, dest, fs::copy_options::overwrite_existing, ec);
		if (ec)
		{
			echo("WARNING: failed to copy model texture: " + resolved.string());
			return stored;
		}

		const std::string rel = std::string("textures/") + destName;
		remapped[absKey] = rel;
		return rel;
	};

	size_t pos = 0;
	int32_t materialsSize = 0;
	if (!ReadBytes(data, pos, &materialsSize, sizeof(int32_t)))
	{
		if (errorOut) *errorOut = "Failed to read materials header";
		return false;
	}
	if (materialsSize < 0 || materialsSize > 100000)
	{
		if (errorOut) *errorOut = "Invalid materials count in p3dm";
		return false;
	}

	std::vector<unsigned char> out;
	out.reserve(data.size());
	AppendBytes(out, &materialsSize, sizeof(int32_t));

	bool anyRewrite = false;
	for (int32_t mi = 0; mi < materialsSize; ++mi)
	{
		int32_t id = 0;
		if (!ReadBytes(data, pos, &id, sizeof(int32_t)))
		{
			if (errorOut) *errorOut = "Failed to parse p3dm materials";
			return false;
		}
		AppendBytes(out, &id, sizeof(int32_t));

		std::string name;
		if (!ReadString(data, pos, name))
		{
			if (errorOut) *errorOut = "Failed to parse p3dm materials";
			return false;
		}
		AppendString(out, name);

		for (int k = 0; k < 4; ++k)
		{
			unsigned char flag = 0;
			if (!ReadBytes(data, pos, &flag, 1)) return false;
			AppendBytes(out, &flag, 1);
			unsigned char vec[sizeof(float) * 4];
			if (!ReadBytes(data, pos, vec, sizeof(vec))) return false;
			AppendBytes(out, vec, sizeof(vec));
		}

		unsigned char wire = 0, two = 0;
		if (!ReadBytes(data, pos, &wire, 1) || !ReadBytes(data, pos, &two, 1)) return false;
		AppendBytes(out, &wire, 1);
		AppendBytes(out, &two, 1);

		float floats[3];
		if (!ReadBytes(data, pos, floats, sizeof(floats))) return false;
		AppendBytes(out, floats, sizeof(floats));

		for (int t = 0; t < 3; ++t)
		{
			unsigned char have = 0;
			if (!ReadBytes(data, pos, &have, 1)) return false;
			AppendBytes(out, &have, 1);

			std::string texPath;
			if (!ReadString(data, pos, texPath)) return false;
			std::string rewritten = texPath;
			if (have && !texPath.empty())
			{
				rewritten = remapOne(texPath);
				if (rewritten != texPath) anyRewrite = true;
			}
			AppendString(out, rewritten);
		}

		unsigned char haveBones = 0;
		if (!ReadBytes(data, pos, &haveBones, 1)) return false;
		AppendBytes(out, &haveBones, 1);
	}

	out.insert(out.end(), data.begin() + (std::ptrdiff_t)pos, data.end());

	// Always rewrite when we remapped anything, or when paths changed.
	if (!anyRewrite && remapped.empty())
		return true;

	std::ofstream outFile(p3dmPath, std::ios::binary | std::ios::trunc);
	if (!outFile)
	{
		if (errorOut) *errorOut = "Failed to rewrite p3dm texture paths";
		return false;
	}
	outFile.write(reinterpret_cast<const char*>(out.data()), (std::streamsize)out.size());
	if (!remapped.empty())
	{
		char buf[64];
		snprintf(buf, sizeof(buf), "Packaged %d model texture(s)", (int)remapped.size());
		echo(buf);
	}
	return true;
}

bool ProjectManager::ImportAssetFile(const std::string& sourcePath, std::string& outAbsolute, std::string* errorOut,
	std::string* outTrashedExisting)
{
	outAbsolute.clear();
	if (outTrashedExisting) outTrashedExisting->clear();
	if (!IsOpen())
	{
		if (errorOut) *errorOut = "Open a project first";
		return false;
	}
	if (sourcePath.empty() || !fs::exists(sourcePath))
	{
		if (errorOut) *errorOut = "File not found";
		return false;
	}

	std::error_code ec;
	if (fs::is_directory(sourcePath, ec))
	{
		if (errorOut) *errorOut = "Drop individual files (folders not supported yet)";
		return false;
	}

	// Already inside this project — nothing to do.
	{
		const std::string rel = RelativePath(sourcePath);
		if (!rel.empty())
		{
			outAbsolute = sourcePath;
			return true;
		}
	}

	if (IsP3dm(sourcePath) || IsModelSourceExtension(sourcePath))
		return ImportModel(sourcePath, outAbsolute, errorOut, outTrashedExisting);

	std::string destDir;
	if (IsTextureExtension(sourcePath)) destDir = TexturesPath();
	else if (IsSoundExtension(sourcePath)) destDir = SoundsPath();
	else if (IsShaderExtension(sourcePath)) destDir = ShadersPath();
	else if (IsLuaExtension(sourcePath)) destDir = LuaPath();
	else if (IsMaterialExtension(sourcePath)) destDir = MaterialsPath();
	else if (IsSceneExtension(sourcePath)) destDir = ScenesPath();
	else
	{
		// Unknown type → drop into assets/ root.
		destDir = AssetsPath();
	}

	fs::create_directories(destDir, ec);
	fs::path dest = fs::path(destDir) / fs::path(sourcePath).filename();

	// Trash whatever's already at `dest` before the copy overwrites it, so
	// undo can put it back exactly (same rationale as ImportModel's package
	// case above).
	if (fs::exists(dest, ec))
	{
		const std::string trashRel = MoveToTrash(dest.string(), errorOut);
		if (trashRel.empty())
			return false;
		if (outTrashedExisting) *outTrashedExisting = trashRel;
	}

	fs::copy_file(sourcePath, dest, fs::copy_options::overwrite_existing, ec);
	if (ec)
	{
		if (errorOut) *errorOut = "Failed to copy into project: " + ec.message();
		return false;
	}
	outAbsolute = dest.string();
	projectDirty = true;
	return true;
}

// Trash lives at <project>/.trash/, a sibling of assets/ - ListAssets()
// only ever walks under "assets" (see its default underRelative), so
// trashed files never leak into the Assets panel or any Agent listing
// without needing their own exclusion filter. Nothing here ever deletes a
// .trash/ entry once it's there: undo commands hold a reference to it and
// may need it back at any point in the session, and a stale entry from a
// finished undo history is harmless clutter, not a bug - a future "Empty
// Trash" action or age-based sweep is a deliberate v2, not this one.
std::string ProjectManager::MoveToTrash(const std::string& absolutePath, std::string* errorOut)
{
	if (!IsOpen())
	{
		if (errorOut) *errorOut = "No project open";
		return std::string();
	}
	std::error_code ec;
	if (!fs::exists(absolutePath, ec))
	{
		if (errorOut) *errorOut = "Nothing to trash: " + absolutePath;
		return std::string();
	}

	const std::string trashDir = AbsolutePath(".trash");
	fs::create_directories(trashDir, ec);
	if (ec)
	{
		if (errorOut) *errorOut = "Failed to create .trash: " + ec.message();
		return std::string();
	}

	const std::string rel = RelativePath(absolutePath);
	std::string safeName = rel.empty() ? fs::path(absolutePath).filename().string() : rel;
	for (char& c : safeName) if (c == '/' || c == '\\') c = '_';

	const long long stamp = std::chrono::duration_cast<std::chrono::milliseconds>(
		std::chrono::system_clock::now().time_since_epoch()).count();
	std::string trashRel = ".trash/" + std::to_string(stamp) + "_" + safeName;
	std::string trashAbs = AbsolutePath(trashRel);
	// Same-millisecond collision is astronomically unlikely but cheap to guard.
	for (int suffix = 1; fs::exists(trashAbs, ec); ++suffix)
	{
		trashRel = ".trash/" + std::to_string(stamp) + "_" + std::to_string(suffix) + "_" + safeName;
		trashAbs = AbsolutePath(trashRel);
	}

	fs::rename(absolutePath, trashAbs, ec);
	if (ec)
	{
		if (errorOut) *errorOut = "Failed to move to trash: " + ec.message();
		return std::string();
	}
	projectDirty = true;
	return trashRel;
}

bool ProjectManager::MoveFromTrash(const std::string& trashRelativePath, const std::string& destinationAbsolute, std::string* errorOut)
{
	if (!IsOpen())
	{
		if (errorOut) *errorOut = "No project open";
		return false;
	}
	const std::string trashAbs = AbsolutePath(trashRelativePath);
	std::error_code ec;
	if (!fs::exists(trashAbs, ec))
	{
		if (errorOut) *errorOut = "Trash entry missing: " + trashRelativePath;
		return false;
	}
	if (fs::exists(destinationAbsolute, ec))
	{
		if (errorOut) *errorOut = "Destination already occupied: " + destinationAbsolute;
		return false;
	}
	fs::create_directories(fs::path(destinationAbsolute).parent_path(), ec);
	fs::rename(trashAbs, destinationAbsolute, ec);
	if (ec)
	{
		if (errorOut) *errorOut = "Failed to restore from trash: " + ec.message();
		return false;
	}
	projectDirty = true;
	return true;
}

bool ProjectManager::DeleteAsset(const std::string& relativePath, std::string* errorOut, std::string* outTrashRelativePath,
	std::string* outMovedFromRelativePath)
{
	if (outTrashRelativePath) outTrashRelativePath->clear();
	if (outMovedFromRelativePath) outMovedFromRelativePath->clear();
	if (!IsOpen())
	{
		if (errorOut) *errorOut = "No project open";
		return false;
	}
	if (relativePath.empty() || relativePath == "." || relativePath.find("..") != std::string::npos)
	{
		if (errorOut) *errorOut = "Invalid asset path";
		return false;
	}
	if (relativePath == "project.json")
	{
		if (errorOut) *errorOut = "Cannot delete project.json";
		return false;
	}

	const std::string abs = AbsolutePath(relativePath);
	std::error_code ec;
	if (!fs::exists(abs, ec))
	{
		if (errorOut) *errorOut = "File not found";
		return false;
	}

	// Model packages live at assets/models/<stem>/ — deleting the .p3dm
	// trashes the whole package (textures, staged source, etc.) as one unit.
	if (IsP3dm(relativePath))
	{
		fs::path rel(relativePath);
		fs::path parent = rel.parent_path();
		if (!parent.empty() && parent.filename() == rel.stem()
			&& parent.string().find("assets/models/") == 0)
		{
			const std::string trashRel = MoveToTrash(AbsolutePath(parent.string()), errorOut);
			if (trashRel.empty())
				return false;
			if (outTrashRelativePath) *outTrashRelativePath = trashRel;
			if (outMovedFromRelativePath) *outMovedFromRelativePath = parent.string();
			projectDirty = true;
			return true;
		}
	}

	if (fs::is_directory(abs, ec) && !fs::is_empty(abs, ec))
	{
		if (errorOut) *errorOut = "Folder is not empty";
		return false;
	}

	const std::string trashRel = MoveToTrash(abs, errorOut);
	if (trashRel.empty())
		return false;
	if (outTrashRelativePath) *outTrashRelativePath = trashRel;
	if (outMovedFromRelativePath) *outMovedFromRelativePath = relativePath;
	projectDirty = true;
	return true;
}

bool ProjectManager::CreateLuaScript(const std::string& name, std::string& outAbsolute,
	std::string* errorOut, LuaScriptKind kind)
{
	outAbsolute.clear();
	if (!IsOpen())
	{
		if (errorOut) *errorOut = "No project open";
		return false;
	}

	std::string stem = name;
	while (!stem.empty() && (stem.front() == ' ' || stem.front() == '\t')) stem.erase(stem.begin());
	while (!stem.empty() && (stem.back() == ' ' || stem.back() == '\t')) stem.pop_back();
	if (stem.empty())
	{
		if (errorOut) *errorOut = "Script name is empty";
		return false;
	}
	if (stem.find('/') != std::string::npos || stem.find('\\') != std::string::npos)
	{
		if (errorOut) *errorOut = "Name cannot contain path separators";
		return false;
	}
	if (stem.size() < 4 || stem.compare(stem.size() - 4, 4, ".lua") != 0)
		stem += ".lua";

	const std::string classStem = fs::path(stem).stem().string();
	const std::string className = SanitizeLuaClassName(classStem);
	const std::string rel = (kind == LuaScriptKind::Scene)
		? ("scenes/" + stem)
		: ("assets/lua/" + stem);
	const std::string abs = AbsolutePath(rel);

	std::error_code ec;
	fs::create_directories(fs::path(abs).parent_path(), ec);
	if (fs::exists(abs, ec))
	{
		if (errorOut) *errorOut = "Script already exists: " + rel;
		return false;
	}

	std::ofstream out(abs);
	if (!out)
	{
		if (errorOut) *errorOut = "Could not create " + rel;
		return false;
	}
	out << BuildLuaSnippet(kind, className, stem);
	out.close();

	outAbsolute = abs;
	projectDirty = true;
	return true;
}

bool ProjectManager::CreateMaterial(const std::string& name, MaterialAssetKind kind, std::string& outAbsolute,
	std::string* errorOut, bool useTextMode)
{
	outAbsolute.clear();
	if (!IsOpen())
	{
		if (errorOut) *errorOut = "No project open";
		return false;
	}

	std::string stem = name;
	while (!stem.empty() && (stem.front() == ' ' || stem.front() == '\t')) stem.erase(stem.begin());
	while (!stem.empty() && (stem.back() == ' ' || stem.back() == '\t')) stem.pop_back();
	if (stem.empty())
	{
		if (errorOut) *errorOut = "Material name is empty";
		return false;
	}
	if (stem.find('/') != std::string::npos || stem.find('\\') != std::string::npos)
	{
		if (errorOut) *errorOut = "Name cannot contain path separators";
		return false;
	}
	if (stem.size() < 4 || stem.compare(stem.size() - 4, 4, ".mat") != 0)
		stem += ".mat";

	const std::string rel = "assets/materials/" + stem;
	const std::string abs = AbsolutePath(rel);

	std::error_code ec;
	fs::create_directories(fs::path(abs).parent_path(), ec);
	if (fs::exists(abs, ec))
	{
		if (errorOut) *errorOut = "Material already exists: " + rel;
		return false;
	}

	const std::string materialName = fs::path(stem).stem().string();
	json j;
	j["name"] = materialName;
	if (kind == MaterialAssetKind::Generic)
	{
		j["kind"] = "generic";
	}
	else if (useTextMode)
	{
		// Text and Node Graph are two INCOMPATIBLE representations of a
		// Custom material - neither converts to the other (see
		// MaterialCodegen.h) - so a text-mode material is seeded with only
		// the text snippet, not the starter node graph below (an empty,
		// never-shown Node Graph tab would be misleading, not a real
		// alternate copy of this material). Kept as a plain string literal,
		// matching kDefaultSimpleShaderText in MaterialCodegen.cpp, since
		// ProjectManager shouldn't depend on the node-graph editor's types.
		j["kind"] = "custom";
		j["editMode"] = "text";
		j["customShaderText"] =
			"vec3 Albedo = vec3(1.0, 1.0, 1.0);\n"
			"float Metallic = 0.0;\n"
			"float Roughness = 0.5;\n"
			"vec3 Emissive = vec3(0.0, 0.0, 0.0);\n"
			"float Occlusion = 1.0;\n"
			"\n// Leave Normal at (0,0,0) to use the surface's own normal.\n"
			"vec3 Normal = vec3(0.0, 0.0, 0.0);\n";
	}
	else
	{
		// Small starter graph: Base Color -> Albedo, Metallic -> Metallic,
		// Roughness -> Roughness, matching MaterialEditor's own
		// SeedDefaultCustomGraph() (kept as a plain JSON literal here since
		// ProjectManager shouldn't depend on the node-graph editor's types).
		j["kind"] = "custom";
		j["editMode"] = "nodegraph";
		j["nodes"] = json::array({
			{ {"id", 1}, {"type", "Color"}, {"name", "Base Color"}, {"pos", {80.0, 140.0}}, {"userData", "1.000000,1.000000,1.000000,1.000000"}, {"texturePath", ""} },
			{ {"id", 2}, {"type", "Float"}, {"name", "Metallic"}, {"pos", {80.0, 320.0}}, {"userData", "0.000000"}, {"texturePath", ""} },
			{ {"id", 3}, {"type", "Float"}, {"name", "Roughness"}, {"pos", {80.0, 460.0}}, {"userData", "0.500000"}, {"texturePath", ""} },
			{ {"id", 4}, {"type", "Output"}, {"name", "Output"}, {"pos", {560.0, 280.0}}, {"userData", ""}, {"texturePath", ""} }
		});
		j["connections"] = json::array({
			{ {"fromNode", 1}, {"fromPinIndex", 4}, {"toNode", 4}, {"toPinIndex", 0} },
			{ {"fromNode", 2}, {"fromPinIndex", 0}, {"toNode", 4}, {"toPinIndex", 2} },
			{ {"fromNode", 3}, {"fromPinIndex", 0}, {"toNode", 4}, {"toPinIndex", 3} }
		});
	}

	std::ofstream out(abs);
	if (!out)
	{
		if (errorOut) *errorOut = "Could not create " + rel;
		return false;
	}
	out << j.dump(2);
	out.close();

	outAbsolute = abs;
	projectDirty = true;
	return true;
}

std::string ProjectManager::SceneScriptPathForSceneJson(const std::string& sceneJsonAbsolute)
{
	if (sceneJsonAbsolute.empty()) return std::string();
	fs::path p(sceneJsonAbsolute);
	p.replace_extension(".lua");
	return p.string();
}

bool ProjectManager::EnsureSceneCompanionScript(const std::string& sceneJsonAbsolute,
	std::string& outAbsolute, std::string* errorOut)
{
	outAbsolute.clear();
	if (sceneJsonAbsolute.empty())
	{
		if (errorOut) *errorOut = "Scene path is empty";
		return false;
	}
	outAbsolute = SceneScriptPathForSceneJson(sceneJsonAbsolute);
	std::error_code ec;
	if (fs::exists(outAbsolute, ec))
		return true;

	const std::string stem = fs::path(outAbsolute).stem().string();
	if (!IsOpen())
	{
		// Allow writing beside the scene even without a project manager open
		// (absolute path already known).
		const std::string className = SanitizeLuaClassName(stem);
		fs::create_directories(fs::path(outAbsolute).parent_path(), ec);
		std::ofstream out(outAbsolute);
		if (!out)
		{
			if (errorOut) *errorOut = "Could not create scene script: " + outAbsolute;
			outAbsolute.clear();
			return false;
		}
		out << BuildLuaSnippet(LuaScriptKind::Scene, className, stem + ".lua");
		out.close();
		return true;
	}

	std::string created;
	std::string err;
	if (!CreateLuaScript(stem, created, &err, LuaScriptKind::Scene))
	{
		// CreateLuaScript fails if file exists — race; treat as ok if present.
		if (fs::exists(outAbsolute, ec))
			return true;
		if (errorOut) *errorOut = err.empty() ? "Could not create scene script" : err;
		outAbsolute.clear();
		return false;
	}
	outAbsolute = created;
	return true;
}

std::string ProjectManager::SanitizeLuaClassName(const std::string& stem)
{
	std::string out;
	out.reserve(stem.size());
	for (size_t i = 0; i < stem.size(); ++i)
	{
		const char c = stem[i];
		if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_')
			out.push_back(c);
		else if (c == '-' || c == ' ')
			out.push_back('_');
	}
	if (out.empty() || (out[0] >= '0' && out[0] <= '9'))
		out = "Script_" + out;
	return out;
}

std::string ProjectManager::BuildLuaSnippet(LuaScriptKind kind, const std::string& className, const std::string& fileLabel)
{
	std::ostringstream ss;
	if (kind == LuaScriptKind::Scene)
	{
		ss << "-- " << fileLabel << "\n"
		   << "-- Scene main script (no GameObject owner).\n"
		   << "-- Created automatically next to the scene .json — not listed in Assets.\n"
		   << "\n"
		   << "local " << className << " = class('" << className << "')\n"
		   << "\n"
		   << "function " << className << ":initialize()\n"
		   << "end\n"
		   << "\n"
		   << "function " << className << ":init(owner)\n"
		   << "\t-- owner is always nil for scene scripts\n"
		   << "end\n"
		   << "\n"
		   << "function " << className << ":update(time)\n"
		   << "end\n"
		   << "\n"
		   << "function " << className << ":destroy()\n"
		   << "end\n"
		   << "\n"
		   << "return " << className << "\n";
	}
	else
	{
		ss << "-- " << fileLabel << "\n"
		   << "-- GameObject behavior. Attach via Properties → Script.\n"
		   << "-- Must return a middleclass class with :new().\n"
		   << "\n"
		   << "local " << className << " = class('" << className << "')\n"
		   << "\n"
		   << "function " << className << ":initialize()\n"
		   << "end\n"
		   << "\n"
		   << "function " << className << ":init(owner)\n"
		   << "\tself.owner = owner\n"
		   << "end\n"
		   << "\n"
		   << "function " << className << ":update(time)\n"
		   << "end\n"
		   << "\n"
		   << "function " << className << ":destroy()\n"
		   << "end\n"
		   << "\n"
		   << "function " << className << ":serialize()\n"
		   << "\treturn {}\n"
		   << "end\n"
		   << "\n"
		   << "function " << className << ".deserialize(data)\n"
		   << "\treturn " << className << ":new()\n"
		   << "end\n"
		   << "\n"
		   << "return " << className << "\n";
	}
	return ss.str();
}

std::string ProjectManager::FindAssimpImporterBinary()
{
	std::error_code ec;
	std::vector<fs::path> candidates;

#ifdef _WIN32
	candidates.push_back(fs::current_path() / "AssimpImporter.exe");
	candidates.push_back(fs::current_path() / "tools" / "AssimpImporter.exe");
#else
	candidates.push_back(fs::current_path() / "AssimpImporter");
	candidates.push_back(fs::current_path() / "tools" / "AssimpImporter");
#endif

	// Common build-tree locations relative to the editor binary cwd.
	candidates.push_back(fs::current_path() / ".." / "tools" / "AssimpImporter" / "AssimpImporter");
	candidates.push_back(fs::current_path() / ".." / ".." / "tools" / "AssimpImporter" / "AssimpImporter");
	candidates.push_back(fs::current_path().parent_path() / "tools" / "AssimpImporter" / "AssimpImporter");

	for (size_t i = 0; i < candidates.size(); ++i)
	{
		fs::path p = fs::weakly_canonical(candidates[i], ec);
		if (!ec && fs::exists(p, ec) && fs::is_regular_file(p, ec))
			return p.string();
	}
	return std::string();
}

bool ProjectManager::ImportModel(const std::string& sourcePath, std::string& outP3dmAbsolute, std::string* errorOut,
	std::string* outTrashedPackageDir)
{
	outP3dmAbsolute.clear();
	if (outTrashedPackageDir) outTrashedPackageDir->clear();
	if (!IsOpen())
	{
		if (errorOut) *errorOut = "Open a project before importing models";
		return false;
	}
	if (sourcePath.empty() || !fs::exists(sourcePath))
	{
		if (errorOut) *errorOut = "Source model not found";
		return false;
	}

	std::error_code ec;
	const std::string stem = fs::path(sourcePath).stem().string();
	if (stem.empty())
	{
		if (errorOut) *errorOut = "Invalid model filename";
		return false;
	}

	const fs::path modelDir = fs::path(ModelsPath()) / stem;

	// A .p3dm re-"imported" from right where it already lives is a true
	// no-op - must be checked BEFORE the trash step below, or re-importing
	// an unchanged in-place model would trash the very file it then tries
	// to report success against.
	if (IsP3dm(sourcePath))
	{
		const std::string alreadyRel = RelativePath(sourcePath);
		if (!alreadyRel.empty() && alreadyRel.find("assets/models/") == 0)
		{
			outP3dmAbsolute = sourcePath;
			return true;
		}
	}

	// Re-importing over an existing package (same model name imported
	// again, from a genuinely different source) would otherwise silently
	// overwrite its contents file-by-file below - trash the whole
	// pre-existing folder as one unit first so undo can restore it
	// exactly, rather than trying to reconstruct which individual files
	// got clobbered.
	if (fs::exists(modelDir, ec) && !fs::is_empty(modelDir, ec))
	{
		const std::string trashRel = MoveToTrash(modelDir.string(), errorOut);
		if (trashRel.empty())
			return false;
		if (outTrashedPackageDir) *outTrashedPackageDir = trashRel;
	}

	fs::create_directories(modelDir, ec);
	if (ec)
	{
		if (errorOut) *errorOut = "Failed to create model folder: " + ec.message();
		return false;
	}

	if (IsP3dm(sourcePath))
	{
		const fs::path dest = modelDir / (stem + ".p3dm");
		fs::copy_file(sourcePath, dest, fs::copy_options::overwrite_existing, ec);
		if (ec)
		{
			if (errorOut) *errorOut = "Failed to copy .p3dm into project: " + ec.message();
			return false;
		}
		CopyModelPackageSidecars(sourcePath, modelDir.string());
		{
			std::string texErr;
			if (!PackageReferencedModelTextures(dest.string(), sourcePath, modelDir.string(), &texErr) && errorOut && !texErr.empty())
				*errorOut = texErr; // non-fatal if empty; still keep the model
		}
		outP3dmAbsolute = dest.string();
		projectDirty = true;
		return true;
	}

	if (!IsModelSourceExtension(sourcePath))
	{
		if (errorOut) *errorOut = "Unsupported model format";
		return false;
	}

	const std::string importer = FindAssimpImporterBinary();
	if (importer.empty())
	{
		if (errorOut) *errorOut = "AssimpImporter not found (build with -DBUILD_CONVERTER=ON)";
		return false;
	}

	// Stage source + companions into the package folder, convert, then copy
	// only the textures the .p3dm actually references and rewrite paths.
	const fs::path stagedSrc = modelDir / fs::path(sourcePath).filename();
	fs::copy_file(sourcePath, stagedSrc, fs::copy_options::overwrite_existing, ec);
	if (ec)
	{
		if (errorOut) *errorOut = "Failed to stage model into package: " + ec.message();
		return false;
	}
	CopyModelPackageSidecars(sourcePath, modelDir.string());

	const fs::path outBase = modelDir / stem; // converter appends .p3dm
	const fs::path outP3dm = fs::path(outBase.string() + ".p3dm");

	std::ostringstream cmd;
	cmd << ShellQuote(importer) << " --model " << ShellQuote(stagedSrc.string()) << " " << ShellQuote(outBase.string());
	if (!RunProcess(cmd.str(), errorOut))
		return false;

	if (!fs::exists(outP3dm, ec))
	{
		if (errorOut) *errorOut = "Converter finished but output missing: " + outP3dm.string();
		return false;
	}

	std::string texErr;
	if (!PackageReferencedModelTextures(outP3dm.string(), sourcePath, modelDir.string(), &texErr))
	{
		// Model is usable even if texture packaging fails; report when we can.
		if (errorOut && !texErr.empty())
			*errorOut = texErr;
	}

	outP3dmAbsolute = outP3dm.string();
	projectDirty = true;
	return true;
}

bool ProjectManager::EnsureDirectories(std::string* errorOut) const
{
	if (!IsOpen()) return false;
	std::error_code ec;
	const char* dirs[] = {
		"assets",
		"assets/models",
		"assets/sounds",
		"assets/textures",
		"assets/shaders",
		"assets/lua",
		"assets/materials",
		"scenes"
	};
	for (size_t i = 0; i < sizeof(dirs) / sizeof(dirs[0]); ++i)
	{
		fs::create_directories(AbsolutePath(dirs[i]), ec);
		if (ec)
		{
			if (errorOut) *errorOut = "Failed to create " + std::string(dirs[i]) + ": " + ec.message();
			return false;
		}
	}
	return true;
}

bool ProjectManager::WriteEmptySceneFile(const std::string& absolutePath, const std::string& sceneName, std::string* errorOut) const
{
	std::error_code ec;
	fs::create_directories(fs::path(absolutePath).parent_path(), ec);
	json root;
	root["version"] = 1;
	root["materials"] = json::array();
	root["roots"] = json::array();
	const std::string scriptName = sceneName.empty()
		? fs::path(absolutePath).stem().string()
		: sceneName;
	root["mainScript"] = "scenes/" + scriptName + ".lua";
	std::ofstream out(absolutePath.c_str());
	if (!out.is_open())
	{
		if (errorOut) *errorOut = "Could not write scene: " + absolutePath;
		return false;
	}
	out << root.dump(4);
	out.close();
	return true;
}

bool ProjectManager::WriteProjectJson(std::string* errorOut) const
{
	if (!IsOpen()) return false;
	json root;
	root["name"] = projectName;
	root["version"] = 1;
	root["activeScene"] = activeSceneRel;

	json settingsJ;
	settingsJ["defaultMainScript"] = settings.defaultMainScript;
	if (settings.rendererType == ProjectRendererType::Deferred)
		settingsJ["rendererType"] = "deferred";
	else
		settingsJ["rendererType"] = "forward";
	root["settings"] = settingsJ;

	std::vector<std::string> scenes;
	ListScenes(scenes);
	json scenesArr = json::array();
	for (size_t i = 0; i < scenes.size(); ++i)
		scenesArr.push_back(scenes[i]);
	if (scenesArr.empty() && !activeSceneRel.empty())
		scenesArr.push_back(activeSceneRel);
	root["scenes"] = scenesArr;

	const std::string path = AbsolutePath("project.json");
	std::ofstream out(path.c_str());
	if (!out.is_open())
	{
		if (errorOut) *errorOut = "Could not write project.json";
		return false;
	}
	out << root.dump(4);
	out.close();
	return true;
}

bool ProjectManager::LoadProjectJson(const std::string& jsonPath, std::string* errorOut)
{
	std::ifstream in(jsonPath.c_str());
	if (!in.is_open())
	{
		if (errorOut) *errorOut = "Could not read project.json";
		return false;
	}
	json root;
	try { in >> root; }
	catch (const std::exception& e)
	{
		if (errorOut) *errorOut = std::string("Invalid project.json: ") + e.what();
		return false;
	}

	projectName = root.value("name", fs::path(projectPath).filename().string());
	activeSceneRel = root.value("activeScene", std::string("scenes/Default.json"));
	if (activeSceneRel.empty())
		activeSceneRel = "scenes/Default.json";

	settings = ProjectSettings();
	if (root.contains("settings") && root["settings"].is_object())
	{
		const json& s = root["settings"];
		settings.defaultMainScript = s.value("defaultMainScript", std::string());
		std::string rt = s.value("rendererType", "forward");
		if (rt == "deferred") settings.rendererType = ProjectRendererType::Deferred;
		else settings.rendererType = ProjectRendererType::Forward;
	}

	std::error_code ec;
	if (!fs::exists(AbsolutePath(activeSceneRel), ec))
	{
		std::vector<std::string> scenes;
		ListScenes(scenes);
		if (!scenes.empty())
			activeSceneRel = scenes[0];
	}
	return true;
}

void ProjectManager::RefreshSceneListInJson()
{
	// WriteProjectJson already re-lists scenes from disk.
}
