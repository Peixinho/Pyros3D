//============================================================================
// Name        : LuaCompletion.h
// Description : Keyword / member completion for the Lua code editor
//============================================================================

#ifndef LUACOMPLETION_H
#define LUACOMPLETION_H

#include <string>
#include <vector>
#include <cstring>
#include <cctype>
#include <algorithm>
#include <cstdint>

namespace LuaCompletion {

enum class Kind : uint8_t
{
	Keyword = 0, // language keyword
	Function,    // free function / builtin
	Method,      // instance method
	Field,       // variable / property
	Type,        // class / usertype
	Enum,        // enum / constant
	Module       // table / namespace
};

struct Item
{
	std::string text;
	Kind kind = Kind::Field;
};

inline const char* KindIcon(Kind k)
{
	// Font Awesome solid (merged into default font in Editor init)
	switch (k)
	{
	case Kind::Keyword:  return u8"\uf02b"; // tag
	case Kind::Function: return u8"\uf0e7"; // bolt
	case Kind::Method:   return u8"\uf121"; // code
	case Kind::Field:    return u8"\uf111"; // circle
	case Kind::Type:     return u8"\uf1b2"; // cube
	case Kind::Enum:     return u8"\uf02c"; // tags
	case Kind::Module:   return u8"\uf07b"; // folder
	}
	return u8"\uf111";
}

inline unsigned int KindColor(Kind k)
{
	// ABGR for ImU32 via IM_COL32(r,g,b,a) — return packed later at draw site
	switch (k)
	{
	case Kind::Keyword:  return 0xC9A0FFu; // soft violet RGB
	case Kind::Function: return 0xE6C07Bu; // gold
	case Kind::Method:   return 0x61AFEFu; // blue
	case Kind::Field:    return 0x9CDCFFu; // cyan
	case Kind::Type:     return 0x89D185u; // green
	case Kind::Enum:     return 0xD19A66u; // orange
	case Kind::Module:   return 0xABB2BFu; // gray
	}
	return 0xABB2BFu;
}

inline bool StartsWithInsensitive(const char* word, const char* prefix)
{
	if (!word || !prefix) return false;
	if (!*prefix) return true;
	for (; *prefix; ++word, ++prefix)
	{
		if (!*word) return false;
		if (std::tolower((unsigned char)*word) != std::tolower((unsigned char)*prefix))
			return false;
	}
	return true;
}

inline bool EqInsensitive(const std::string& a, const char* b)
{
	if (!b) return false;
	size_t i = 0;
	for (; i < a.size() && b[i]; ++i)
	{
		if (std::tolower((unsigned char)a[i]) != std::tolower((unsigned char)b[i]))
			return false;
	}
	return i == a.size() && b[i] == 0;
}

inline void AddUnique(std::vector<Item>& out, const char* w, Kind kind, const std::string& prefix)
{
	if (!w || !w[0]) return;
	if (!prefix.empty() && !StartsWithInsensitive(w, prefix.c_str()))
		return;
	for (size_t i = 0; i < out.size(); ++i)
		if (out[i].text == w) return;
	out.push_back(Item{ w, kind });
}

inline void AddList(std::vector<Item>& out, const char* const* words, size_t n,
	Kind kind, const std::string& prefix)
{
	for (size_t i = 0; i < n; ++i)
		AddUnique(out, words[i], kind, prefix);
}

inline void CollectFromBuffer(const std::string& text, const std::string& receiver,
	std::vector<Item>& out, const std::string& prefix)
{
	if (receiver.empty() || text.empty()) return;
	const std::string dot = receiver + ".";
	const std::string colon = receiver + ":";

	auto scan = [&](const std::string& needle, Kind kind)
	{
		size_t pos = 0;
		while ((pos = text.find(needle, pos)) != std::string::npos)
		{
			size_t i = pos + needle.size();
			if (i >= text.size()) break;
			if (!(std::isalpha((unsigned char)text[i]) || text[i] == '_'))
			{
				pos = i;
				continue;
			}
			size_t start = i;
			while (i < text.size() && (std::isalnum((unsigned char)text[i]) || text[i] == '_'))
				++i;
			AddUnique(out, text.substr(start, i - start).c_str(), kind, prefix);
			pos = i;
		}
	};
	scan(dot, Kind::Field);
	scan(colon, Kind::Method);

	size_t pos = 0;
	while ((pos = text.find("function ", pos)) != std::string::npos)
	{
		size_t i = pos + 9;
		while (i < text.size() && std::isspace((unsigned char)text[i])) ++i;
		while (i < text.size() && (std::isalnum((unsigned char)text[i]) || text[i] == '_')) ++i;
		if (i < text.size() && text[i] == ':')
		{
			++i;
			size_t start = i;
			while (i < text.size() && (std::isalnum((unsigned char)text[i]) || text[i] == '_'))
				++i;
			if (i > start)
				AddUnique(out, text.substr(start, i - start).c_str(), Kind::Method, prefix);
		}
		pos = i;
	}
}

inline void CollectCandidates(const std::string& receiver, const std::string& prefix,
	const std::string& buffer, std::vector<Item>& out, int maxCount = 80)
{
	out.clear();
	std::vector<Item> matches;
	matches.reserve(96);

	static const char* const kKeywords[] = {
		"and", "break", "do", "else", "elseif", "end", "false", "for", "function",
		"if", "in", "local", "nil", "not", "or", "repeat", "return", "then", "true",
		"until", "while", "class",
	};
	static const char* const kBuiltinFns[] = {
		"require", "pairs", "ipairs", "tonumber", "tostring", "type", "print",
		"error", "assert", "pcall", "xpcall", "select", "next",
		"setMouseCaptured", "warpMouseToCenter", "getWindowSize", "getMousePosition",
		"placeDecalAtCursor",
	};
	static const char* const kModules[] = {
		"math", "string", "table", "os", "coroutine", "Input", "Key", "MouseButton",
		"camera", "self", "owner", "allowMouseCapture", "editorAutoCapture",
	};
	static const char* const kTypes[] = {
		"GameObject", "Scene", "SceneGraph", "Vec2", "Vec3", "Vec4", "Quaternion", "Matrix",
		"Color", "Texture", "Model", "Cube", "Sphere", "Plane",
		"RenderingComponent", "DirectionalLight", "PointLight", "SpotLight",
		"AudioSource", "AudioManager", "ForwardRenderer", "PerspectiveProjection",
		"GenericShaderMaterial", "CustomShaderMaterial",
	};
	static const char* const kLifecycle[] = {
		"initialize", "init", "update", "destroy",
	};

	static const char* const kGameObjectMethods[] = {
		"init", "update", "destroy",
		"getPosition", "setPosition", "getRotation", "setRotation",
		"getScale", "setScale", "getDirection",
		"getWorldPosition", "getWorldRotation", "getWorldTransformation",
		"getLocalTransformation", "refreshTransformation",
		"getName", "setName", "setTransformationMatrix",
		"lookAtGameObject", "lookAtVec",
		"addComponent", "removeComponent", "addGameObject", "removeGameObject",
		"getParent", "haveParent", "addTag", "removeTag", "haveTag", "isStatic",
		"attachScript", "getComponent",
		"onUpdate", "onInit", "onDestroy",
	};

	static const char* const kSelfFields[] = {
		"owner", "input", "captured", "yaw", "pitch", "baseSpeed", "lookSensitivity",
		"moveFront", "moveBack", "strafeLeft", "strafeRight", "moveUp", "moveDown",
		"sprint", "lastMouseX", "lastMouseY", "lastTime", "ignoreNextMouseDelta",
	};
	static const char* const kSelfMethods[] = {
		"initialize", "init", "update", "destroy", "applyRotation",
	};

	static const char* const kInputMethods[] = {
		"onKeyPressed", "onKeyReleased",
		"onMouseButtonPressed", "onMouseButtonReleased",
		"onMouseMoved", "onMouseWheelMoved", "new",
	};

	static const char* const kKeyEnums[] = {
		"W", "A", "S", "D", "E", "Q", "R", "F", "Z", "X", "C", "V",
		"LShift", "RShift", "LCtrl", "RCtrl", "LAlt", "RAlt",
		"Tab", "Space", "Escape", "Return", "Backspace",
		"Up", "Down", "Left", "Right",
		"F1", "F2", "F3", "F4", "F5", "F6", "F7", "F8", "F9", "F10", "F11", "F12",
	};

	static const char* const kVecFields[] = { "x", "y", "z", "w" };
	static const char* const kVecMethods[] = {
		"dot", "cross", "normalize", "magnitude", "abs", "negate", "distance", "lerp", "new",
	};
	static const char* const kQuatMethods[] = {
		"axisToQuaternion", "getEulerRotation", "setRotationFromEuler",
		"slerp", "nlerp", "inverse", "magnitude", "dot", "new",
	};
	static const char* const kMathFns[] = {
		"abs", "acos", "asin", "atan", "atan2", "ceil", "cos", "deg", "exp",
		"floor", "fmod", "huge", "log", "max", "min", "modf", "pi", "rad",
		"random", "randomseed", "sin", "sqrt", "tan",
	};
	static const char* const kStringFns[] = {
		"byte", "char", "find", "format", "gmatch", "gsub", "len", "lower",
		"match", "rep", "reverse", "sub", "upper",
	};
	static const char* const kTableFns[] = {
		"concat", "insert", "remove", "sort", "unpack", "pack", "move",
	};

	if (receiver.empty())
	{
		AddList(matches, kKeywords, sizeof(kKeywords) / sizeof(kKeywords[0]), Kind::Keyword, prefix);
		AddList(matches, kBuiltinFns, sizeof(kBuiltinFns) / sizeof(kBuiltinFns[0]), Kind::Function, prefix);
		AddList(matches, kModules, sizeof(kModules) / sizeof(kModules[0]), Kind::Module, prefix);
		AddList(matches, kTypes, sizeof(kTypes) / sizeof(kTypes[0]), Kind::Type, prefix);
		AddList(matches, kLifecycle, sizeof(kLifecycle) / sizeof(kLifecycle[0]), Kind::Method, prefix);
	}
	else if (EqInsensitive(receiver, "self"))
	{
		AddList(matches, kSelfFields, sizeof(kSelfFields) / sizeof(kSelfFields[0]), Kind::Field, prefix);
		AddList(matches, kSelfMethods, sizeof(kSelfMethods) / sizeof(kSelfMethods[0]), Kind::Method, prefix);
		AddList(matches, kGameObjectMethods, sizeof(kGameObjectMethods) / sizeof(kGameObjectMethods[0]), Kind::Method, prefix);
		CollectFromBuffer(buffer, "self", matches, prefix);
	}
	else if (EqInsensitive(receiver, "owner") || EqInsensitive(receiver, "camera"))
	{
		AddList(matches, kGameObjectMethods, sizeof(kGameObjectMethods) / sizeof(kGameObjectMethods[0]), Kind::Method, prefix);
		CollectFromBuffer(buffer, receiver, matches, prefix);
	}
	else if (EqInsensitive(receiver, "Input") || EqInsensitive(receiver, "input"))
	{
		AddList(matches, kInputMethods, sizeof(kInputMethods) / sizeof(kInputMethods[0]), Kind::Method, prefix);
	}
	else if (EqInsensitive(receiver, "Key"))
	{
		AddList(matches, kKeyEnums, sizeof(kKeyEnums) / sizeof(kKeyEnums[0]), Kind::Enum, prefix);
	}
	else if (EqInsensitive(receiver, "Vec3") || EqInsensitive(receiver, "Vec2") || EqInsensitive(receiver, "Vec4"))
	{
		AddList(matches, kVecFields, sizeof(kVecFields) / sizeof(kVecFields[0]), Kind::Field, prefix);
		AddList(matches, kVecMethods, sizeof(kVecMethods) / sizeof(kVecMethods[0]), Kind::Method, prefix);
	}
	else if (EqInsensitive(receiver, "Quaternion") || EqInsensitive(receiver, "qPitch")
		|| EqInsensitive(receiver, "qYaw"))
	{
		AddList(matches, kVecFields, sizeof(kVecFields) / sizeof(kVecFields[0]), Kind::Field, prefix);
		AddList(matches, kQuatMethods, sizeof(kQuatMethods) / sizeof(kQuatMethods[0]), Kind::Method, prefix);
	}
	else if (EqInsensitive(receiver, "math"))
	{
		AddList(matches, kMathFns, sizeof(kMathFns) / sizeof(kMathFns[0]), Kind::Function, prefix);
	}
	else if (EqInsensitive(receiver, "string"))
	{
		AddList(matches, kStringFns, sizeof(kStringFns) / sizeof(kStringFns[0]), Kind::Function, prefix);
	}
	else if (EqInsensitive(receiver, "table"))
	{
		AddList(matches, kTableFns, sizeof(kTableFns) / sizeof(kTableFns[0]), Kind::Function, prefix);
	}
	else
	{
		AddList(matches, kGameObjectMethods, sizeof(kGameObjectMethods) / sizeof(kGameObjectMethods[0]), Kind::Method, prefix);
		CollectFromBuffer(buffer, receiver, matches, prefix);
	}

	std::sort(matches.begin(), matches.end(),
		[](const Item& a, const Item& b) { return a.text < b.text; });
	if ((int)matches.size() > maxCount)
		matches.resize((size_t)maxCount);
	out.swap(matches);
}

} // namespace LuaCompletion

#endif
