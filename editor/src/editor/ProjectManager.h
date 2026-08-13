//============================================================================
// Name        : ProjectManager.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Editor project create / open / paths / asset listing
//============================================================================

#ifndef PROJECTMANAGER_H
#define PROJECTMANAGER_H

#include <string>
#include <vector>

struct ProjectAssetEntry {
	std::string relativePath; // under project root, e.g. assets/models/foo.p3dm
	std::string name;
	bool isDirectory;
};

struct ProjectSettings {
	// Deprecated: scene scripts are scenes/<SceneName>.lua companions.
	std::string defaultMainScript;
};

enum class LuaScriptKind {
	GameObject, // assets/lua — attachable to GameObjects
	Scene       // scenes/<Name>.lua — one per scene, not listed in Assets
};

class ProjectManager {
public:
	ProjectManager();

	bool IsOpen() const { return !projectPath.empty(); }
	const std::string& GetProjectPath() const { return projectPath; }
	const std::string& GetProjectName() const { return projectName; }
	void SetProjectName(const std::string& name, bool markDirty = true)
	{
		if (projectName != name)
		{
			projectName = name;
			if (markDirty) projectDirty = true;
		}
	}
	const std::string& GetActiveSceneRel() const { return activeSceneRel; }
	void SetActiveSceneRel(const std::string& rel, bool markDirty = true)
	{
		if (activeSceneRel != rel)
		{
			activeSceneRel = rel;
			if (markDirty) projectDirty = true;
		}
	}

	const ProjectSettings& GetSettings() const { return settings; }
	ProjectSettings& GetSettingsMutable() { return settings; }
	void SetSettings(const ProjectSettings& s, bool markDirty = true)
	{
		settings = s;
		if (markDirty) projectDirty = true;
	}

	// {parentDir}/{name}/project.json + asset folders + scenes/Default.json
	bool Create(const std::string& parentDir, const std::string& name, std::string* errorOut = NULL);
	// Open an existing project.json (or the project folder that contains one).
	bool Open(const std::string& projectJsonOrFolder, std::string* errorOut = NULL);
	bool Close();
	bool Save(std::string* errorOut = NULL);

	std::string AssetsPath() const;
	std::string ModelsPath() const;
	std::string SoundsPath() const;
	std::string TexturesPath() const;
	std::string ShadersPath() const;
	std::string LuaPath() const;
	std::string MaterialsPath() const;
	std::string ScenesPath() const;

	std::string AbsolutePath(const std::string& relative) const;
	// Empty if path is outside the project.
	std::string RelativePath(const std::string& absolute) const;

	void ListScenes(std::vector<std::string>& outSceneRelPaths) const;
	void ListAssets(const std::string& underRelative, std::vector<ProjectAssetEntry>& out, bool recursive = true) const;

	// Convert source model → assets/models/<stem>/<stem>.p3dm via AssimpImporter,
	// packaging textures and companion files into that folder.
	// Returns absolute path to .p3dm on success.
	bool ImportModel(const std::string& sourcePath, std::string& outP3dmAbsolute, std::string* errorOut = NULL);

	// Copy/convert a dropped or browsed file into the matching assets/ folder
	// by extension (models, textures, sounds, shaders, lua, materials, scenes).
	bool ImportAssetFile(const std::string& sourcePath, std::string& outAbsolute, std::string* errorOut = NULL);

	// Delete a file or empty directory under the project (relative path).
	// Deleting a .p3dm under assets/models/<stem>/ removes the whole package folder.
	bool DeleteAsset(const std::string& relativePath, std::string* errorOut = NULL);

	// GameObject → assets/lua/<name>.lua. Scene → scenes/<name>.lua.
	bool CreateLuaScript(const std::string& name, std::string& outAbsolute,
		std::string* errorOut = NULL, LuaScriptKind kind = LuaScriptKind::GameObject);

	// Absolute path of the companion script for a scene .json (…/Foo.json → …/Foo.lua).
	static std::string SceneScriptPathForSceneJson(const std::string& sceneJsonAbsolute);
	// Create the companion if missing (scene snippet). outAbsolute set on success.
	bool EnsureSceneCompanionScript(const std::string& sceneJsonAbsolute,
		std::string& outAbsolute, std::string* errorOut = NULL);

	bool IsDirty() const { return projectDirty; }
	void MarkDirty() { projectDirty = true; }
	void ClearDirty() { projectDirty = false; }

	static std::string FindAssimpImporterBinary();
	static bool IsModelSourceExtension(const std::string& path);
	static bool IsP3dm(const std::string& path);
	static bool IsTextureExtension(const std::string& path);
	static bool IsSoundExtension(const std::string& path);
	static bool IsShaderExtension(const std::string& path);
	static bool IsLuaExtension(const std::string& path);
	static bool IsMaterialExtension(const std::string& path);
	static bool IsSceneExtension(const std::string& path);
	static bool IsModelCompanionExtension(const std::string& path);
	// True for files that belong inside a model package (textures/, .thumbnails/,
	// staged sources) and should not appear as standalone Assets entries.
	static bool IsInternalAssetPath(const std::string& relativePath);
	// scenes/*.lua companions — not reusable GameObject scripts; hide from Assets.
	static bool IsSceneLuaScript(const std::string& relativePath);

private:
	std::string projectPath;
	std::string projectName;
	std::string activeSceneRel;
	ProjectSettings settings;
	bool projectDirty;

	bool EnsureDirectories(std::string* errorOut) const;
	bool WriteProjectJson(std::string* errorOut) const;
	bool WriteEmptySceneFile(const std::string& absolutePath, const std::string& sceneName, std::string* errorOut) const;
	bool LoadProjectJson(const std::string& jsonPath, std::string* errorOut);
	void RefreshSceneListInJson();
	// Copies non-texture companions (.mtl, .bin, …) next to the model.
	static void CopyModelPackageSidecars(const std::string& sourceFile, const std::string& modelDir);
	// Reads texture paths from a .p3dm, copies the referenced files into
	// modelDir/textures/, and rewrites the .p3dm strings to relative paths.
	static bool PackageReferencedModelTextures(const std::string& p3dmPath,
		const std::string& originalSourcePath, const std::string& modelDir,
		std::string* errorOut = NULL);
	static std::string SanitizeLuaClassName(const std::string& stem);
	static std::string BuildLuaSnippet(LuaScriptKind kind, const std::string& className, const std::string& fileLabel);
};

#endif /* PROJECTMANAGER_H */
