//============================================================================
// Name        : AgentServer.h
// Description : Local TCP command server that lets external agents drive a
//               running PyrosBuilder editor instance.
//
//               - Binds 127.0.0.1 only (loopback), OS-assigned port.
//               - Protocol: newline-delimited JSON.
//                 Request : {"id":1,"token":"<t>","cmd":"add_object","args":{...}}
//                 Response: {"id":1,"ok":true,"result":{...}}
//                           {"id":1,"ok":false,"error":"..."}
//               - A listener thread accepts connections; Process() is called
//                 on the editor's main thread once per frame. It reads
//                 commands, executes the bound handler (main thread, so the
//                 SceneGraph and renderer are safe to touch) and sends the
//                 responses back.
//               - Writes a discovery file (port + token + pid) to the system
//                 temp dir so out-of-process clients (the MCP bridge) can
//                 find a running editor.
//
//               Cross-platform: POSIX sockets and Winsock2 (named in the
//               same code path).
//============================================================================

#ifndef AGENT_SERVER_H
#define AGENT_SERVER_H

#include <Pyros3D/Utils/Json/json.hpp>

#include <atomic>
#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

class AgentServer {
public:

	using Handler = std::function<nlohmann::json(const nlohmann::json& cmd)>;

	AgentServer();
	~AgentServer();

	// Creates the socket and starts the listener thread. Returns false if
	// the socket could not be created or bound.
	bool Start(const Handler& handler);
	void Stop();

	bool IsRunning() const { return running_.load(); }
	uint16_t GetPort() const { return port_; }
	const std::string &GetToken() const { return token_; }

	// Called on the editor's main thread, once per frame.
	void Process();

	// <tempdir>/pyros3d-editor.json
	static std::string DiscoveryFilePath();

	// Utility: base64-encode `len` bytes (used to ship screenshots over the
	// JSON protocol).
	static std::string Base64Encode(const unsigned char* data, size_t len);

private:

	// intptr_t (not long): on 64-bit Windows a SOCKET is a 64-bit UINT_PTR
	// while `long` is only 32 bits (LLP64) — storing a SOCKET in a `long`
	// truncates the handle. intptr_t is pointer-width on every target ABI
	// we build for (LP64 and LLP64 alike), so it round-trips both a POSIX
	// int fd and a Win64 SOCKET without loss.
	struct Client {
		intptr_t fd = -1;
		std::string buffer;
	};

	struct PendingCommand {
		uint64_t id = 0;
		std::string cmd;
		nlohmann::json args;
		intptr_t fd = -1;
	};

	void AcceptLoop();
	void WriteDiscovery();
	static void DeleteDiscovery();
	static bool SendAll(intptr_t fd, const std::string& data);

	std::atomic<bool> running_{false};
	std::thread acceptThread_;
	intptr_t listenFd_ = -1;
	uint16_t port_ = 0;
	std::string token_;
	Handler handler_;

	std::mutex clientsMutex_;
	std::vector<Client> clients_;

	std::mutex queueMutex_;
	std::vector<PendingCommand> queue_;
};

#endif /* AGENT_SERVER_H */
