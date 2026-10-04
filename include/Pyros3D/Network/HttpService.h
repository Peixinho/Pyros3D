//============================================================================
// Name        : HttpService.h
// Author      : Duarte Peixinho
// Description : A very small HTTP/1.1 server, for a headless process to be
//               looked at and managed from a browser: a dedicated server's
//               admin page, a build machine's status, a tool's control
//               panel.
//
//                 HttpService web;
//                 web.SetStaticRoot("assets/admin");          // files: /, /app.js ...
//                 web.SetHandler([](const HttpService::Request &q, HttpService::Response &r) {
//                     r.body = "{\"ok\":true}";                // everything under /api/
//                 });
//                 web.Start(8080, "127.0.0.1");
//                 for (;;) { web.Poll(); ... }                 // from the main loop
//
//               It has no thread of its own: Poll() accepts, reads, answers
//               and returns without ever blocking, and the handler runs
//               inside it - on the caller's thread, where the scene, the
//               scripts and the network session can be touched freely.
//
//               What it is NOT: it speaks plain HTTP, one request a
//               connection, bodies up to a megabyte. On a public address
//               put it behind something that does TLS (a reverse proxy, an
//               SSH tunnel) - a password sent to it in the clear can be
//               read on the way. Who may call the handler is the handler's
//               business; this class authenticates nobody.
//============================================================================

#ifndef HTTPSERVICE_H
#define HTTPSERVICE_H

#include <Pyros3D/Core/Math/Math.h>
#include <Pyros3D/Other/Export.h>
#include <functional>
#include <map>
#include <string>
#include <vector>

namespace p3d {

	class PYROS3D_API HttpService {
	public:
		// Exported in its own right: a nested type does not take the outer
		// class's export, and Cookie() is defined in the library. Without
		// this a DLL build's users (PyrosServer) could not link it.
		struct PYROS3D_API Request
		{
			std::string method;		// GET, POST ...
			std::string path;		// "/api/status" - decoded, without the query
			std::string query;		// after the '?', as sent
			std::string body;
			std::string remote;		// the caller's address
			std::map<std::string, std::string> headers;	// names in lower case
			// One cookie's value, or "" - what a login is usually kept in.
			std::string Cookie(const std::string &name) const;
		};
		struct Response
		{
			int status = 200;
			std::string contentType = "application/json";
			std::string body;
			std::map<std::string, std::string> headers;
		};
		typedef std::function<void(const Request &, Response &)> Handler;

		HttpService();
		~HttpService();

		// `bind`: the address to listen on. 127.0.0.1 is reachable from this
		// machine only; 0.0.0.0 from anywhere that can reach it.
		bool Start(const uint16 port, const std::string &bind = "127.0.0.1");
		void Stop();
		bool IsRunning() const;
		uint16 Port() const { return port; }

		// Files under this folder are served for any path not under /api/;
		// "/" is index.html. Nothing above the folder can be asked for.
		void SetStaticRoot(const std::string &dir) { staticRoot = dir; }
		// Every request under /api/.
		void SetHandler(const Handler &h) { handler = h; }

		// Call every frame. Never blocks.
		void Poll();

	private:
		struct Connection;
		void Serve(Connection &c);
		void ServeFile(const Request &q, Response &r);

		std::vector<Connection*> connections;
		std::string staticRoot;
		Handler handler;
		intptr_t listener;
		uint16 port;
	};

}

#endif /* HTTPSERVICE_H */
