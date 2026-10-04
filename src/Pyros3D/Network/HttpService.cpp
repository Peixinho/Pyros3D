//============================================================================
// Name        : HttpService.cpp
// Author      : Duarte Peixinho
// Description : See HttpService.h.
//============================================================================

#include <Pyros3D/Network/HttpService.h>
#include <Pyros3D/Core/Logs/Log.h>

#include <algorithm>
#include <chrono>
#include <cstring>
#include <fstream>
#include <sstream>

#ifdef _WIN32
	#include <winsock2.h>
	#include <ws2tcpip.h>
	typedef SOCKET http_sock_t;
	typedef int http_ssize_t;
	#define HTTP_INVALID INVALID_SOCKET
	#define HTTP_CLOSE(s) closesocket(s)
	#define HTTP_WOULDBLOCK (WSAGetLastError() == WSAEWOULDBLOCK)
#else
	#include <arpa/inet.h>
	#include <cerrno>
	#include <fcntl.h>
	#include <netinet/in.h>
	#include <sys/socket.h>
	#include <unistd.h>
	typedef int http_sock_t;
	typedef ssize_t http_ssize_t;
	#define HTTP_INVALID (-1)
	#define HTTP_CLOSE(s) ::close(s)
	#define HTTP_WOULDBLOCK (errno == EWOULDBLOCK || errno == EAGAIN)
#endif

namespace p3d {

	namespace {
		const size_t MaxHeader = 16 * 1024;
		const size_t MaxBody = 1024 * 1024;
		const size_t MaxConnections = 64;
		const double IdleSeconds = 10.0;

		double Now()
		{
			return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
		}

		bool SetNonBlocking(http_sock_t fd)
		{
#ifdef _WIN32
			u_long mode = 1;
			return ioctlsocket(fd, FIONBIO, &mode) == 0;
#else
			const int flags = fcntl(fd, F_GETFL, 0);
			return flags >= 0 && fcntl(fd, F_SETFL, flags | O_NONBLOCK) == 0;
#endif
		}

		std::string Lower(std::string s)
		{
			std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return (char)std::tolower(c); });
			return s;
		}

		std::string Trim(const std::string &s)
		{
			size_t a = 0, b = s.size();
			while (a < b && (s[a] == ' ' || s[a] == '\t')) a++;
			while (b > a && (s[b - 1] == ' ' || s[b - 1] == '\t' || s[b - 1] == '\r')) b--;
			return s.substr(a, b - a);
		}

		int Hex(const char c)
		{
			if (c >= '0' && c <= '9') return c - '0';
			if (c >= 'a' && c <= 'f') return c - 'a' + 10;
			if (c >= 'A' && c <= 'F') return c - 'A' + 10;
			return -1;
		}

		std::string UrlDecode(const std::string &s)
		{
			std::string out;
			for (size_t i = 0; i < s.size(); i++)
			{
				if (s[i] == '%' && i + 2 < s.size() && Hex(s[i + 1]) >= 0 && Hex(s[i + 2]) >= 0)
				{
					out += (char)(Hex(s[i + 1]) * 16 + Hex(s[i + 2]));
					i += 2;
				}
				else out += s[i];
			}
			return out;
		}

		const char* StatusText(const int status)
		{
			switch (status)
			{
				case 200: return "OK";
				case 204: return "No Content";
				case 400: return "Bad Request";
				case 401: return "Unauthorized";
				case 403: return "Forbidden";
				case 404: return "Not Found";
				case 405: return "Method Not Allowed";
				case 413: return "Payload Too Large";
				case 429: return "Too Many Requests";
				case 500: return "Internal Server Error";
				default: return "OK";
			}
		}

		const char* MimeOf(const std::string &path)
		{
			const size_t dot = path.find_last_of('.');
			const std::string ext = dot == std::string::npos ? std::string() : Lower(path.substr(dot + 1));
			if (ext == "html" || ext == "htm") return "text/html; charset=utf-8";
			if (ext == "js") return "text/javascript; charset=utf-8";
			if (ext == "css") return "text/css; charset=utf-8";
			if (ext == "json") return "application/json";
			if (ext == "png") return "image/png";
			if (ext == "jpg" || ext == "jpeg") return "image/jpeg";
			if (ext == "svg") return "image/svg+xml";
			if (ext == "ico") return "image/x-icon";
			if (ext == "txt") return "text/plain; charset=utf-8";
			return "application/octet-stream";
		}

#ifdef _WIN32
		bool g_wsa = false;
#endif
	}

	struct HttpService::Connection
	{
		http_sock_t fd = HTTP_INVALID;
		std::string in, out, remote;
		size_t sent = 0;
		double last = 0.0;
		bool answered = false;
	};

	std::string HttpService::Request::Cookie(const std::string &name) const
	{
		std::map<std::string, std::string>::const_iterator h = headers.find("cookie");
		if (h == headers.end()) return std::string();
		std::stringstream ss(h->second);
		std::string part;
		while (std::getline(ss, part, ';'))
		{
			const size_t eq = part.find('=');
			if (eq == std::string::npos) continue;
			if (Trim(part.substr(0, eq)) == name) return Trim(part.substr(eq + 1));
		}
		return std::string();
	}

	HttpService::HttpService() : listener((intptr_t)HTTP_INVALID), port(0) {}

	HttpService::~HttpService() { Stop(); }

	bool HttpService::IsRunning() const { return (http_sock_t)listener != HTTP_INVALID; }

	bool HttpService::Start(const uint16 wanted, const std::string &bind)
	{
		Stop();
#ifdef _WIN32
		if (!g_wsa)
		{
			WSADATA wsa;
			if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) return false;
			g_wsa = true;
		}
#endif
		const http_sock_t fd = ::socket(AF_INET, SOCK_STREAM, 0);
		if (fd == HTTP_INVALID) return false;
		int yes = 1;
		setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (const char*)&yes, sizeof(yes));
		sockaddr_in addr;
		std::memset(&addr, 0, sizeof(addr));
		addr.sin_family = AF_INET;
		addr.sin_port = htons(wanted);
		if (inet_pton(AF_INET, bind.c_str(), &addr.sin_addr) != 1) addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
		if (::bind(fd, (sockaddr*)&addr, sizeof(addr)) != 0 || ::listen(fd, 16) != 0 || !SetNonBlocking(fd))
		{
			HTTP_CLOSE(fd);
			return false;
		}
		listener = (intptr_t)fd;
		port = wanted;
		return true;
	}

	void HttpService::Stop()
	{
		for (size_t i = 0; i < connections.size(); i++)
		{
			if (connections[i]->fd != HTTP_INVALID) HTTP_CLOSE(connections[i]->fd);
			delete connections[i];
		}
		connections.clear();
		if ((http_sock_t)listener != HTTP_INVALID) HTTP_CLOSE((http_sock_t)listener);
		listener = (intptr_t)HTTP_INVALID;
	}

	void HttpService::ServeFile(const Request &q, Response &r)
	{
		if (q.method != "GET" && q.method != "HEAD") { r.status = 405; r.contentType = "text/plain"; r.body = "method not allowed"; return; }
		std::string rel = q.path;
		if (rel.empty() || rel[rel.size() - 1] == '/') rel += "index.html";
		// nothing above the root, and nothing hidden
		if (staticRoot.empty() || rel.find("..") != std::string::npos || rel.find('\\') != std::string::npos
			|| rel.find("/.") != std::string::npos || rel.find('\0') != std::string::npos)
		{
			r.status = 404; r.contentType = "text/plain"; r.body = "not found";
			return;
		}
		std::ifstream in((staticRoot + rel).c_str(), std::ios::binary);
		if (!in.is_open()) { r.status = 404; r.contentType = "text/plain"; r.body = "not found"; return; }
		std::stringstream ss;
		ss << in.rdbuf();
		r.body = ss.str();
		r.contentType = MimeOf(rel);
		r.headers["Cache-Control"] = "no-cache";
	}

	// A whole request is in c.in: answer it.
	void HttpService::Serve(Connection &c)
	{
		Request q;
		Response r;
		q.remote = c.remote;
		const size_t headEnd = c.in.find("\r\n\r\n");
		std::stringstream head(c.in.substr(0, headEnd));
		std::string line;
		std::getline(head, line);
		{
			std::stringstream first(line);
			std::string target;
			first >> q.method >> target;
			const size_t mark = target.find('?');
			q.path = UrlDecode(mark == std::string::npos ? target : target.substr(0, mark));
			if (mark != std::string::npos) q.query = target.substr(mark + 1);
		}
		while (std::getline(head, line))
		{
			const size_t colon = line.find(':');
			if (colon == std::string::npos) continue;
			q.headers[Lower(Trim(line.substr(0, colon)))] = Trim(line.substr(colon + 1));
		}
		q.body = c.in.substr(headEnd + 4);

		if (q.method.empty() || q.path.empty() || q.path[0] != '/')
		{
			r.status = 400; r.contentType = "text/plain"; r.body = "bad request";
		}
		else if (q.path.compare(0, 5, "/api/") == 0)
		{
			if (handler)
			{
				try { handler(q, r); }
				catch (const std::exception &e) { r = Response(); r.status = 500; r.body = "{\"error\":\"internal error\"}"; echo(std::string("ERROR: HttpService handler - ") + e.what()); }
			}
			else { r.status = 404; r.body = "{\"error\":\"not found\"}"; }
			r.headers["Cache-Control"] = "no-store";
		}
		else ServeFile(q, r);

		std::stringstream out;
		out << "HTTP/1.1 " << r.status << " " << StatusText(r.status) << "\r\n";
		out << "Content-Type: " << r.contentType << "\r\n";
		out << "Content-Length: " << r.body.size() << "\r\n";
		out << "Connection: close\r\n";
		out << "X-Content-Type-Options: nosniff\r\n";
		out << "X-Frame-Options: DENY\r\n";
		for (std::map<std::string, std::string>::const_iterator h = r.headers.begin(); h != r.headers.end(); ++h)
			out << h->first << ": " << h->second << "\r\n";
		out << "\r\n";
		c.out = out.str();
		if (q.method != "HEAD") c.out += r.body;
		c.answered = true;
		c.in.clear();
	}

	void HttpService::Poll()
	{
		if (!IsRunning()) return;
		const double now = Now();

		// whoever is knocking
		for (int n = 0; n < 16; n++)
		{
			sockaddr_in from;
			socklen_t len = sizeof(from);
			const http_sock_t fd = ::accept((http_sock_t)listener, (sockaddr*)&from, &len);
			if (fd == HTTP_INVALID) break;
			if (connections.size() >= MaxConnections || !SetNonBlocking(fd)) { HTTP_CLOSE(fd); continue; }
			Connection* c = new Connection();
			c->fd = fd;
			c->last = now;
			char text[64] = { 0 };
			inet_ntop(AF_INET, &from.sin_addr, text, sizeof(text));
			c->remote = text;
			connections.push_back(c);
		}

		for (size_t i = 0; i < connections.size();)
		{
			Connection &c = *connections[i];
			bool close = false;

			if (!c.answered)
			{
				char buf[8192];
				for (;;)
				{
					const http_ssize_t got = ::recv(c.fd, buf, sizeof(buf), 0);
					if (got > 0) { c.in.append(buf, (size_t)got); c.last = now; if (c.in.size() > MaxHeader + MaxBody) { close = true; break; } continue; }
					if (got == 0) close = true;				// hung up before asking for anything
					else if (!HTTP_WOULDBLOCK) close = true;
					break;
				}
				const size_t headEnd = c.in.find("\r\n\r\n");
				if (!close && headEnd == std::string::npos && c.in.size() > MaxHeader) close = true;
				if (headEnd != std::string::npos)
				{
					// the body, if one was promised
					size_t want = 0;
					const std::string head = Lower(c.in.substr(0, headEnd));
					const size_t at = head.find("content-length:");
					if (at != std::string::npos) want = (size_t)std::strtoul(head.c_str() + at + 15, NULL, 10);
					if (want > MaxBody)
					{
						c.out = "HTTP/1.1 413 Payload Too Large\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
						c.answered = true;
						close = false;
					}
					else if (c.in.size() >= headEnd + 4 + want)
					{
						c.in.resize(headEnd + 4 + want);
						Serve(c);
						close = false;
					}
				}
			}

			if (c.answered)
			{
				while (c.sent < c.out.size())
				{
					const http_ssize_t put = ::send(c.fd, c.out.data() + c.sent, (int)std::min<size_t>(c.out.size() - c.sent, 65536), 0);
					if (put > 0) { c.sent += (size_t)put; c.last = now; continue; }
					if (!HTTP_WOULDBLOCK) close = true;
					break;
				}
				if (c.sent >= c.out.size()) close = true;
			}

			if (!close && now - c.last > IdleSeconds) close = true;
			if (close)
			{
				HTTP_CLOSE(c.fd);
				delete connections[i];
				connections.erase(connections.begin() + i);
			}
			else i++;
		}
	}

}
