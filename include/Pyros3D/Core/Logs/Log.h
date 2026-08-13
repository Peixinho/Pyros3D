//============================================================================
// Name        : Log.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Log
//============================================================================

#ifndef LOG_H
#define	LOG_H

#include <Pyros3D/Other/Export.h>
#include <string>
#include <iostream>
#include <fstream>
#include <vector>
#if defined(ANDROID)
#include <android/log.h>
#endif
// Filename
#define LOG_FILE_PATH "Pyros.log"

namespace p3d {

	namespace LOG {

		// Severity, classified once at record time from the message's own
		// prefix. The engine has always written "ERROR: ...", "WARNING: ..."
		// and "SUCCESS: ..." by convention; this reads that convention
		// rather than requiring 122 call sites to be rewritten to pass a
		// level.
		namespace Level
		{
			enum {
				Error = 0,
				Warning = 1,
				Success = 2,
				Info = 3
			};
		}

		struct Entry
		{
			std::string text;
			int level;
			bool error;   // level == Level::Error, kept for brevity at call sites
		};

		class PYROS3D_API _LOG {
		public:
#ifdef LOG_TO_FILE
			static std::ofstream outputFile;
#endif
			static bool _initiated;

			// Every message is recorded here regardless of which sinks are
			// compiled in, because the sinks cannot cover each other's blind
			// spots: std::cout goes nowhere at all in a GUI-subsystem process
			// on Windows, and an in-app viewer cannot show anything logged
			// before it exists - which is exactly when device creation,
			// shader compilation and script init fail. Recording
			// unconditionally means a viewer that opens late still sees the
			// startup that went wrong.
			//
			// Fixed-capacity ring: a long-running app must not grow this
			// without bound, and dropping the oldest lines is the right
			// trade when the newest are what you are looking at. Not
			// thread-safe - logging here is called from the main thread.
			static std::vector<Entry> _entries;
			static unsigned int _entriesStart;
			static const unsigned int _entriesMax = 1024;

			static unsigned int MessageCount() { return (unsigned int)_entries.size(); }
			// 0 is the oldest line still retained, not the oldest ever logged.
			static const Entry &MessageAt(const unsigned int i)
			{
				return _entries[(_entriesStart + i) % _entries.size()];
			}
			static void ClearMessages() { _entries.clear(); _entriesStart = 0; }

			// Anything above this is dropped outright - not recorded, not
			// printed. Defaults to Warning because the engine logs a SUCCESS
			// line per component added and per GameObject added to a scene:
			// loading one demo scene emits 250-odd of them, which buries any
			// real message in both the console and the ring buffer. Raise it
			// at runtime when that trace is what you actually want.
			static int _threshold;
			static void SetLevel(const int level) { _threshold = level; }
			static int GetLevel() { return _threshold; }

			// When false, skip std::cout / Android log even if LOG_TO_CONSOLE
			// or _DEBUG is compiled in. The ring buffer still records. The
			// editor turns this off so messages only appear in the Log panel.
			static bool _mirrorStdout;
			static void SetMirrorStdout(const bool enabled) { _mirrorStdout = enabled; }
			static bool GetMirrorStdout() { return _mirrorStdout; }

			static int _classify(const std::string &Message)
			{
				if (Message.compare(0, 6, "ERROR:") == 0) return Level::Error;
				if (Message.compare(0, 8, "WARNING:") == 0) return Level::Warning;
				if (Message.compare(0, 8, "SUCCESS:") == 0) return Level::Success;
				return Level::Info;
			}

			static void _record(const std::string &Message)
			{
				Entry e;
				e.text = Message;
				e.level = _classify(Message);
				e.error = (e.level == Level::Error);
				if (_entries.size() < _entriesMax)
				{
					_entries.push_back(e);
				}
				else
				{
					_entries[_entriesStart] = e;
					_entriesStart = (_entriesStart + 1) % _entriesMax;
				}
			}

			static void _message(const std::string &Message)
			{
				if (_classify(Message) > _threshold)
					return;
				_record(Message);

#if defined(LOG_TO_FILE)

				outputFile << Message << std::endl;

#elif defined(LOG_DISABLE)

				// NONE - deliberately empty. This branch used to print to
				// std::cout anyway, directly contradicting both its name and
				// the comment, so opting out of logging was the one thing
				// LOG_DISABLE could not do.
				(void)Message;

#elif defined(LOG_TO_CONSOLE) || defined(_DEBUG)
				if (_mirrorStdout)
				{
#if defined(ANDROID)
					__android_log_print(ANDROID_LOG_DEBUG, "Pyros3D", "%s", Message.c_str());
#else
					std::cout << Message << std::endl;
#endif
				}
#endif
			}
			static void _echo(const std::string &Message)
			{
				if (!_initiated)
				{
					_initiated = true;
					_message("=== Pyros3D Start ===");
				}

				_message(Message);

			}
		};

#define echo( x ) ( p3d::LOG::_LOG::_echo(x) )

	};
};

#endif	/* LOG_H */
