//============================================================================
// Name        : AgentServer.cpp
// Description : Implementation of the local TCP command server (see .h).
//============================================================================

#include "AgentServer.h"

#include <Pyros3D/Core/Logs/Log.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <random>
#include <sstream>

#ifdef _WIN32
	#include <winsock2.h>
	#include <ws2tcpip.h>
	#include <process.h>
	#pragma comment(lib, "ws2_32.lib")
	typedef SOCKET sock_t;
	#define AGENT_INVALID_SOCKET INVALID_SOCKET
	#define AGENT_CLOSESOCKET(s) closesocket(s)
	#define AGENT_EWOULDBLOCK WSAEWOULDBLOCK
	#define AGENT_POLLWSAPOLL(p, n, timeout) WSAPoll(p, n, timeout)
	static sock_t g_sockInvalid = INVALID_SOCKET;
#else
	#include <arpa/inet.h>
	#include <cerrno>
	#include <fcntl.h>
	#include <netinet/in.h>
	#include <poll.h>
	#include <sys/socket.h>
	#include <sys/stat.h>
	#include <unistd.h>
	typedef int sock_t;
	#define AGENT_INVALID_SOCKET (-1)
	#define AGENT_CLOSESOCKET(s) ::close(s)
	#define AGENT_EWOULDBLOCK (EWOULDBLOCK)
	#define AGENT_POLLWSAPOLL(p, n, timeout) ::poll(p, n, timeout)
	static sock_t g_sockInvalid = -1;
#endif

using json = nlohmann::json;

namespace {

	bool g_wsaInitDone = false;

	bool SocketInit()
	{
#ifdef _WIN32
		if (!g_wsaInitDone)
		{
			WSADATA wsa;
			if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
				return false;
			g_wsaInitDone = true;
		}
#endif
		return true;
	}

	bool SetNonblocking(sock_t fd)
	{
#ifdef _WIN32
		u_long mode = 1;
		return ioctlsocket(fd, FIONBIO, &mode) == 0;
#else
		const int flags = fcntl(fd, F_GETFL, 0);
		if (flags < 0) return false;
		return fcntl(fd, F_SETFL, flags | O_NONBLOCK) == 0;
#endif
	}

	bool FdReadable(sock_t fd, int timeoutMs)
	{
#ifdef _WIN32
		WSAPOLLFD pfd;
		pfd.fd = fd;
		pfd.events = POLLRDNORM;
		pfd.revents = 0;
		return AGENT_POLLWSAPOLL(&pfd, 1, timeoutMs) > 0;
#else
		struct pollfd pfd;
		pfd.fd = fd;
		pfd.events = POLLIN;
		pfd.revents = 0;
		return AGENT_POLLWSAPOLL(&pfd, 1, timeoutMs) > 0;
#endif
	}

	bool FdWritable(sock_t fd, int timeoutMs)
	{
#ifdef _WIN32
		WSAPOLLFD pfd;
		pfd.fd = fd;
		pfd.events = POLLOUT;
		pfd.revents = 0;
		return AGENT_POLLWSAPOLL(&pfd, 1, timeoutMs) > 0;
#else
		struct pollfd pfd;
		pfd.fd = fd;
		pfd.events = POLLOUT;
		pfd.revents = 0;
		return AGENT_POLLWSAPOLL(&pfd, 1, timeoutMs) > 0;
#endif
	}

} // namespace

AgentServer::AgentServer() {}

AgentServer::~AgentServer()
{
	Stop();
}

std::string AgentServer::DiscoveryFilePath()
{
#ifdef _WIN32
	const char* temp = std::getenv("TEMP");
	if (!temp) temp = std::getenv("TMP");
	if (!temp) temp = ".";
#else
	const char* temp = std::getenv("TMPDIR");
	if (!temp || !*temp) temp = "/tmp";
#endif
	return std::string(temp) + "/pyros3d-editor.json";
}

void AgentServer::DeleteDiscovery()
{
	// std::remove() works on both POSIX and Windows (no need for the
	// Win32-only DeleteFileA, which also isn't declared by the
	// winsock2.h/ws2tcpip.h headers this file already includes).
	std::remove(DiscoveryFilePath().c_str());
}

void AgentServer::WriteDiscovery()
{
	json d;
	d["name"] = "PyrosBuilder";
	d["port"] = port_;
	d["token"] = token_;
	d["pid"] = (long long)(
#ifdef _WIN32
		GetCurrentProcessId()
#else
		::getpid()
#endif
	);
	const std::string path = DiscoveryFilePath();
	std::ofstream out(path);
	if (out)
		out << d.dump(2);
	out.close();

	// The discovery file carries the auth token for the loopback command
	// server; on a shared/multi-user machine the system temp dir is
	// world-readable, so restrict it to the owner. Best-effort: Windows'
	// ACL-based permission model has no direct equivalent to POSIX mode
	// bits, so this only tightens things on POSIX (macOS/Linux).
#ifndef _WIN32
	::chmod(path.c_str(), S_IRUSR | S_IWUSR);
#endif
}

std::string AgentServer::Base64Encode(const unsigned char* data, size_t len)
{
	static const char* table =
		"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
	std::string out;
	out.reserve(((len + 2) / 3) * 4);
	size_t i = 0;
	while (i + 3 <= len)
	{
		const unsigned int v = (data[i] << 16) | (data[i + 1] << 8) | data[i + 2];
		out.push_back(table[(v >> 18) & 63]);
		out.push_back(table[(v >> 12) & 63]);
		out.push_back(table[(v >> 6) & 63]);
		out.push_back(table[v & 63]);
		i += 3;
	}
	const unsigned rem = len - i;
	if (rem == 1)
	{
		const unsigned int v = data[i] << 16;
		out.push_back(table[(v >> 18) & 63]);
		out.push_back(table[(v >> 12) & 63]);
		out.push_back('=');
		out.push_back('=');
	}
	else if (rem == 2)
	{
		const unsigned int v = (data[i] << 16) | (data[i + 1] << 8);
		out.push_back(table[(v >> 18) & 63]);
		out.push_back(table[(v >> 12) & 63]);
		out.push_back(table[(v >> 6) & 63]);
		out.push_back('=');
	}
	return out;
}

bool AgentServer::Start(const Handler& handler)
{
	if (running_.load())
		return true;
	handler_ = handler;

	if (!SocketInit())
		return false;

#ifdef _WIN32
	listenFd_ = (sock_t)socket(AF_INET, SOCK_STREAM, 0);
#else
	listenFd_ = socket(AF_INET, SOCK_STREAM, 0);
#endif
	if (listenFd_ == AGENT_INVALID_SOCKET)
	{
		echo("AGENT: failed to create socket");
		return false;
	}

	int yes = 1;
	setsockopt(listenFd_, SOL_SOCKET, SO_REUSEADDR, (const char*)&yes, sizeof(yes));

	sockaddr_in addr;
	std::memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK); // 127.0.0.1 only
	addr.sin_port = 0; // OS-assigned

	if (bind(listenFd_, (sockaddr*)&addr, sizeof(addr)) == -1)
	{
		echo("AGENT: bind failed");
		AGENT_CLOSESOCKET(listenFd_);
		listenFd_ = AGENT_INVALID_SOCKET;
		return false;
	}

	if (listen(listenFd_, 8) == -1)
	{
		echo("AGENT: listen failed");
		AGENT_CLOSESOCKET(listenFd_);
		listenFd_ = AGENT_INVALID_SOCKET;
		return false;
	}

	socklen_t len = sizeof(addr);
	getsockname(listenFd_, (sockaddr*)&addr, &len);
	port_ = ntohs(addr.sin_port);

	// Random token (32 hex chars).
	std::random_device rd;
	std::mt19937_64 gen((uint64_t)(rd() ^ (port_ << 16) ^ 0x9e3779b97f4a7c15ULL));
	std::uniform_int_distribution<uint64_t> dist;
	char tok[33];
	for (int i = 0; i < 32; i++)
		tok[i] = "0123456789abcdef"[(dist(gen) >> 60) & 0xF];
	tok[32] = 0;
	token_.assign(tok, 32);

	running_.store(true);
	acceptThread_ = std::thread(&AgentServer::AcceptLoop, this);

	WriteDiscovery();
	echo("AGENT: command server listening on 127.0.0.1:" + std::to_string(port_));
	return true;
}

void AgentServer::Stop()
{
	if (!running_.exchange(false))
		return;

	if (listenFd_ != AGENT_INVALID_SOCKET)
	{
		// Closing the listen socket unblocks accept() on POSIX; on Winsock
		// it makes accept() fail with WSAECONNABORTED/WSAEINVAL.
		AGENT_CLOSESOCKET(listenFd_);
		listenFd_ = AGENT_INVALID_SOCKET;
	}

	if (acceptThread_.joinable())
		acceptThread_.join();

	std::lock_guard<std::mutex> lk(clientsMutex_);
	for (auto& c : clients_)
		if (c.fd != -1)
			AGENT_CLOSESOCKET((sock_t)c.fd);
	clients_.clear();

	DeleteDiscovery();
	echo("AGENT: command server stopped");
}

void AgentServer::AcceptLoop()
{
	while (running_.load())
	{
		sockaddr_in peer;
		socklen_t peerLen = sizeof(peer);
		sock_t c = accept(listenFd_, (sockaddr*)&peer, &peerLen);
		if (c == AGENT_INVALID_SOCKET)
		{
			if (!running_.load())
				break;
			// Transient (e.g. interrupted by Stop()); keep looping briefly.
			continue;
		}
		SetNonblocking(c);
		std::lock_guard<std::mutex> lk(clientsMutex_);
		Client cl;
		cl.fd = (intptr_t)c;
		clients_.push_back(cl);
	}
}

bool AgentServer::SendAll(intptr_t fd, const std::string& data)
{
	sock_t s = (sock_t)fd;
	size_t sent = 0;
	const int maxWaitMs = 5000;
	while (sent < data.size())
	{
		if (!FdWritable(s, maxWaitMs))
			return false;
#ifdef _WIN32
		const int n = (int)send(s, data.data() + sent, (int)(data.size() - sent), 0);
#else
		const ssize_t n = send(s, data.data() + sent, data.size() - sent, 0);
#endif
		if (n <= 0)
			return false;
		sent += (size_t)n;
	}
	return true;
}

void AgentServer::Process()
{
	if (!running_.load())
		return;

	// 1) Drain readable clients, split into JSON lines, validate token, queue.
	{
		std::vector<intptr_t> dead;
		std::vector<std::pair<intptr_t, std::string>> errors;

		{
			std::lock_guard<std::mutex> lk(clientsMutex_);
			for (auto& c : clients_)
			{
				if (c.fd == -1)
					continue;
				sock_t s = (sock_t)c.fd;
				if (!FdReadable(s, 0))
					continue;

				char buf[4096];
#ifdef _WIN32
				const int n = (int)recv(s, buf, sizeof(buf), 0);
#else
				const ssize_t n = recv(s, buf, sizeof(buf), 0);
#endif
				if (n <= 0)
				{
					c.fd = -1;
					dead.push_back((intptr_t)s);
					AGENT_CLOSESOCKET(s);
					continue;
				}

				c.buffer.append(buf, (size_t)n);

				size_t pos;
				while ((pos = c.buffer.find('\n')) != std::string::npos)
				{
					std::string line = c.buffer.substr(0, pos);
					c.buffer.erase(0, pos + 1);

					// Trim whitespace.
					while (!line.empty() && (line.back() == '\r' || line.back() == ' '))
						line.pop_back();
					size_t b = line.find_first_not_of(" \t");
					if (b == std::string::npos)
						line.clear();
					else
						line.erase(0, b);
					if (line.empty())
						continue;

					json msg;
					try
					{
						msg = json::parse(line);
					}
					catch (const std::exception&)
					{
						errors.emplace_back(c.fd, std::string("{\"id\":0,\"ok\":false,\"error\":\"invalid JSON\"}"));
						continue;
					}

					if (!msg.is_object() || msg.value("token", std::string()) != token_)
					{
						const uint64_t id = msg.is_object() ? msg.value("id", (uint64_t)0) : 0;
						std::ostringstream oss;
						oss << "{\"id\":" << id << ",\"ok\":false,\"error\":\"bad token\"}";
						errors.emplace_back(c.fd, oss.str());
						continue;
					}

					PendingCommand pc;
					pc.id = msg.value("id", (uint64_t)0);
					pc.cmd = msg.value("cmd", std::string());
					pc.args = msg.value("args", json::object());
					pc.fd = c.fd;
					std::lock_guard<std::mutex> q(queueMutex_);
					queue_.push_back(pc);
				}

				// Guard against a client streaming garbage without newlines.
				if (c.buffer.size() > (1u << 20))
				{
					c.fd = -1;
					dead.push_back((intptr_t)s);
					AGENT_CLOSESOCKET(s);
				}
			}

			clients_.erase(
				std::remove_if(clients_.begin(), clients_.end(),
					[](const Client& c) { return c.fd == -1; }),
				clients_.end());
		}

		for (auto& e : errors)
			SendAll(e.first, e.second + "\n");
	}

	// 2) Execute queued commands on the main thread and answer each client.
	std::vector<PendingCommand> batch;
	{
		std::lock_guard<std::mutex> q(queueMutex_);
		batch.swap(queue_);
	}

	for (auto& pc : batch)
	{
		json resp;
		try
		{
			if (!handler_)
				throw std::runtime_error("no handler");
			json cmdObj;
			cmdObj["cmd"] = pc.cmd;
			cmdObj["args"] = pc.args;
			const json result = handler_(cmdObj);
			resp = {{"id", pc.id}, {"ok", true}, {"result", result}};
		}
		catch (const std::exception& e)
		{
			resp = {{"id", pc.id}, {"ok", false}, {"error", e.what()}};
		}
		catch (...)
		{
			resp = {{"id", pc.id}, {"ok", false}, {"error", "unknown error"}};
		}
		if (pc.fd != -1)
			SendAll(pc.fd, resp.dump() + "\n");
	}
}
