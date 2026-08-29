//=============================================================================
// Name        : AIAssistant.cpp
// Description : AI Assistant panel + worker-thread libcurl client (see .h).
//=============================================================================

#include "AIAssistant.h"

#include <curl/curl.h>

#include <algorithm>
#include <chrono>
#include <cctype>
#include <cinttypes>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>

namespace fs = std::filesystem;

using json = nlohmann::json;

//=============================================================================
// Provider presets
//=============================================================================

const AIProviderPreset kAIProviders[] = {
	{ "openai",     "OpenAI",                   "https://api.openai.com/v1",      true  },
	{ "openrouter", "OpenRouter",               "https://openrouter.ai/api/v1",   true  },
	{ "anthropic",  "Anthropic (Claude)",       "https://api.anthropic.com/v1",   true  },
	{ "groq",       "Groq",                     "https://api.groq.com/v1",        true  },
	{ "mistral",    "Mistral",                  "https://api.mistral.ai/v1",      true  },
	{ "ollama",     "Ollama (local)",           "http://localhost:11434/v1",      false },
	{ "lmstudio",   "LM Studio (local)",        "http://localhost:1234/v1",       false },
	{ "custom",     "Custom (OpenAI-compatible)","http://localhost:8000/v1",      false },
};
const size_t kAIProviderCount = sizeof(kAIProviders) / sizeof(kAIProviders[0]);

//=============================================================================
// Tool table - mirrors the editor's agent commands (Editor::HandleAgentCommand),
// which is also what the MCP bridge drives, so the AI can do everything the
// external agents can.
//=============================================================================

struct AIParamDef { std::string name; std::string type; std::string desc; bool required; };
struct AIToolDef  { std::string name; std::string desc; std::vector<AIParamDef> params; };

static const AIToolDef kAITools[] = {
	{ "status",         "Editor status: open project, active scene, dirty/playing state.", {} },
	{ "log",            "Read the last N lines of the editor log.",
		{ { "lines", "integer", "Number of log lines", false } } },
	{ "scene_state",    "Whole scene: every object with transform, components and children. Large - prefer get_object when you already know the name.", {} },
	{ "get_object",     "One object's transform, tags, components and children.",
		{ { "name", "string", "Object name", true } } },
	{ "select_object",  "Select an object in the editor (shows it in the hierarchy and Properties).",
		{ { "name", "string", "Object name", true } } },
	{ "list_scenes",    "List the project's scene files and which one is active.", {} },
	{ "list_assets",    "List project asset files (project-relative paths).",
		{ { "under", "string", "Limit to a subfolder, e.g. assets/lua", false },
		  { "extension", "string", "Only files ending with this, e.g. .lua or .mat", false },
		  { "recursive", "boolean", "Recurse into subfolders (default true)", false } } },
	{ "read_script",    "Read a Lua script's text.",
		{ { "path", "string", "Project-relative path, e.g. assets/lua/player.lua", true } } },
	{ "write_script",   "Overwrite a Lua script's text (the whole file). Reloads it in the editor if it is open.",
		{ { "path", "string", "Project-relative path, e.g. assets/lua/player.lua", true },
		  { "text", "string", "Full new file contents", true } } },
	{ "create_script",  "Create a new Lua script from the editor's template.",
		{ { "name", "string", "Script name (no extension)", true },
		  { "kind", "string", "gameobject or scene", false } } },
	{ "open_project",   "Open a project by path.",
		{ { "path", "string", "Project folder or project.json path", true } } },
	{ "create_material","Create a new material asset (generic or custom).",
		{ { "name", "string", "Material name", true },
		  { "kind", "string", "generic or custom", false } } },
	{ "delete_asset",   "Delete a project asset by relative path.",
		{ { "path", "string", "Project-relative asset path", true } } },
	{ "get_material_graph", "Read a Custom Shader material's node graph.",
		{ { "path", "string", "Material .mat path", true } } },
	{ "set_material_graph", "Replace a Custom Shader material's node graph (Inspector mode).",
		{ { "path", "string", "Material .mat path", true },
		  { "nodes", "array", "Graph nodes", true },
		  { "connections", "array", "Graph connections", false } } },
	{ "get_material_text", "Read a Custom Shader material's Text-mode GLSL snippet and its named texture inputs.",
		{ { "path", "string", "Material .mat path", true } } },
	{ "set_material_text", "Write a Custom Shader material's Text-mode GLSL snippet (hand-written shader code) and switch it to Text mode.",
		{ { "path", "string", "Material .mat path", true },
		  { "text", "string", "GLSL snippet: assignments for Albedo/Normal/Metallic/Roughness/Emissive/Occlusion", true },
		  { "textures", "array", "Optional named texture inputs: [{\"name\":\"uTexture\",\"path\":\"brick.png\"}]", false } } },
	{ "apply_material", "Compile a material's graph/text and apply it to the live material.",
		{ { "path", "string", "Material .mat path", true } } },
	{ "add_object",     "Add an empty game object.",
		{ { "name", "string", "Object name", true },
		  { "parent", "string", "Parent object name (omit for root)", false },
		  { "position", "array", "Position [x, y, z]", false },
		  { "rotation", "array", "Rotation, euler radians [x, y, z]", false },
		  { "scale", "array", "Scale [x, y, z]", false } } },
	{ "add_primitive",  "Add a primitive mesh: Cube, Sphere, Cone, Cylinder, Plane, Capsule, Torus, TorusKnot.",
		{ { "name", "string", "Object name", true },
		  { "shape", "string", "Cube, Sphere, Cone, Cylinder, Plane, Capsule, Torus, TorusKnot", true },
		  { "position", "array", "Position [x, y, z]", false },
		  { "rotation", "array", "Rotation, euler radians [x, y, z]", false },
		  { "scale", "array", "Scale [x, y, z]", false },
		  { "color", "array", "Color [r, g, b, a] 0..1", false },
		  { "parent", "string", "Parent object name (omit for root)", false } } },
	{ "add_ui",         "Add a screen-space UI component to an object: canvas, rect, image or text. Image and text add a rect if the object has none.",
		{ { "object", "string", "Object name", true },
		  { "kind", "string", "canvas, rect, image or text", true },
		  { "font", "string", "Font file for text (project-relative). Defaults to a font already in the project, or imports one.", false } } },
	{ "add_light",      "Add a light: DirectionalLight, PointLight or SpotLight.",
		{ { "name", "string", "Light name", true },
		  { "type", "string", "DirectionalLight, PointLight or SpotLight", true },
		  { "position", "array", "Position [x, y, z]", false },
		  { "color", "array", "Color [r, g, b, a] 0..1", false },
		  { "intensity", "number", "Light intensity", false },
		  { "parent", "string", "Parent object name (omit for root)", false } } },
	{ "add_audio",      "Add an audio source.",
		{ { "name", "string", "Source name", true },
		  { "file", "string", "Sound file path", true },
		  { "parent", "string", "Parent object name (omit for root)", false },
		  { "position", "array", "Position [x, y, z]", false },
		  { "looping", "boolean", "Loop the audio", false },
		  { "volume", "number", "Volume 0..1", false } } },
	{ "add_particles",  "Add a particle emitter.",
		{ { "name", "string", "Emitter name", true },
		  { "preset", "string", "default, fire, smoke or explosion", false },
		  { "position", "array", "Position [x, y, z]", false },
		  { "parent", "string", "Parent object name (omit for root)", false } } },
	{ "add_physics",    "Add a physics component: Box, Sphere, Capsule, Cone, Cylinder, StaticPlane, ConvexHull.",
		{ { "name", "string", "Body name", true },
		  { "shape", "string", "Box, Sphere, Capsule, Cone, Cylinder, StaticPlane, ConvexHull", true },
		  { "position", "array", "Position [x, y, z]", false },
		  { "mass", "number", "Mass in kg", false } } },
	{ "add_model",      "Add a 3D model (.p3dm, or .obj/.fbx/.gltf/... which gets imported).",
		{ { "name", "string", "Object name", true },
		  { "model", "string", "Model file path", true },
		  { "position", "array", "Position [x, y, z]", false },
		  { "parent", "string", "Parent object name (omit for root)", false } } },
	{ "add_camera",     "Add a scene camera.",
		{ { "name", "string", "Camera name", true },
		  { "position", "array", "Position [x, y, z]", false },
		  { "fov", "number", "Field of view in degrees", false } } },
	{ "set_transform",  "Set an object's position/rotation/scale.",
		{ { "name", "string", "Object name", true },
		  { "transform", "object", "{position:[x,y,z], rotation:[x,y,z], scale:[x,y,z]}", false } } },
	{ "set_tags",       "Add and/or remove tags on a game object.",
		{ { "name", "string", "Object name", true },
		  { "add", "array", "Tags to add", false },
		  { "remove", "array", "Tags to remove", false } } },
	{ "rename",         "Rename a game object.",
		{ { "name", "string", "Current object name", true },
		  { "newName", "string", "New name", true } } },
	{ "reparent",       "Move a game object under a new parent.",
		{ { "name", "string", "Object name", true },
		  { "newParent", "string", "New parent name (omit for root)", false } } },
	{ "duplicate",      "Duplicate a game object with its children.",
		{ { "name", "string", "Object name", true } } },
	{ "delete_object",  "Remove a game object (and its children) from the scene.",
		{ { "name", "string", "Object name", true } } },
	{ "create_prefab",  "Save a game object and its children as a reusable .prefab asset; the object becomes the first instance of it.",
		{ { "name", "string", "Object name", true },
		  { "prefabName", "string", "Prefab file name (defaults to the object's name)", false } } },
	{ "instantiate_prefab", "Add an instance of a .prefab to the scene.",
		{ { "path", "string", "Prefab path (assets/prefabs/...)", true },
		  { "position", "array", "[x,y,z] to place it at", false } } },
	{ "apply_prefab",   "Write an instance's current state back over its prefab, updating every unmodified instance. Not undoable.",
		{ { "name", "string", "Instance object name", true } } },
	{ "revert_prefab",  "Discard an instance's local changes and rebuild it from its prefab, keeping its name, transform and tags.",
		{ { "name", "string", "Instance object name", true } } },
	{ "unpack_prefab",  "Break an instance's link to its prefab; the objects stay, future prefab edits no longer reach them.",
		{ { "name", "string", "Instance object name", true } } },
	{ "prefab_state",   "List the prefab instances in the scene and which of them have local changes.", {} },
	{ "build_game",     "Export a runnable game folder: the player, engine shaders, and the project's scenes and assets.",
		{ { "outputDir", "string", "Folder to build into (must be outside the project)", true },
		  { "startupScene", "string", "Scene to start on (defaults to the project's active scene)", false },
		  { "title", "string", "Window title (defaults to the project name)", false },
		  { "width", "integer", "Window width", false },
		  { "height", "integer", "Window height", false },
		  { "fullscreen", "boolean", "Start fullscreen", false } } },
	{ "undo",           "Undo the last scene edit.", {} },
	{ "redo",           "Redo the last undone scene edit.", {} },
	{ "undo_material",  "Undo the last edit in the focused Material Editor document.", {} },
	{ "redo_material",  "Redo the last undone edit in the focused Material Editor document.", {} },
	{ "attach_script",  "Attach a Lua script component to a game object.",
		{ { "name", "string", "Object name", true },
		  { "scriptFile", "string", "Lua script path (assets/lua/...)", true },
		  { "data", "object", "Initial script data", false } } },
	{ "detach_component","Detach a component from a game object.",
		{ { "name", "string", "Object name", true },
		  { "componentType", "string", "Component type to detach", true } } },
	{ "set_material",   "Edit a scene material's properties (color, opacity, maps, ...).",
		{ { "object", "string", "Object name", true },
		  { "material", "object", "Material properties to set (color, opacity, metallic, ...)", false } } },
	{ "assign_material","Assign a material asset onto an object's submesh.",
		{ { "object", "string", "Object name", true },
		  { "path", "string", "Material .mat path", true },
		  { "submesh", "integer", "Submesh index", false } } },
	{ "save_scene",     "Save the active scene to disk.", {} },
	{ "save_scene_as",  "Save the active scene to a new path.",
		{ { "path", "string", "Project-relative scene path, e.g. scenes/Level2.json", true } } },
	{ "load_scene",     "Load a scene file into the editor.",
		{ { "path", "string", "Project-relative scene path, e.g. scenes/Level2.json (as returned by list_scenes)", true } } },
	{ "set_renderer",   "Switch renderer (forward or deferred) for all open scenes.",
		{ { "type", "string", "forward or deferred", true } } },
	{ "play",           "Start play mode (run the game).", {} },
	{ "stop_play",      "Stop play mode.", {} },
	{ "screenshot",     "Capture the scene viewport (returns base64 PNG).",
		{ { "live", "boolean", "Read back what the Scene View is actually showing, using the project's own renderer (Deferred included), instead of re-rendering through the offscreen forward preview", false } } },
	{ "reload",         "Reload the active scene from disk if the file changed.", {} },
};
const size_t kAIToolCount = sizeof(kAITools) / sizeof(kAITools[0]);

//=============================================================================
// Small helpers
//=============================================================================

static std::string Truncate(const std::string& s, size_t max)
{
	if (s.size() <= max) return s;
	return s.substr(0, max) + " …(truncated)";
}

static std::string JoinBase(const std::string& base, const std::string& suffix)
{
	std::string b = base;
	while (b.size() > 1 && b.back() == '/') b.pop_back();
	if (b.empty()) return suffix;
	if (b.back() != '/') b += '/';
	return b + suffix;
}

static nlohmann::json ParamSchema(const AIParamDef& p)
{
	nlohmann::json s;
	if (p.type == "array")
	{
		s["type"] = "array";
		s["items"] = { { "type", "number" } };
	}
	else
	{
		s["type"] = p.type;
	}
	s["description"] = p.desc;
	return s;
}

static void FillToolParams(const AIToolDef& t, nlohmann::json& props, nlohmann::json& required)
{
	for (const auto& p : t.params)
	{
		props[p.name] = ParamSchema(p);
		if (p.required)
			required.push_back(p.name);
	}
}

static nlohmann::json BuildOpenAITools()
{
	nlohmann::json::array_t out;
	for (size_t t = 0; t < kAIToolCount; ++t)
	{
		const AIToolDef& def = kAITools[t];
		nlohmann::json props, required;
		FillToolParams(def, props, required);
		// JSON Schema: "properties" must be an object - null is rejected by
		// some servers (LM Studio returns 400 for it).
		if (props.is_null())
			props = nlohmann::json::object();
		nlohmann::json params = { { "type", "object" }, { "properties", std::move(props) } };
		if (!required.empty()) params["required"] = std::move(required);
		nlohmann::json fn = {
			{ "name", def.name },
			{ "description", def.desc },
			{ "parameters", std::move(params) },
		};
		out.push_back({ { "type", "function" }, { "function", std::move(fn) } });
	}
	return out;
}

static nlohmann::json BuildAnthropicTools()
{
	nlohmann::json::array_t out;
	for (size_t t = 0; t < kAIToolCount; ++t)
	{
		const AIToolDef& def = kAITools[t];
		nlohmann::json props, required;
		FillToolParams(def, props, required);
		if (props.is_null())
			props = nlohmann::json::object(); // JSON Schema: must be an object, not null
		nlohmann::json schema = { { "type", "object" }, { "properties", std::move(props) } };
		if (!required.empty()) schema["required"] = std::move(required);
		out.push_back({
			{ "name", def.name },
			{ "description", def.desc },
			{ "input_schema", std::move(schema) },
		});
	}
	return out;
}

// Human-readable reference of every tool the model can call, generated from
// the same table the JSON schemas come from - so it can never drift out of
// sync. Injected into the system prompt when tools are enabled.
static std::string BuildToolReference()
{
	std::ostringstream os;
	os << "You are the AI assistant inside the Pyros3D editor. You control the editor by calling tools; ";
	os << "each call returns a JSON result ({\"ok\":true,...} or {\"ok\":false,\"error\":\"...\"}). ";
	os << "Work in small steps: call a tool, read its result, then continue.\n\n";
	os << "Available tools:\n";
	for (size_t t = 0; t < kAIToolCount; ++t)
	{
		const AIToolDef& def = kAITools[t];
		os << "\n### " << def.name << "\n" << def.desc << "\n";
		if (def.params.empty())
			os << "  (no parameters)\n";
		for (const auto& p : def.params)
			os << "  - " << p.name << " (" << p.type << (p.required ? ", required" : ", optional") << "): " << p.desc << "\n";
	}
	os << "\nMaterial workflow: create_material -> set_material_graph (node graph) or set_material_text (GLSL text) -> apply_material -> assign_material to put it on an object. ";
	os << "Scene workflow: add_primitive/add_object/add_light -> set_transform/set_tags -> attach_script for behavior -> save_scene.\n";
	return os.str();
}

//=============================================================================
// curl worker plumbing
//=============================================================================

struct RoundCtx {
	AIChatClient* self = NULL;
	bool isStream = false;
	bool isAnthropic = false;
	std::string lineBuf;
	std::string body;
	std::string text;
	std::string streamError;
	nlohmann::json usage;
	std::map<int, ToolCall> calls;
};

static void PushDelta(RoundCtx* ctx, const std::string& text)
{
	if (text.empty() || !ctx->self) return;
	ctx->text += text;
	AIEvent e;
	e.type = AIEvent::Delta;
	e.text = text;
	ctx->self->Push(e);
}

static void ParseStreamEvent(RoundCtx* ctx, const nlohmann::json& obj)
{
	if (ctx->isAnthropic)
	{
		const std::string t = obj.value("type", std::string());
		if (t == "message_start" && obj.contains("message") && obj["message"].contains("usage"))
		{
			ctx->usage = obj["message"]["usage"];
		}
		else if (t == "content_block_start")
		{
			const nlohmann::json b = obj.value("content_block", nlohmann::json::object());
			if (b.value("type", std::string()) == "tool_use")
			{
				ToolCall c;
				c.id = b.value("id", std::string());
				c.name = b.value("name", std::string());
				ctx->calls[obj.value("index", 0)] = c;
			}
		}
		else if (t == "content_block_delta")
		{
			const nlohmann::json d = obj.value("delta", nlohmann::json::object());
			const std::string dt = d.value("type", std::string());
			if (dt == "text_delta")
				PushDelta(ctx, d.value("text", std::string()));
			else if (dt == "input_json_delta")
			{
				auto it = ctx->calls.find(obj.value("index", -1));
				if (it != ctx->calls.end())
					it->second.argsRaw += d.value("partial_json", std::string());
			}
		}
		else if (t == "message_delta" && obj.contains("usage"))
		{
			nlohmann::json u = ctx->usage.is_object() ? ctx->usage : nlohmann::json::object();
			u.update(obj["usage"]);
			ctx->usage = std::move(u);
		}
		else if (t == "error")
		{
			const nlohmann::json e = obj.value("error", nlohmann::json::object());
			ctx->streamError = e.value("message", std::string("provider stream error"));
		}
		return;
	}

	// OpenAI-compatible
	if (obj.contains("usage") && obj["usage"].is_object())
		ctx->usage = obj["usage"];
	if (!obj.contains("choices") || !obj["choices"].is_array())
		return;
	for (const auto& ch : obj["choices"])
	{
		const nlohmann::json delta = ch.value("delta", nlohmann::json::object());
		if (!delta.is_object()) continue;
		if (delta.contains("content") && delta["content"].is_string())
			PushDelta(ctx, delta["content"].get<std::string>());
		// Reasoning models (Qwen "thinking", ...) stream reasoning_content
		// before the answer - surface it separately instead of dropping it.
		if (delta.contains("reasoning_content") && delta["reasoning_content"].is_string())
		{
			const std::string text = delta["reasoning_content"].get<std::string>();
			if (!text.empty() && ctx->self)
			{
				AIEvent e;
				e.type = AIEvent::Reasoning;
				e.text = text;
				ctx->self->Push(e);
			}
		}
		if (!delta.contains("tool_calls") || !delta["tool_calls"].is_array()) continue;
		for (const auto& tc : delta["tool_calls"])
		{
			const int idx = tc.value("index", 0);
			ToolCall& slot = ctx->calls[idx];
			if (tc.contains("id") && tc["id"].is_string())
				slot.id = tc["id"].get<std::string>();
			const nlohmann::json fn = tc.value("function", nlohmann::json::object());
			if (!fn.is_object()) continue;
			if (fn.contains("name") && fn["name"].is_string())
				slot.name = fn["name"].get<std::string>();
			if (fn.contains("arguments") && fn["arguments"].is_string())
				slot.argsRaw += fn["arguments"].get<std::string>();
		}
	}
}

static size_t RoundWriteCb(char* p, size_t sz, size_t nm, void* ud)
{
	RoundCtx* ctx = (RoundCtx*)ud;
	const size_t len = sz * nm;
	if (!ctx) return len;
	// Stop was pressed: drop the data and abort the transfer (returning 0
	// makes curl fail with CURLE_WRITE_ERROR - handled as a clean stop).
	if (ctx->self && ctx->self->ShouldCancel())
		return 0;
	if (!ctx->isStream)
	{
		ctx->body.append(p, len);
		return len;
	}

	ctx->lineBuf.append(p, len);
	size_t nl;
	while ((nl = ctx->lineBuf.find('\n')) != std::string::npos)
	{
		std::string line = ctx->lineBuf.substr(0, nl);
		ctx->lineBuf.erase(0, nl + 1);
		if (!line.empty() && line.back() == '\r') line.pop_back();
		if (line.rfind("data:", 0) != 0) continue;
		std::string payload = line.substr(5);
		while (!payload.empty() && (payload.front() == ' ' || payload.front() == '\t'))
			payload.erase(payload.begin());
		if (payload == "[DONE]") continue;
		nlohmann::json obj;
		try
		{
			obj = nlohmann::json::parse(payload);
		}
		catch (...)
		{
			continue;
		}
		ParseStreamEvent(ctx, obj);
	}
	return len;
}

static int RoundProgressCb(void* ud, curl_off_t, curl_off_t, curl_off_t, curl_off_t)
{
	const RoundCtx* ctx = (const RoundCtx*)ud;
	if (ctx && ctx->self && ctx->self->ShouldCancel())
		return 1; // abort the transfer
	return 0;
}

//=============================================================================
// AIChatClient
//=============================================================================

AIChatClient::AIChatClient()
{
	curl_global_init(CURL_GLOBAL_DEFAULT);
}

AIChatClient::~AIChatClient()
{
	cancel_ = true;
	toolCV_.notify_all(); // wake a worker parked in the tool-result wait
	if (worker_.joinable())
		worker_.join();
}

void AIChatClient::Push(AIEvent e)
{
	std::lock_guard<std::mutex> lk(queueMutex_);
	queue_.push_back(std::move(e));
}

std::vector<AIEvent> AIChatClient::DrainEvents()
{
	std::lock_guard<std::mutex> lk(queueMutex_);
	std::vector<AIEvent> out;
	out.swap(queue_);
	return out;
}

void AIChatClient::Stop()
{
	cancel_ = true;
	toolCV_.notify_all(); // wake a worker parked in the tool-result wait
}

bool AIChatClient::Start(const AIChatRequest& req)
{
	bool expected = false;
	if (!busy_.compare_exchange_strong(expected, true))
		return false;
	cancel_ = false;
	// Join the finished predecessor before replacing the thread handle -
	// assigning a std::thread while it is still joinable calls
	// std::terminate() and aborts the whole editor.
	if (worker_.joinable())
		worker_.join();
	worker_ = std::thread(&AIChatClient::Worker, this, req);
	return true;
}

void AIChatClient::PumpToolRequests()
{
	std::vector<AIToolRequest*> ready;
	{
		std::lock_guard<std::mutex> lk(toolMutex_);
		for (auto& t : toolQueue_)
			if (!t.done)
				ready.push_back(&t);
		if (ready.empty())
			return;
	}

	// Runs on the editor's main thread - safe for the SceneGraph.
	for (auto* t : ready)
	{
		if (!toolExecutor_)
		{
			t->failed = true;
			t->error = "no tool executor attached";
		}
		else
		{
			try
			{
				t->result = toolExecutor_(t->name, t->args);
				if (t->result.is_object() && t->result.contains("error"))
				{
					t->failed = true;
					t->error = t->result.value("error", std::string("tool error"));
				}
			}
			catch (const std::exception& e)
			{
				t->failed = true;
				t->error = e.what();
			}
			catch (...)
			{
				t->failed = true;
				t->error = "unknown tool error";
			}
		}
		t->done = true;
	}
	{
		std::lock_guard<std::mutex> lk(toolMutex_);
		toolCV_.notify_all();
	}
}

// ---------------------------------------------------------------------------

bool AIChatClient::DoRound(const AIChatRequest& req, bool isAnthropic,
	const nlohmann::json& messages, const std::string& system,
	const nlohmann::json& tools, std::string& textOut,
	std::vector<ToolCall>& callsOut, std::string& errOut)
{
	std::string url;
	nlohmann::json body;
	std::vector<std::string> headers;
	headers.push_back("Content-Type: application/json");
	headers.push_back("Accept: text/event-stream");

	if (isAnthropic)
	{
		url = JoinBase(req.baseUrl, "messages");
		headers.push_back("anthropic-version: 2023-06-01");
		if (!req.apiKey.empty())
			headers.push_back("x-api-key: " + req.apiKey);
		nlohmann::json::array_t msgs;
		for (const auto& m : messages)
		{
			const std::string role = m.value("role", std::string());
			if (role == "user" || role == "assistant")
				msgs.push_back(m);
		}
		body["model"] = req.model;
		body["messages"] = std::move(msgs);
		body["max_tokens"] = req.maxTokens > 0 ? req.maxTokens : 4096;
		body["temperature"] = req.temperature;
		body["stream"] = true;
		if (!system.empty())
			body["system"] = system;
		if (!tools.is_null())
			body["tools"] = tools;
	}
	else
	{
		url = JoinBase(req.baseUrl, "chat/completions");
		if (!req.apiKey.empty())
			headers.push_back("Authorization: Bearer " + req.apiKey);
		body["model"] = req.model;
		if (!system.empty())
		{
			// OpenAI-style: the system prompt is the first message.
			nlohmann::json::array_t msgs;
			msgs.push_back({ { "role", "system" }, { "content", system } });
			for (const auto& m : messages)
				msgs.push_back(m);
			body["messages"] = std::move(msgs);
		}
		else
		{
			body["messages"] = messages;
		}
		body["temperature"] = req.temperature;
		if (req.maxTokens > 0)
			body["max_tokens"] = req.maxTokens;
		body["stream"] = true;
		if (!tools.is_null())
			body["tools"] = tools;
	}

	const std::string bodyStr = body.dump();
	if (const char* dbgEnv = std::getenv("AI_DEBUG_BODY"))
		if (*dbgEnv)
			fprintf(stderr, "[AI debug] POST %s\n%s\n", url.c_str(), bodyStr.c_str());

	RoundCtx ctx;
	ctx.self = this;
	ctx.isStream = true;
	ctx.isAnthropic = isAnthropic;

	CURL* curl = curl_easy_init();
	if (!curl)
	{
		errOut = "curl_easy_init failed";
		return false;
	}
	struct curl_slist* hdrs = NULL;
	for (const auto& h : headers)
		hdrs = curl_slist_append(hdrs, h.c_str());

	curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
	curl_easy_setopt(curl, CURLOPT_POST, 1L);
	curl_easy_setopt(curl, CURLOPT_POSTFIELDS, bodyStr.c_str());
	curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)bodyStr.size());
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, hdrs);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, RoundWriteCb);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &ctx);
	curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, RoundProgressCb);
	curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &ctx);
	curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
	curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
	curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 20L);
	curl_easy_setopt(curl, CURLOPT_TIMEOUT, 1800L);

	const CURLcode rc = curl_easy_perform(curl);
	long code = 0;
	curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &code);
	if (hdrs) curl_slist_free_all(hdrs);
	curl_easy_cleanup(curl);

	if (rc == CURLE_ABORTED_BY_CALLBACK || (rc == CURLE_WRITE_ERROR && cancel_.load()))
	{
		// User pressed Stop - not an error.
		textOut = ctx.text;
		return true;
	}
	if (rc != CURLE_OK)
	{
		errOut = std::string("request failed: ") + curl_easy_strerror(rc);
		if (!ctx.body.empty())
			errOut += " - " + Truncate(ctx.body, 500);
		textOut = ctx.text;
		return false;
	}
	if (code >= 400)
	{
		// Diagnostics: URL + body, so a 4xx can be fixed without guessing.
		errOut = "HTTP " + std::to_string(code) + " from provider (POST " + url + ")";
		if (!ctx.body.empty())
			errOut += " - " + Truncate(ctx.body, 800);
		if (code == 404)
			// The client appends /chat/completions to the Base URL - a 404
			// almost always means the Base URL points at the wrong root.
			errOut += " - check the Base URL: it must be the API root (e.g. http://host:1234/v1), the client appends /chat/completions to it";
		textOut = ctx.text;
		return false;
	}
	if (!ctx.streamError.empty())
		errOut = ctx.streamError;

	textOut = ctx.text;
	callsOut.clear();
	for (auto& kv : ctx.calls)
	{
		ToolCall c = kv.second;
		if (!c.argsRaw.empty())
		{
			try
			{
				nlohmann::json parsed = nlohmann::json::parse(c.argsRaw);
				c.args = parsed.is_object() ? parsed : nlohmann::json::object({ { "value", std::move(parsed) } });
			}
			catch (...)
			{
				c.args = nlohmann::json::object({ { "raw", c.argsRaw } });
			}
		}
		if (c.id.empty())
			c.id = "call_" + std::to_string(kv.first);
		callsOut.push_back(std::move(c));
	}
	// Usage is pushed by the Worker after the round.
	if (ctx.usage.is_object())
	{
		AIEvent e;
		e.type = AIEvent::Usage;
		e.extra = ctx.usage;
		Push(e);
	}
	return errOut.empty();
}

bool AIChatClient::FetchModels(const AIChatRequest& req, nlohmann::json& modelsOut, std::string& errOut)
{
	std::string url;
	std::vector<std::string> headers;
	if (req.provider == "ollama")
	{
		std::string b = req.baseUrl;
		while (!b.empty() && b.back() == '/') b.pop_back();
		if (b.size() >= 3 && b.compare(b.size() - 3, 3, "/v1") == 0)
			b.erase(b.size() - 3);
		url = b + "/api/tags";
	}
	else if (req.provider == "anthropic")
	{
		url = JoinBase(req.baseUrl, "models");
		if (!req.apiKey.empty())
			headers.push_back("x-api-key: " + req.apiKey);
	}
	else
	{
		url = JoinBase(req.baseUrl, "models");
		if (!req.apiKey.empty())
			headers.push_back("Authorization: Bearer " + req.apiKey);
	}

	RoundCtx ctx;
	ctx.self = NULL;
	ctx.isStream = false;

	CURL* curl = curl_easy_init();
	if (!curl)
	{
		errOut = "curl_easy_init failed";
		return false;
	}
	struct curl_slist* hdrs = NULL;
	for (const auto& h : headers)
		hdrs = curl_slist_append(hdrs, h.c_str());
	curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, hdrs);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, RoundWriteCb);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &ctx);
	curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
	curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 15L);
	curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);

	const CURLcode rc = curl_easy_perform(curl);
	long code = 0;
	curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &code);
	if (hdrs) curl_slist_free_all(hdrs);
	curl_easy_cleanup(curl);

	if (rc != CURLE_OK)
	{
		errOut = std::string("model list failed: ") + curl_easy_strerror(rc);
		return false;
	}
	if (code >= 400)
	{
		errOut = ctx.body.empty()
			? ("HTTP " + std::to_string(code) + " while listing models")
			: Truncate(ctx.body, 400);
		return false;
	}

	nlohmann::json data;
	try
	{
		data = nlohmann::json::parse(ctx.body);
	}
	catch (...)
	{
		errOut = "could not parse model list";
		return false;
	}
	nlohmann::json::array_t out;
	if (data.is_object())
	{
		const nlohmann::json* items = NULL;
		for (const char* key : { "data", "models", "items" })
		{
			if (data.contains(key) && data[key].is_array())
			{
				items = &data[key];
				break;
			}
		}
		if (items)
		{
			for (const auto& i : *items)
			{
				if (i.is_string()) out.push_back(i);
				else if (i.is_object())
				{
					// id (OpenAI/Anthropic), key/display_name (LM Studio), name (Ollama).
					for (const char* key : { "id", "key", "name", "display_name" })
					{
						if (i.contains(key) && i[key].is_string())
						{
							out.push_back(i[key]);
							break;
						}
					}
				}
			}
		}
	}
	modelsOut = std::move(out);
	return true;
}

void AIChatClient::Worker(AIChatRequest req)
{
	try
	{
		if (req.kind == AIChatRequest::Models)
		{
			nlohmann::json models;
			std::string err;
			if (FetchModels(req, models, err))
			{
				AIEvent e;
				e.type = AIEvent::Models;
				e.extra = models;
				Push(e);
			}
			else
			{
				AIEvent e;
				e.type = AIEvent::Error;
				e.text = err;
				Push(e);
			}
		}
		else
		{
			const bool isAnthropic = (req.provider == "anthropic");
			nlohmann::json messages = req.messages;
			std::string system = req.systemPrompt;
			if (!req.contextText.empty())
				system = (system.empty() ? "" : system + "\n\n") +
					"Live Pyros3D editor state (act on it with the tools):\n" + req.contextText;
			if (req.useTools)
				// Full tool reference (auto-generated from the tool table),
				// so the model knows exactly what it can do.
				system = (system.empty() ? "" : system + "\n\n") + BuildToolReference();
			const nlohmann::json tools = req.useTools
				? (isAnthropic ? BuildAnthropicTools() : BuildOpenAITools())
				: nlohmann::json();

			for (int round = 0; round < 8; ++round)
			{
				if (cancel_.load())
					break;

				std::string text;
				std::vector<ToolCall> calls;
				std::string err;
				if (!DoRound(req, isAnthropic, messages, system, tools, text, calls, err))
				{
					AIEvent e;
					e.type = AIEvent::Error;
					e.text = err.empty() ? "request failed" : err;
					Push(e);
					break;
				}
				if (calls.empty())
					break;

				for (const auto& c : calls)
				{
					AIEvent e;
					e.type = AIEvent::Tool;
					e.name = c.name;
					e.extra = c.args;
					Push(e);
				}

				struct Res { std::string name; bool ok; std::string text; };
				std::vector<Res> results;
				for (const auto& c : calls)
				{
					if (cancel_.load())
						break; // Stop pressed - don't execute any more tools
					AIToolRequest tr;
					tr.name = c.name;
					tr.args = c.args;
					bool ok = true;
					std::string result;
					{
						std::unique_lock<std::mutex> lk(toolMutex_);
						// C++17: deque::emplace_back returns a reference (P0448).
						AIToolRequest& slot = toolQueue_.emplace_back(std::move(tr));
						AIToolRequest* slotPtr = &slot; // deque element addresses are stable
						toolCV_.wait_for(lk, std::chrono::seconds(60),
							[slotPtr, this]() { return slotPtr->done || cancel_.load(); });
						if (!slot.done)
						{
							ok = false;
							result = cancel_.load()
								? "stopped by user"
								: "tool execution timed out (main thread busy)";
						}
						else if (slot.failed)
						{
							ok = false;
							result = slot.error;
						}
						else
						{
							// A screenshot is ~100 KB of base64 that the
							// Truncate() below would cut into a meaningless
							// fragment before any provider saw a whole image.
							// Acknowledge the capture and drop the payload
							// rather than spending the whole tool budget on
							// half a data URL.
							nlohmann::json shown = slot.result;
							if (shown.is_object() && shown.contains("pngBase64"))
							{
								const size_t chars = shown["pngBase64"].is_string()
									? shown["pngBase64"].get<std::string>().size() : 0;
								shown.erase("pngBase64");
								shown["png"] = "captured, " + std::to_string(chars)
									+ " base64 chars (shown in the editor, not sent)";
							}
							result = shown.is_null() ? std::string() : shown.dump();
						}
						for (auto it = toolQueue_.begin(); it != toolQueue_.end(); ++it)
							if (&*it == slotPtr) { toolQueue_.erase(it); break; }
					}
					result = Truncate(result, 8000);
					AIEvent e;
					e.type = AIEvent::ToolResult;
					e.name = c.name;
					e.ok = ok;
					e.text = result;
					Push(e);
					results.push_back({ c.name, ok, result });
				}

				if (isAnthropic)
				{
					nlohmann::json::array_t content;
					if (!text.empty())
						content.push_back({ { "type", "text" }, { "text", text } });
					for (const auto& c : calls)
						content.push_back({
							{ "type", "tool_use" },
							{ "id", c.id },
							{ "name", c.name },
							{ "input", c.args },
						});
					nlohmann::json m;
					m["role"] = "assistant";
					m["content"] = std::move(content);
					messages.push_back(std::move(m));

					nlohmann::json::array_t trs;
					for (size_t i = 0; i < results.size(); ++i)
						trs.push_back({
							{ "type", "tool_result" },
							{ "tool_use_id", calls[i].id },
							{ "content", results[i].text },
						});
					nlohmann::json m2;
					m2["role"] = "user";
					m2["content"] = std::move(trs);
					messages.push_back(std::move(m2));
				}
				else
				{
					nlohmann::json::array_t tcs;
					for (const auto& c : calls)
					{
						nlohmann::json tc;
						tc["id"] = c.id;
						tc["type"] = "function";
						tc["function"] = {
							{ "name", c.name },
							{ "arguments", nlohmann::json(c.args).dump() },
						};
						tcs.push_back(std::move(tc));
					}
					nlohmann::json m;
					m["role"] = "assistant";
					m["content"] = text.empty() ? nlohmann::json(nullptr) : nlohmann::json(text);
					m["tool_calls"] = std::move(tcs);
					messages.push_back(std::move(m));

					for (size_t i = 0; i < results.size(); ++i)
					{
						nlohmann::json tm;
						tm["role"] = "tool";
						tm["tool_call_id"] = calls[i].id;
						tm["content"] = results[i].text;
						messages.push_back(std::move(tm));
					}
				}
			}
		}
	}
	catch (const std::exception& e)
	{
		AIEvent ev;
		ev.type = AIEvent::Error;
		ev.text = e.what();
		Push(ev);
	}
	catch (...)
	{
		AIEvent ev;
		ev.type = AIEvent::Error;
		ev.text = "unknown error";
		Push(ev);
	}

	AIEvent done;
	done.type = AIEvent::Done;
	Push(done);
	busy_ = false;
}

//=============================================================================
// AIAssistantTab
//=============================================================================

AIAssistantTab::AIAssistantTab(bool* open)
	: Open(open), Name("AI Assistant"), statusText_("open a project to chat")
{
	baseUrl = kAIProviders[providerIndex].defaultBase;
}

void AIAssistantTab::SetProjectOpen(bool open)
{
	if (projectOpen_ == open)
		return;
	projectOpen_ = open;
	if (!open)
	{
		modelList_.clear();
		statusText_ = "open a project to chat";
	}
	else if (statusText_ == "open a project to chat")
		statusText_ = "ready";
}

void AIAssistantTab::LoadFrom(const nlohmann::json& j)
{
	if (!j.is_object())
		return;
	const std::string pid = j.value("provider", std::string());
	for (size_t i = 0; i < kAIProviderCount; ++i)
		if (kAIProviders[i].id == pid)
			providerIndex = (int)i;
	baseUrl = j.value("baseUrl", std::string());
	if (baseUrl.empty())
		baseUrl = kAIProviders[providerIndex].defaultBase;
	model = j.value("model", std::string());
	apiKey = j.value("apiKey", std::string());
	temperature = (float)j.value("temperature", 0.2f);
	maxTokens = j.value("maxTokens", 2048);
	useTools = j.value("useTools", true);
	includeContext = j.value("includeContext", true);
	showThinking = j.value("showThinking", true);
	systemPrompt = j.value("systemPrompt", std::string());
}

nlohmann::json AIAssistantTab::ToJson() const
{
	nlohmann::json j;
	j["provider"] = kAIProviders[providerIndex].id;
	j["baseUrl"] = baseUrl;
	j["model"] = model;
	j["apiKey"] = apiKey;
	j["temperature"] = temperature;
	j["maxTokens"] = maxTokens;
	j["useTools"] = useTools;
	j["includeContext"] = includeContext;
	j["showThinking"] = showThinking;
	j["systemPrompt"] = systemPrompt;
	return j;
}

bool AIAssistantTab::ConsumeDirtySettings()
{
	const bool dirty = settingsDirty;
	settingsDirty = false;
	return dirty;
}

AIAssistantTab::~AIAssistantTab() {}

void AIAssistantTab::Update(const f64 time)
{
	(void)time;
	client_.PumpToolRequests();
	DrainEvents();
	const bool busyNow = client_.Busy();
	bool idle = !busyNow;
	// Message queue: dispatch the next queued message now that the client
	// is idle. If another message follows immediately the run never goes
	// idle, so thinking/tool rows stay open for the whole chain.
	if (idle && !sendQueue_.empty())
	{
		std::string next = std::move(sendQueue_.front());
		sendQueue_.pop_front();
		DoSend(std::move(next));
		idle = false;
	}
	// Run just finished (transition tracked across frames - the worker can
	// complete between Update() calls): collapse the detail rows that were
	// held open while streaming.
	if (wasBusy_ && idle)
		collapseOnNextDraw_ = true;
	wasBusy_ = busyNow;
}

void AIAssistantTab::CancelQueued(size_t index)
{
	if (index >= sendQueue_.size())
		return;
	sendQueue_.erase(sendQueue_.begin() + (long)index);
	if (sendQueue_.empty())
		statusText_ = "ready";
}

void AIAssistantTab::Shutdown()
{
	client_.Stop();
}

void AIAssistantTab::DrainEvents()
{
	for (const AIEvent& e : client_.DrainEvents())
	{
		switch (e.type)
		{
		case AIEvent::Delta:
			streamText_ += e.text;
			break;

		case AIEvent::Reasoning:
		{
			if (showThinking)
				streamReasoning_ += e.text;
			break;
		}

		case AIEvent::Tool:
		{
			if (!streamReasoning_.empty())
			{
				messages_.push_back({ "thinking", streamReasoning_, "thinking" });
				streamReasoning_.clear();
			}
			if (!streamText_.empty())
			{
				messages_.push_back({ "assistant", streamText_ });
				streamText_.clear();
			}
			// Header stays one line ("→ add_primitive"); the arguments go in
			// the body, which is collapsed until the user asks for it.
			std::string args = e.extra.is_null() ? std::string("{}") : e.extra.dump(2);
			messages_.push_back({ "tool", Truncate(args, 4000), u8"\u2192 " + e.name });
			break;
		}

		case AIEvent::ToolResult:
		{
			// A result can be the entire scene graph. Keep a generous slice
			// for the user to expand into, but never put any of it in the
			// header - the model still receives the untruncated result, this
			// is purely what the transcript shows.
			const std::string mark = e.ok ? u8"\u2713 " : u8"\u2717 ";
			messages_.push_back({
				e.ok ? "tool" : "error",
				Truncate(e.text, 4000),
				mark + e.name + (e.ok ? "" : " failed"),
			});
			break;
		}

		case AIEvent::Usage:
		{
			if (e.extra.is_object())
			{
				auto pick = [&](const char* a, const char* b) -> int {
					if (e.extra.contains(a)) return e.extra.value(a, 0);
					if (e.extra.contains(b)) return e.extra.value(b, 0);
					return 0;
				};
				lastUsageIn = pick("prompt_tokens", "input_tokens");
				lastUsageOut = pick("completion_tokens", "output_tokens");
			}
			break;
		}

		case AIEvent::Error:
		{
			if (!streamReasoning_.empty())
			{
				messages_.push_back({ "thinking", streamReasoning_, "thinking" });
				streamReasoning_.clear();
			}
			if (!streamText_.empty())
			{
				messages_.push_back({ "assistant", streamText_ });
				streamText_.clear();
			}
			messages_.push_back({ "error", e.text, "error" });
			errorThisRun_ = true;
			modelsInFlight_ = false;
			statusText_ = "error: " + Truncate(e.text, 120);
			break;
		}

		case AIEvent::Models:
		{
			// modelsInFlight_ stays set: the trailing Done event must NOT
			// clobber this status, and it is the one that clears the flag.
			modelList_.clear();
			if (e.extra.is_array())
				for (const auto& m : e.extra)
					if (m.is_string())
						modelList_.push_back(m.get<std::string>());
			statusText_ = modelList_.empty() ? "no models found" : ("models: " + std::to_string(modelList_.size()) + " available");
			break;
		}

		case AIEvent::Done:
		{
			if (!streamReasoning_.empty())
			{
				messages_.push_back({ "thinking", streamReasoning_, "thinking" });
				streamReasoning_.clear();
			}
			if (!streamText_.empty())
			{
				messages_.push_back({ "assistant", streamText_ });
				streamText_.clear();
			}
			if (modelsInFlight_)
			{
				// Models request finished - the Models event already set the
				// status ("models: N available" / "no models found").
				modelsInFlight_ = false;
			}
			else if (errorThisRun_)
			{
				// Keep the "error: ..." status set by the Error event.
			}
			else if (lastUsageIn > 0 || lastUsageOut > 0)
				statusText_ = std::to_string(lastUsageIn) + " in / " + std::to_string(lastUsageOut) + " out tokens";
			else
				statusText_ = "done";
			errorThisRun_ = false;
			break;
		}
		}
	}
}

void AIAssistantTab::Configure(const std::string& providerId, const std::string& baseUrl,
	const std::string& model, const std::string& apiKey, bool useTools)
{
	for (size_t i = 0; i < kAIProviderCount; ++i)
		if (kAIProviders[i].id == providerId)
			providerIndex = (int)i;
	this->baseUrl = baseUrl.empty() ? kAIProviders[providerIndex].defaultBase : baseUrl;
	this->model = model;
	this->apiKey = apiKey;
	this->useTools = useTools;
}

void AIAssistantTab::Send(const std::string& overrideText)
{
	std::string userText = overrideText.empty() ? inputBuf_ : overrideText;
	while (!userText.empty() && std::isspace((unsigned char)userText.back()))
		userText.pop_back();
	if (userText.empty())
		return;
	if (client_.Busy())
	{
		// Message queue: held until the current run finishes, then sent
		// automatically by Update().
		inputBuf_.clear();
		sendQueue_.push_back(std::move(userText));
		statusText_ = std::to_string(sendQueue_.size()) + " queued - will send after the current response";
		return;
	}
	inputBuf_.clear();
	DoSend(std::move(userText));
}

void AIAssistantTab::DoSend(std::string userText)
{
	if (!projectOpen_)
	{
		statusText_ = "open a project to chat";
		return;
	}
	modelsInFlight_ = false; // a chat run is starting (not a models fetch)
	errorThisRun_ = false;
	if (model.empty())
	{
		// An empty model id makes most servers answer 400 - fail with a
		// clear message instead (or auto-pick when a list is loaded).
		if (!modelList_.empty())
			model = modelList_[0];
		else
		{
			statusText_ = "no model set - Refresh the model list or type a model id below";
			return;
		}
	}

	AIChatRequest req;
	req.kind = AIChatRequest::Chat;
	const AIProviderPreset& p = kAIProviders[providerIndex];
	req.provider = p.id;
	req.baseUrl = baseUrl.empty() ? p.defaultBase : baseUrl;
	req.model = model;
	req.apiKey = apiKey;
	req.temperature = temperature;
	req.maxTokens = maxTokens;
	req.useTools = useTools;
	req.systemPrompt = systemPrompt;
	if (includeContext && contextProvider_)
		req.contextText = contextProvider_();

	nlohmann::json::array_t msgs;
	for (const auto& m : messages_)
	{
		// Only user/assistant are valid wire roles. Transcript entries like
		// "thinking"/"tool"/"error" are display-only - replaying them is
		// what made the provider 400 on every message after the first.
		if (m.role != "user" && m.role != "assistant")
			continue;
		msgs.push_back({ { "role", m.role }, { "content", m.text } });
	}
	msgs.push_back({ { "role", "user" }, { "content", userText } });
	req.messages = std::move(msgs);

	messages_.push_back({ "user", userText });
	streamText_.clear();
	statusText_ = "sending…";
	if (!client_.Start(req))
		statusText_ = "busy";
}

void AIAssistantTab::RefreshModels()
{
	if (client_.Busy())
		return;
	if (!projectOpen_)
		return;
	AIChatRequest req;
	req.kind = AIChatRequest::Models;
	const AIProviderPreset& p = kAIProviders[providerIndex];
	req.provider = p.id;
	req.baseUrl = baseUrl.empty() ? p.defaultBase : baseUrl;
	req.apiKey = apiKey;
	modelList_.clear();
	modelsInFlight_ = true;
	statusText_ = "loading models…";
	client_.Start(req);
}

void AIAssistantTab::ClearChat()
{
	messages_.clear();
	sendQueue_.clear();
	streamText_.clear();
	streamReasoning_.clear();
	inputBuf_.clear();
	lastUsageIn = lastUsageOut = 0;
	statusText_ = "ready";
}

void AIAssistantTab::Show()
{
	if (!Open || !*Open)
		return;

	if (ImGui::Begin(Name.c_str(), Open))
	{
		const bool busy = client_.Busy();

		// ------------------------------------------------------------------
		// Toolbar. Everything that is not the conversation lives on this one
		// row or behind its Settings popup. Provider/key/temperature/system
		// prompt used to sit in two always-present CollapsingHeaders above
		// the transcript, which is configuration you touch once taking up
		// the top third of a panel you use constantly.
		// ------------------------------------------------------------------
		// The toolbar measures itself and wraps rather than overlapping: this
		// panel is routinely docked into a column barely wider than a button,
		// and a fixed one-line layout there just draws the right-hand
		// controls on top of the left-hand ones. Wide enough for one line,
		// it uses one; otherwise the small fixed controls keep the first row
		// and the model field - the one part that needs room to be readable -
		// gets a full-width row of its own.
		const ImGuiStyle& st = ImGui::GetStyle();
		auto btnW = [&](const char* label) {
			return ImGui::CalcTextSize(label).x + st.FramePadding.x * 2.f;
		};
		const float settingsW = btnW("Settings");
		const float refreshW = btnW("Refresh");
		const float clearW = btnW("Clear");
		const float toolsW = ImGui::CalcTextSize("Tools").x + st.ItemInnerSpacing.x + ImGui::GetFrameHeight();
		const float sp = st.ItemSpacing.x;
		const float avail = ImGui::GetContentRegionAvail().x;
		const float kMinModelW = 140.f;
		const bool oneRow = avail >= settingsW + toolsW + clearW + refreshW + kMinModelW + sp * 4.f;

		auto drawModelField = [&](float width) {
			ImGui::SetNextItemWidth(width);
			if (!modelList_.empty())
			{
				std::string mlabels;
				for (size_t i = 0; i < modelList_.size(); ++i)
				{
					if (i) mlabels.push_back('\0');
					mlabels += modelList_[i];
				}
				int msel = 0;
				for (size_t i = 0; i < modelList_.size(); ++i)
					if (modelList_[i] == model) { msel = (int)i; break; }
				if (ImGui::Combo("##aimodel", &msel, mlabels.c_str()))
				{
					model = modelList_[msel];
					settingsDirty = true;
				}
			}
			else if (ImGui::InputTextWithHint("##aimodel", "model id", &model))
				settingsDirty = true;
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip("Model: %s", model.empty() ? "(none set)" : model.c_str());
		};

		if (ImGui::Button("Settings"))
			openSettingsPopup_ = true;
		if (openSettingsPopup_)
		{
			ImGui::OpenPopup("##aisettings");
			openSettingsPopup_ = false;
		}
		DrawSettingsPopup();

		ImGui::SameLine();
		if (ImGui::Checkbox("Tools", &useTools))
			settingsDirty = true;
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Let the assistant edit the project through the editor's tools");

		ImGui::SameLine();
		if (ImGui::Button("Clear"))
			ClearChat();
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Clear the conversation");

		if (oneRow)
		{
			ImGui::SameLine();
			drawModelField(avail - settingsW - toolsW - clearW - refreshW - sp * 4.f);
		}
		else
		{
			drawModelField(ImGui::GetContentRegionAvail().x - refreshW - sp);
		}
		ImGui::SameLine();
		if (ImGui::Button("Refresh"))
			RefreshModels();
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Refresh the provider's model list");

		// Status line: one row, and the token counters go here rather than
		// nowhere (they were tracked and never shown).
		ImGui::TextColored(busy ? ImVec4(0.6f, 0.8f, 1.0f, 1.0f) : ImVec4(0.62f, 0.62f, 0.68f, 1.0f),
			"%s", statusText_.c_str());
		if (lastUsageIn > 0 || lastUsageOut > 0)
		{
			ImGui::SameLine();
			ImGui::TextDisabled("  %d in / %d out", lastUsageIn, lastUsageOut);
		}
		ImGui::Separator();

		// ------------------------------------------------------------------
		// Transcript. Takes every pixel the input row does not.
		// ------------------------------------------------------------------
		const float inputH = 56.f;
		const float bottomH = inputH + ImGui::GetTextLineHeightWithSpacing() + ImGui::GetStyle().ItemSpacing.y * 2.f;
		ImGui::BeginChild("##aichat", ImVec2(0.0f, -bottomH), true);
		static const ImVec4 kUserCol(0.55f, 0.75f, 1.00f, 1.0f);
		static const ImVec4 kAsstCol(0.85f, 0.85f, 0.88f, 1.0f);
		static const ImVec4 kErrCol(1.00f, 0.55f, 0.55f, 1.0f);
		static const ImVec4 kThinkCol(0.55f, 0.55f, 0.60f, 1.0f);

		// A detail row is a header the user can open, never something that
		// opens itself. Auto-expanding every tool call while a run was in
		// flight is what buried the conversation under argument and result
		// dumps; live progress is covered by the status line and by the
		// streaming blocks at the end of the transcript instead.
		auto renderDetail = [&](size_t key, const char* label, const char* body, const ImVec4& col)
		{
			const bool wasOpen = expandedDetails_.count(key) != 0;
			ImGui::PushStyleColor(ImGuiCol_Text, col);
			ImGui::SetNextItemOpen(wasOpen);
			const bool open = ImGui::TreeNodeEx("##detail", ImGuiTreeNodeFlags_SpanAvailWidth, "%s", label);
			ImGui::PopStyleColor();
			if (open != wasOpen)
			{
				if (open) expandedDetails_.insert(key);
				else expandedDetails_.erase(key);
			}
			if (open)
			{
				// Bounded and scrollable: a tool result is routinely longer
				// than the whole panel is tall.
				const float maxH = ImGui::GetTextLineHeightWithSpacing() * 12.f;
				ImGui::PushStyleColor(ImGuiCol_Text, kThinkCol);
				ImGui::BeginChild("##detailbody", ImVec2(0.f, maxH), true,
					ImGuiWindowFlags_HorizontalScrollbar);
				ImGui::TextWrapped("%s", body[0] ? body : "(empty)");
				ImGui::EndChild();
				ImGui::PopStyleColor();
				ImGui::TreePop();
			}
		};

		for (size_t i = 0; i < messages_.size(); ++i)
		{
			const AIChatMessage& m = messages_[i];
			if (m.role == "user" || m.role == "assistant")
			{
				ImGui::TextColored(m.role == "user" ? kUserCol : kAsstCol, "%s", m.role.c_str());
				ImGui::TextWrapped("%s", m.text.empty() ? "(empty)" : m.text.c_str());
				ImGui::Spacing();
				continue;
			}
			ImGui::PushID((int)i);
			const std::string label = m.title.empty() ? m.role : m.title;
			renderDetail(i, label.c_str(), m.text.c_str(), m.role == "error" ? kErrCol : kThinkCol);
			ImGui::PopID();
		}

		// Live blocks: the one place something opens itself, because it is
		// the run currently happening.
		if (busy && !streamReasoning_.empty())
		{
			ImGui::PushStyleColor(ImGuiCol_Text, kThinkCol);
			ImGui::SetNextItemOpen(true);
			const bool open = ImGui::TreeNodeEx("##thinkinglive", 0, "thinking…");
			ImGui::PopStyleColor();
			if (open)
			{
				ImGui::PushStyleColor(ImGuiCol_Text, kThinkCol);
				ImGui::TextWrapped("%s", streamReasoning_.c_str());
				ImGui::PopStyleColor();
				ImGui::TreePop();
			}
		}
		if (busy && !streamText_.empty())
		{
			ImGui::TextColored(kAsstCol, "assistant");
			ImGui::TextWrapped("%s", streamText_.c_str());
		}
		if (busy && streamText_.empty() && messages_.empty())
			ImGui::TextDisabled("waiting for the first token…");

		// Queued messages: visible, each with a small "x" to cancel it.
		for (size_t i = 0; i < sendQueue_.size(); ++i)
		{
			ImGui::TextDisabled("queued: %s", Truncate(sendQueue_[i], 100).c_str());
			ImGui::SameLine(ImGui::GetContentRegionAvail().x);
			char xid[48];
			snprintf(xid, sizeof(xid), "x##qcancel%zu", (size_t)i);
			if (ImGui::SmallButton(xid))
			{
				CancelQueued(i);
				break; // indices shift after the erase
			}
		}
		if (autoScroll_ && ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 1.0f)
			ImGui::SetScrollHereY(1.0f);
		ImGui::EndChild();

		// ------------------------------------------------------------------
		// Input row, pinned to the bottom: text on the left, one action
		// button on the right that is Send or Stop depending on the state.
		// ------------------------------------------------------------------
		const float sendW = 70.f;
		const float frameH = ImGui::GetFrameHeight();
		const float yTop = ImGui::GetCursorPosY();
		ImGui::InputTextMultiline("##aiinput", &inputBuf_, ImVec2(-1 - sendW, inputH),
			ImGuiInputTextFlags_CtrlEnterForNewLine);
		ImGui::SameLine();
		ImGui::SetCursorPosY(yTop + inputH - frameH);
		if (busy)
		{
			if (ImGui::Button("Stop", ImVec2(sendW, 0)))
				client_.Stop();
		}
		else if (ImGui::Button("Send", ImVec2(sendW, 0)))
			Send();

		const bool shift = ImGui::IsKeyDown(ImGuiKey_LeftShift) || ImGui::IsKeyDown(ImGuiKey_RightShift);
		const bool enter = ImGui::IsKeyPressed(ImGuiKey_Enter, false) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter, false);
		if (enter && !shift)
			Send();

		ImGui::TextDisabled(busy ? "Enter to send (queues while busy) · Shift+Enter for a new line"
		                         : "Enter to send · Shift+Enter for a new line");
	}
	ImGui::End();
}

void AIAssistantTab::DrawSettingsPopup()
{
	ImGui::SetNextWindowSize(ImVec2(460.f, 0.f), ImGuiCond_Appearing);
	if (!ImGui::BeginPopup("##aisettings"))
		return;

	// push_back('\0'): operator+=(const char*) would append strlen("\0")==0 chars.
	std::string labels;
	for (size_t i = 0; i < kAIProviderCount; ++i)
	{
		if (i) labels.push_back('\0');
		labels += kAIProviders[i].label;
	}
	int idx = providerIndex;
	if (ImGui::Combo("Provider", &idx, labels.c_str()))
	{
		const std::string prevDefault = kAIProviders[providerIndex].defaultBase;
		providerIndex = idx;
		if (baseUrl.empty() || baseUrl == prevDefault)
			baseUrl = kAIProviders[providerIndex].defaultBase;
		modelList_.clear();
		settingsDirty = true;
		// A provider was (re)selected and it has a base URL - try to list
		// its models. Skipped while a chat is in flight.
		if (!baseUrl.empty() && !client_.Busy())
			RefreshModels();
	}
	if (ImGui::InputText("Base URL", &baseUrl))
		settingsDirty = true;
	if (ImGui::InputTextWithHint("API Key", "sk-… (optional for local providers)", &apiKey, ImGuiInputTextFlags_Password))
		settingsDirty = true;

	ImGui::Separator();
	ImGui::SliderFloat("Temperature", &temperature, 0.f, 2.f, "%.2f");
	if (ImGui::DragInt("Max Tokens", &maxTokens, 64, 256, 32768))
		settingsDirty = true;
	if (ImGui::Checkbox("Tools (let the AI edit the project)", &useTools))
		settingsDirty = true;
	if (ImGui::Checkbox("Send project context", &includeContext))
		settingsDirty = true;
	if (ImGui::Checkbox("Show thinking (reasoning) text", &showThinking))
		settingsDirty = true;
	if (ImGui::Checkbox("Follow the newest message", &autoScroll_))
		settingsDirty = true;

	ImGui::Separator();
	if (ImGui::CollapsingHeader("System Prompt"))
	{
		ImGui::TextDisabled("Appended to the built-in tool reference.");
		if (ImGui::InputTextMultiline("##aisysprompt", &systemPrompt, ImVec2(-1, 90)))
			settingsDirty = true;
	}

	ImGui::EndPopup();
}
