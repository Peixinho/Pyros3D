//=============================================================================
// Name        : AIAssistant.h
// Description : AI Assistant panel - chats with LLM providers (OpenAI-
//               compatible and Anthropic) over HTTPS (libcurl, worker
//               thread) and executes the model's tool calls in-editor
//               through Editor::HandleAgentCommand, the same dispatcher
//               the MCP bridge drives.
//=============================================================================

#ifndef AIASSISTANT_H
#define	AIASSISTANT_H

#include "editor/UI/IUInterface.h"
#include <misc/cpp/imgui_stdlib.h>	// std::string overloads of InputText*
#include <Pyros3D/Utils/Json/json.hpp>

#include <atomic>
#include <condition_variable>
#include <deque>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <set>
#include <vector>

struct AIProviderPreset {
	std::string id;
	std::string label;
	std::string defaultBase;
	bool needsKey;
};

extern const AIProviderPreset kAIProviders[];
extern const size_t kAIProviderCount;

struct AIChatMessage {
	std::string role;
	std::string text;
	// One-line label for a collapsed detail row (thinking / tool / error).
	// Empty for plain user/assistant turns, which are never collapsed.
	// Kept separate from `text` so the row header can stay short no matter
	// how big the body is - a tool result used to be spliced into its own
	// header, so a single scene_state call pushed the conversation off
	// screen twice over.
	std::string title;
};

// One event produced by the worker thread, consumed by the UI thread.
struct AIEvent {
	enum Type { Delta, Reasoning, Tool, ToolResult, Usage, Error, Done, Models };
	Type type = Type::Delta;
	std::string text;
	std::string name;
	bool ok = true;
	nlohmann::json extra;
};

struct AIChatRequest {
	enum Kind { Chat, Models };
	Kind kind = Kind::Chat;
	std::string provider;
	std::string baseUrl;
	std::string model;
	std::string apiKey;
	std::string systemPrompt;
	std::string contextText;
	nlohmann::json messages;
	float temperature = 0.2f;
	int maxTokens = 2048;
	bool useTools = true;
};

// One tool call the provider asked for (id/name/args), accumulated from
// the streamed deltas.
struct ToolCall {
	std::string id;
	std::string name;
	std::string argsRaw;
	nlohmann::json args = nlohmann::json::object();
};

// A tool call the worker thread wants executed. Posted to the queue by the
// worker, run on the editor's main thread (HandleAgentCommand touches the
// SceneGraph), then answered here.
struct AIToolRequest {
	std::string id;
	std::string name;
	nlohmann::json args;
	bool done = false;
	bool failed = false;
	nlohmann::json result;
	std::string error;
};

class AIChatClient {
public:

	// Runs one tool call. Must execute on the editor's main thread.
	using ToolExecutor = std::function<nlohmann::json(const std::string& name, const nlohmann::json& args)>;

	AIChatClient();
	~AIChatClient();
	AIChatClient(const AIChatClient&) = delete;
	AIChatClient& operator=(const AIChatClient&) = delete;

	void SetToolExecutor(ToolExecutor fn) { toolExecutor_ = std::move(fn); }

	bool Busy() const { return busy_; }
	bool ShouldCancel() const { return cancel_.load(); }
	// Spawns the worker thread. Returns false if one is already running.
	bool Start(const AIChatRequest& req);
	// Asks the worker to abort (curl transfer is cancelled on the next tick).
	void Stop();
	// UI thread only.
	std::vector<AIEvent> DrainEvents();
	// UI thread: executes every queued tool call (main thread - safe).
	void PumpToolRequests();
	// Also called from the curl write callback (worker thread) to stream
	// events into the queue - thread-safe.
	void Push(AIEvent e);

private:

	void Worker(AIChatRequest req);
	// One streaming exchange with the provider; parses the SSE stream into
	// text (pushed live as Delta events) + accumulated tool calls.
	bool DoRound(const AIChatRequest& req, bool isAnthropic,
		const nlohmann::json& messages, const std::string& system,
		const nlohmann::json& tools, std::string& textOut,
		std::vector<ToolCall>& callsOut, std::string& errOut);
	bool FetchModels(const AIChatRequest& req, nlohmann::json& modelsOut, std::string& errOut);

	std::atomic<bool> busy_{false};
	std::atomic<bool> cancel_{false};
	std::thread worker_;

	std::mutex queueMutex_;
	std::vector<AIEvent> queue_;

	std::mutex toolMutex_;
	std::condition_variable toolCV_;
	std::deque<AIToolRequest> toolQueue_;

	ToolExecutor toolExecutor_;
};

class AIAssistantTab : public IUInterface {
public:

	explicit AIAssistantTab(bool* open);
	virtual ~AIAssistantTab();

	virtual void Init(const uint32 width, const uint32 height) {}
	virtual void OnResize(const uint32 width, const uint32 height) {}
	virtual void Update(const f64 time);
	virtual void Shutdown();
	virtual void Show();

	// Returns a short text summary of the current project/scene state,
	// sent as context when the user opts in. May return "".
	void SetContextProvider(std::function<std::string()> fn) { contextProvider_ = std::move(fn); }
	void SetToolExecutor(AIChatClient::ToolExecutor fn) { client_.SetToolExecutor(std::move(fn)); }

	// Programmatic configuration (same fields as the Settings section).
	void Configure(const std::string& providerId, const std::string& baseUrl,
		const std::string& model, const std::string& apiKey = "", bool useTools = false);
	// Settings live in the open project (project.json -> settings.aiAssistant).
	// Chat is disabled while no project is open.
	void SetProjectOpen(bool open);
	bool ProjectOpen() const { return projectOpen_; }
	void LoadFrom(const nlohmann::json& j);
	nlohmann::json ToJson() const;
	// True once (then clears) if the user changed settings since last call.
	bool ConsumeDirtySettings();
	// text == "" sends the input box content; otherwise sends text directly
	// (used by automation/tests). While a run is in flight the message is
	// queued and sent automatically as soon as the current one finishes.
	void Send(const std::string& text = "");
	// Starts a run unconditionally (no busy check) - used for queued sends.
	void DoSend(std::string userText);
	// Message queue (messages sent while a run is in flight).
	size_t QueuedCount() const { return sendQueue_.size(); }
	const std::deque<std::string>& GetQueue() const { return sendQueue_; }
	// Removes the queued message at index (visible in the transcript, each
	// with a cancel button).
	void CancelQueued(size_t index);
	// Aborts the in-flight run (streaming is cancelled on the next tick).
	void Stop() { client_.Stop(); }
	// Fetches the provider's model list into the Model combo (async).
	void RefreshModels();
	bool Busy() const { return client_.Busy(); }
	const std::vector<AIChatMessage>& GetMessages() const { return messages_; }
	const std::string& GetStatus() const { return statusText_; }
	const std::vector<std::string>& GetModels() const { return modelList_; }

private:

	void ClearChat();
	void DrainEvents();
	// Provider/key/sampling/system prompt, shown from the toolbar's gear.
	void DrawSettingsPopup();

	bool* Open;
	std::string Name;
	bool projectOpen_ = false;
	// Messages sent while a run is in flight; dispatched by Update() as
	// soon as the client goes idle.
	std::deque<std::string> sendQueue_;
	// Set when a run just ended: thinking/tool detail rows were held open
	// while streaming and should collapse on the next draw.
	bool collapseOnNextDraw_ = false;
	// Previous frame's busy state - the worker can finish BETWEEN frames,
	// so the busy->idle transition must be tracked across Update() calls.
	bool wasBusy_ = false;

	// Settings (persisted to ai_assistant.json next to recent_projects.txt)
	int providerIndex = 0;
	std::string baseUrl;
	std::string model;
	std::string apiKey;
	float temperature = 0.2f;
	int maxTokens = 2048;
	bool useTools = true;
	bool includeContext = true;
	bool showThinking = true;
	std::string systemPrompt;
	bool settingsOpen = true;
	// "Advanced" section (system prompt), collapsed by default.
	bool advancedOpen = false;
	bool settingsDirty = false;
	// Settings live in a popup off the toolbar rather than inline above the
	// transcript, where they permanently ate the top third of the panel.
	bool openSettingsPopup_ = false;
	// Expanded detail rows the user opened by hand, keyed by message index.
	// Rows are collapsed by default now (see Show()) - without remembering
	// this, anything the user opened snapped shut on the next redraw.
	std::set<size_t> expandedDetails_;
	std::vector<std::string> modelList_;

	// Chat state
	std::vector<AIChatMessage> messages_;
	std::string inputBuf_;
	bool streaming_ = false;
	// The worker pushes a Done event for EVERY request (chat and models) -
	// these flags keep Done from clobbering a models/error status.
	bool modelsInFlight_ = false;
	bool errorThisRun_ = false;
	std::string streamText_;
	// reasoning_content streamed by thinking models, kept separate so it
	// doesn't pollute the answer text (and can be hidden).
	std::string streamReasoning_;
	std::string statusText_;
	int lastUsageIn = 0;
	int lastUsageOut = 0;
	bool autoScroll_ = true;

	AIChatClient client_;
	std::function<std::string()> contextProvider_;
};

#endif	/* AIASSISTANT_H */
