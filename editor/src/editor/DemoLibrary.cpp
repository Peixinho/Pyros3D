//=============================================================================
// Name        : DemoLibrary.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : GitHub examples browser + installer (see DemoLibrary.h).
//=============================================================================

#include "DemoLibrary.h"
#include "ProjectManager.h"

#include <Pyros3D/Utils/Json/json.hpp>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>

#ifndef PYROS_EDITOR_NO_CURL
#include <curl/curl.h>
#endif

using nlohmann::json;
namespace fs = std::filesystem;

namespace {

	const char* kUserAgent = "PyrosBuilder";

	std::string Trim(const std::string& s)
	{
		size_t b = 0, e = s.size();
		while (b < e && std::isspace((unsigned char)s[b])) ++b;
		while (e > b && std::isspace((unsigned char)s[e - 1])) --e;
		return s.substr(b, e - b);
	}

	// Percent-encodes a repo path for a URL, leaving '/' alone - a path is a
	// sequence of segments, not one opaque string, and a file called
	// "my model.p3dm" is otherwise a 400 from the CDN rather than a download.
	std::string UrlEncodePath(const std::string& path)
	{
		static const char* hex = "0123456789ABCDEF";
		std::string out;
		out.reserve(path.size() + 8);
		for (size_t i = 0; i < path.size(); ++i)
		{
			const unsigned char c = (unsigned char)path[i];
			if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~' || c == '/')
				out.push_back((char)c);
			else
			{
				out.push_back('%');
				out.push_back(hex[(c >> 4) & 0xF]);
				out.push_back(hex[c & 0xF]);
			}
		}
		return out;
	}

	// A path out of the tree listing is attacker-controlled as far as this
	// editor is concerned: anyone can put a repo behind the source field.
	// Nothing with a ".." segment, a root, or a drive letter gets written.
	bool SafeRepoPath(const std::string& path)
	{
		if (path.empty()) return false;
		if (path[0] == '/' || path[0] == '\\') return false;
		if (path.find('\\') != std::string::npos) return false;
		if (path.find(':') != std::string::npos) return false;
		size_t start = 0;
		while (start <= path.size())
		{
			const size_t slash = path.find('/', start);
			const std::string seg = path.substr(start,
				slash == std::string::npos ? std::string::npos : slash - start);
			if (seg.empty() || seg == "." || seg == "..")
				return false;
			if (slash == std::string::npos) break;
			start = slash + 1;
		}
		return true;
	}

	std::string TopFolder(const std::string& path)
	{
		const size_t slash = path.find('/');
		if (slash == std::string::npos) return std::string();
		return path.substr(0, slash);
	}

	std::string HumanSize(unsigned long long bytes)
	{
		char buf[64];
		if (bytes >= 1024ull * 1024ull * 1024ull)
			std::snprintf(buf, sizeof(buf), "%.1f GB", (double)bytes / (1024.0 * 1024.0 * 1024.0));
		else if (bytes >= 1024ull * 1024ull)
			std::snprintf(buf, sizeof(buf), "%.1f MB", (double)bytes / (1024.0 * 1024.0));
		else if (bytes >= 1024ull)
			std::snprintf(buf, sizeof(buf), "%.1f KB", (double)bytes / 1024.0);
		else
			std::snprintf(buf, sizeof(buf), "%llu B", bytes);
		return buf;
	}

	// First real paragraph of a README, used as the description when a folder
	// has no demo.json. Headings, badges and HTML lines are skipped - the
	// point is a sentence a person can read in a list, not the whole file.
	std::string FirstReadmeParagraph(const std::string& md)
	{
		std::istringstream in(md);
		std::string line, para;
		while (std::getline(in, line))
		{
			const std::string t = Trim(line);
			if (t.empty())
			{
				if (!para.empty()) break;
				continue;
			}
			if (t[0] == '#' || t[0] == '<' || t[0] == '!' || t[0] == '|') continue;
			if (t.rfind("[!", 0) == 0) continue;
			if (!para.empty()) para += ' ';
			para += t;
			if (para.size() > 400) break;
		}
		if (para.size() > 400)
			para = para.substr(0, 397) + "...";
		return para;
	}

#ifndef PYROS_EDITOR_NO_CURL

	struct FetchCtx {
		std::string* out = NULL;
		const std::atomic<bool>* cancel = NULL;
		std::atomic<unsigned long long>* bytes = NULL;
	};

	size_t FetchWriteCb(char* p, size_t sz, size_t nm, void* ud)
	{
		FetchCtx* ctx = (FetchCtx*)ud;
		const size_t len = sz * nm;
		if (!ctx || !ctx->out) return len;
		// Returning short makes curl fail the transfer - how Cancel aborts a
		// download that is mid-file rather than between files.
		if (ctx->cancel && ctx->cancel->load()) return 0;
		ctx->out->append(p, len);
		if (ctx->bytes) ctx->bytes->fetch_add(len);
		return len;
	}

	// One blocking GET. `apiJson` adds the api.github.com headers (and the
	// token, when the environment has one) - raw.githubusercontent.com wants
	// none of them.
	bool HttpGet(const std::string& url, bool apiJson, std::string& out, long& httpCode,
		std::string& err, const std::atomic<bool>* cancel,
		std::atomic<unsigned long long>* byteCounter)
	{
		out.clear();
		httpCode = 0;
		CURL* curl = curl_easy_init();
		if (!curl)
		{
			err = "curl_easy_init failed";
			return false;
		}

		struct curl_slist* hdrs = NULL;
		hdrs = curl_slist_append(hdrs, (std::string("User-Agent: ") + kUserAgent).c_str());
		if (apiJson)
		{
			hdrs = curl_slist_append(hdrs, "Accept: application/vnd.github+json");
			hdrs = curl_slist_append(hdrs, "X-GitHub-Api-Version: 2022-11-28");
		}
		// Unauthenticated is 60 API calls an hour per IP, which is plenty for
		// a listing (one call) but not for someone poking at it all morning.
		// A token in the environment lifts it to 5000 without this ever
		// storing a credential of its own.
		const char* token = std::getenv("GITHUB_TOKEN");
		if (apiJson && token && token[0])
			hdrs = curl_slist_append(hdrs, (std::string("Authorization: Bearer ") + token).c_str());

		FetchCtx ctx;
		ctx.out = &out;
		ctx.cancel = cancel;
		ctx.bytes = byteCounter;

		curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
		curl_easy_setopt(curl, CURLOPT_HTTPHEADER, hdrs);
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, FetchWriteCb);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, &ctx);
		curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
		curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
		curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 20L);
		curl_easy_setopt(curl, CURLOPT_TIMEOUT, 600L);
		curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, "");

		const CURLcode rc = curl_easy_perform(curl);
		curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
		curl_slist_free_all(hdrs);
		curl_easy_cleanup(curl);

		if (rc != CURLE_OK)
		{
			if (cancel && cancel->load())
				err = "Cancelled";
			else
				err = curl_easy_strerror(rc);
			return false;
		}
		if (httpCode < 200 || httpCode >= 300)
		{
			std::ostringstream os;
			os << "HTTP " << httpCode;
			if (httpCode == 403 || httpCode == 429)
				os << " - GitHub rate limit reached. Set GITHUB_TOKEN in the "
				      "environment to raise it, or try again later.";
			else if (httpCode == 404)
				os << " - not found";
			err = os.str();
			return false;
		}
		return true;
	}

#else // PYROS_EDITOR_NO_CURL

	bool HttpGet(const std::string&, bool, std::string& out, long& httpCode,
		std::string& err, const std::atomic<bool>*, std::atomic<unsigned long long>*)
	{
		out.clear();
		httpCode = 0;
		err = "This build has no HTTP client (see PYROS_EDITOR_NO_CURL).";
		return false;
	}

#endif

	std::string ApiTreeUrl(const DemoSource& s)
	{
		return "https://api.github.com/repos/" + s.owner + "/" + s.repo
			+ "/git/trees/" + UrlEncodePath(s.branch) + "?recursive=1";
	}

	std::string RawUrl(const DemoSource& s, const std::string& repoPath)
	{
		return "https://raw.githubusercontent.com/" + s.owner + "/" + s.repo
			+ "/" + UrlEncodePath(s.branch) + "/" + UrlEncodePath(repoPath);
	}

	bool WriteBinaryFile(const fs::path& path, const std::string& bytes, std::string& err)
	{
		std::error_code ec;
		fs::create_directories(path.parent_path(), ec);
		std::ofstream out(path, std::ios::binary | std::ios::trunc);
		if (!out)
		{
			err = "Cannot write " + path.string();
			return false;
		}
		out.write(bytes.data(), (std::streamsize)bytes.size());
		if (!out)
		{
			err = "Write failed: " + path.string();
			return false;
		}
		return true;
	}

	// One entry of the Git Trees response we care about.
	struct TreeBlob {
		std::string path;
		std::string sha;
		unsigned long long size = 0;
	};

} // namespace

//=============================================================================
// DemoSource
//=============================================================================

std::string DemoSource::Slug() const
{
	std::string s = owner + "/" + repo;
	if (!branch.empty() && branch != "main")
		s += "@" + branch;
	return s;
}

DemoSource DemoSource::Parse(const std::string& text)
{
	DemoSource out;
	std::string s = Trim(text);

	// https://github.com/owner/repo(.git)(/tree/branch) - paste what you have.
	const char* prefixes[] = { "https://github.com/", "http://github.com/", "github.com/" };
	for (size_t i = 0; i < sizeof(prefixes) / sizeof(prefixes[0]); ++i)
	{
		const std::string p = prefixes[i];
		if (s.rfind(p, 0) == 0) { s = s.substr(p.size()); break; }
	}
	if (s.size() > 4 && s.compare(s.size() - 4, 4, ".git") == 0)
		s = s.substr(0, s.size() - 4);

	std::string branch;
	const size_t at = s.find('@');
	if (at != std::string::npos)
	{
		branch = Trim(s.substr(at + 1));
		s = s.substr(0, at);
	}

	std::vector<std::string> parts;
	size_t start = 0;
	while (start <= s.size())
	{
		const size_t slash = s.find('/', start);
		parts.push_back(s.substr(start, slash == std::string::npos ? std::string::npos : slash - start));
		if (slash == std::string::npos) break;
		start = slash + 1;
	}
	if (parts.size() >= 2)
	{
		out.owner = Trim(parts[0]);
		out.repo = Trim(parts[1]);
	}
	// .../tree/<branch> from a browser URL.
	if (branch.empty() && parts.size() >= 4 && parts[2] == "tree")
		branch = Trim(parts[3]);
	if (!branch.empty())
		out.branch = branch;
	return out;
}

//=============================================================================
// DemoLibrary
//=============================================================================

DemoLibrary::DemoLibrary()
{
#ifndef PYROS_EDITOR_NO_CURL
	curl_global_init(CURL_GLOBAL_DEFAULT);
#endif
	LoadSource();
}

DemoLibrary::~DemoLibrary()
{
	StopWorker();
}

bool DemoLibrary::Available()
{
#ifdef PYROS_EDITOR_NO_CURL
	return false;
#else
	return true;
#endif
}

void DemoLibrary::StopWorker()
{
	cancel_ = true;
	if (worker_.joinable())
		worker_.join();
	cancel_ = false;
}

std::string DemoLibrary::CacheDirectory()
{
	const std::string support = ProjectManager::EditorSupportDirectory();
	if (support.empty()) return std::string();
	const fs::path dir = fs::path(support) / "demo-cache";
	std::error_code ec;
	fs::create_directories(dir, ec);
	return dir.string();
}

std::string DemoLibrary::SourceFilePath()
{
	const std::string support = ProjectManager::EditorSupportDirectory();
	if (support.empty()) return std::string();
	return (fs::path(support) / "examples_source.txt").string();
}

void DemoLibrary::LoadSource()
{
	const std::string path = SourceFilePath();
	if (path.empty()) return;
	std::ifstream in(path.c_str());
	if (!in) return;
	std::string line;
	if (std::getline(in, line))
	{
		const std::string t = Trim(line);
		if (!t.empty())
		{
			const DemoSource parsed = DemoSource::Parse(t);
			if (parsed.Valid())
				source_ = parsed;
		}
	}
}

void DemoLibrary::SaveSource() const
{
	const std::string path = SourceFilePath();
	if (path.empty()) return;
	std::ofstream out(path.c_str(), std::ios::trunc);
	if (!out) return;
	out << source_.owner << "/" << source_.repo << "@" << source_.branch << "\n";
}

void DemoLibrary::SetSource(const DemoSource& s)
{
	if (!s.Valid()) return;
	if (s.owner == source_.owner && s.repo == source_.repo && s.branch == source_.branch)
		return;
	StopWorker();
	source_ = s;
	SaveSource();
	entries_.clear();
	catalogError_.clear();
	catalogState_ = CatalogState::Idle;
}

void DemoLibrary::Refresh()
{
	if (workerBusy_.load()) return;
	if (!source_.Valid()) return;
	StopWorker();
	entries_.clear();
	catalogError_.clear();
	catalogState_ = CatalogState::Loading;
	workerBusy_ = true;
	const DemoSource src = source_;
	worker_ = std::thread(&DemoLibrary::CatalogWorker, this, src);
}

void DemoLibrary::Install(const DemoEntry& demo, const std::string& destinationParentDir)
{
	if (workerBusy_.load()) return;
	if (demo.folder.empty() || destinationParentDir.empty()) return;
	StopWorker();
	installError_.clear();
	installedProjectJson_.clear();
	filesDone_ = 0;
	filesTotal_ = demo.fileCount;
	bytesDone_ = 0;
	installState_ = InstallState::Running;
	workerBusy_ = true;
	worker_ = std::thread(&DemoLibrary::InstallWorker, this, demo, destinationParentDir);
}

void DemoLibrary::CancelInstall()
{
	if (installState_.load() == InstallState::Running)
		cancel_ = true;
}

float DemoLibrary::InstallProgress() const
{
	const size_t total = filesTotal_.load();
	if (total == 0) return 0.f;
	return (float)filesDone_.load() / (float)total;
}

std::string DemoLibrary::InstallStatus() const
{
	std::ostringstream os;
	os << filesDone_.load() << " / " << filesTotal_.load() << " files";
	const unsigned long long bytes = bytesDone_.load();
	if (bytes > 0)
		os << "  -  " << HumanSize(bytes);
	std::string current;
	{
		std::lock_guard<std::mutex> lk(mutex_);
		current = installFileLabel_;
	}
	if (!current.empty())
		os << "\n" << current;
	return os.str();
}

void DemoLibrary::ClearInstallResult()
{
	const InstallState st = installState_.load();
	if (st == InstallState::Done || st == InstallState::Failed)
	{
		installState_ = InstallState::Idle;
		installError_.clear();
		installedProjectJson_.clear();
	}
}

void DemoLibrary::Pump()
{
	std::vector<DemoEntry> newEntries;
	std::vector<std::pair<std::string, std::string> > thumbs;
	bool haveEntries = false;
	{
		std::lock_guard<std::mutex> lk(mutex_);
		if (pendingEntriesValid_)
		{
			newEntries.swap(pendingEntries_);
			pendingEntriesValid_ = false;
			haveEntries = true;
		}
		thumbs.swap(pendingThumbs_);
		if (!pendingCatalogError_.empty())
		{
			catalogError_ = pendingCatalogError_;
			pendingCatalogError_.clear();
		}
		if (!pendingInstallError_.empty())
		{
			installError_ = pendingInstallError_;
			pendingInstallError_.clear();
		}
		if (!pendingInstalledProjectJson_.empty())
		{
			installedProjectJson_ = pendingInstalledProjectJson_;
			pendingInstalledProjectJson_.clear();
		}
	}

	if (haveEntries)
		entries_.swap(newEntries);

	for (size_t i = 0; i < thumbs.size(); ++i)
	{
		for (size_t e = 0; e < entries_.size(); ++e)
		{
			if (entries_[e].folder == thumbs[i].first)
			{
				entries_[e].thumbnailFile = thumbs[i].second;
				break;
			}
		}
	}

	// The worker has finished when it cleared the busy flag; joining here
	// keeps the thread object reusable without the UI ever blocking on it.
	if (!workerBusy_.load() && worker_.joinable())
	{
		worker_.join();
		cancel_ = false;
	}
}

//=============================================================================
// Workers (no ImGui, no engine objects - network and std::filesystem only)
//=============================================================================

void DemoLibrary::CatalogWorker(DemoSource src)
{
	std::string body, err;
	long code = 0;

	bool ok = HttpGet(ApiTreeUrl(src), true, body, code, err, &cancel_, NULL);
	if (!ok && code == 404)
	{
		// Wrong branch is the likely cause, not a wrong repo: "main" is only
		// a default, and plenty of repos are still on "master". Ask the repo
		// which branch it actually has and try that one.
		std::string repoBody, repoErr;
		long repoCode = 0;
		if (HttpGet("https://api.github.com/repos/" + src.owner + "/" + src.repo,
			true, repoBody, repoCode, repoErr, &cancel_, NULL))
		{
			try
			{
				const json j = json::parse(repoBody);
				const std::string def = j.value("default_branch", std::string());
				if (!def.empty() && def != src.branch)
				{
					src.branch = def;
					ok = HttpGet(ApiTreeUrl(src), true, body, code, err, &cancel_, NULL);
				}
			}
			catch (...) { /* keep the original 404 */ }
		}
	}

	if (!ok)
	{
		std::lock_guard<std::mutex> lk(mutex_);
		pendingCatalogError_ = "Could not list " + src.owner + "/" + src.repo + ": " + err;
		catalogState_ = CatalogState::Failed;
		workerBusy_ = false;
		return;
	}

	std::vector<TreeBlob> blobs;
	bool truncated = false;
	try
	{
		const json j = json::parse(body);
		truncated = j.value("truncated", false);
		const json tree = j.value("tree", json::array());
		for (json::const_iterator it = tree.begin(); it != tree.end(); ++it)
		{
			if (it->value("type", std::string()) != "blob") continue;
			// 120000 is a symlink: its "content" is the target path, and
			// writing that out as a file would be a lie. Submodules never
			// reach here (they are type "commit").
			if (it->value("mode", std::string()) == "120000") continue;
			TreeBlob b;
			b.path = it->value("path", std::string());
			b.sha = it->value("sha", std::string());
			b.size = it->value("size", (unsigned long long)0);
			if (!SafeRepoPath(b.path)) continue;
			blobs.push_back(b);
		}
	}
	catch (const std::exception& e)
	{
		std::lock_guard<std::mutex> lk(mutex_);
		pendingCatalogError_ = std::string("Malformed listing from GitHub: ") + e.what();
		catalogState_ = CatalogState::Failed;
		workerBusy_ = false;
		return;
	}

	// Group by top-level folder. A folder is an example when it has a
	// project.json at its root - that is the whole publishing contract.
	std::map<std::string, DemoEntry> byFolder;
	std::map<std::string, std::string> thumbSha;
	for (size_t i = 0; i < blobs.size(); ++i)
	{
		const std::string folder = TopFolder(blobs[i].path);
		if (folder.empty() || folder[0] == '.') continue;
		DemoEntry& e = byFolder[folder];
		e.folder = folder;
		e.fileCount++;
		e.totalBytes += blobs[i].size;

		const std::string rel = blobs[i].path.substr(folder.size() + 1);
		if (rel == "project.json")
			e.name = folder; // replaced by demo.json's name below, if any
		else if (rel == "preview.png" || rel == "preview.jpg" || rel == ".demo/preview.png")
		{
			e.thumbnailRepoPath = blobs[i].path;
			thumbSha[folder] = blobs[i].sha;
		}
	}

	std::vector<DemoEntry> found;
	for (std::map<std::string, DemoEntry>::const_iterator it = byFolder.begin();
		it != byFolder.end(); ++it)
	{
		if (it->second.name.empty()) continue; // no project.json - not an example
		found.push_back(it->second);
	}

	for (size_t i = 0; i < found.size(); ++i)
	{
		if (cancel_.load()) break;
		const std::string folder = found[i].folder;

		// Only fetch the two optional description files when the listing
		// already says they exist - no speculative 404s.
		bool demoJson = false, readme = false;
		for (size_t b = 0; b < blobs.size(); ++b)
		{
			if (blobs[b].path == folder + "/demo.json") demoJson = true;
			else if (blobs[b].path == folder + "/README.md") readme = true;
		}

		if (demoJson)
		{
			std::string txt, e2;
			long c2 = 0;
			if (HttpGet(RawUrl(src, folder + "/demo.json"), false, txt, c2, e2, &cancel_, NULL))
			{
				try
				{
					const json j = json::parse(txt);
					const std::string n = j.value("name", std::string());
					if (!n.empty()) found[i].name = n;
					found[i].description = j.value("description", std::string());
					found[i].author = j.value("author", std::string());
					const json tags = j.value("tags", json::array());
					for (json::const_iterator t = tags.begin(); t != tags.end(); ++t)
						if (t->is_string()) found[i].tags.push_back(t->get<std::string>());
				}
				catch (...) { /* a broken demo.json is not worth failing over */ }
			}
		}
		if (found[i].description.empty() && readme)
		{
			std::string txt, e2;
			long c2 = 0;
			if (HttpGet(RawUrl(src, folder + "/README.md"), false, txt, c2, e2, &cancel_, NULL))
				found[i].description = FirstReadmeParagraph(txt);
		}
	}

	{
		std::lock_guard<std::mutex> lk(mutex_);
		pendingEntries_ = found;
		pendingEntriesValid_ = true;
		if (truncated)
			pendingCatalogError_ = "GitHub truncated the file listing for this repo - "
				"some examples may be missing.";
	}
	catalogState_ = CatalogState::Ready;

	// Images last, and published one at a time: the list is useful without
	// them and should not wait on a download per example.
	const std::string cacheDir = CacheDirectory();
	for (size_t i = 0; i < found.size() && !cacheDir.empty(); ++i)
	{
		if (cancel_.load()) break;
		if (found[i].thumbnailRepoPath.empty()) continue;

		const std::string sha = thumbSha.count(found[i].folder) ? thumbSha[found[i].folder] : std::string();
		const std::string ext = fs::path(found[i].thumbnailRepoPath).extension().string();
		// The blob sha is in the name, so a changed preview lands in a new
		// file instead of being served from a stale cache entry forever.
		const fs::path cached = fs::path(cacheDir) /
			(src.owner + "_" + src.repo + "_" + found[i].folder + "_" + sha.substr(0, std::min<size_t>(8, sha.size())) + ext);

		std::error_code ec;
		if (!fs::exists(cached, ec))
		{
			std::string bytes, e2;
			long c2 = 0;
			if (!HttpGet(RawUrl(src, found[i].thumbnailRepoPath), false, bytes, c2, e2, &cancel_, NULL))
				continue;
			std::string werr;
			if (!WriteBinaryFile(cached, bytes, werr))
				continue;
		}

		std::lock_guard<std::mutex> lk(mutex_);
		pendingThumbs_.push_back(std::make_pair(found[i].folder, cached.string()));
	}

	workerBusy_ = false;
}

void DemoLibrary::InstallWorker(DemoEntry demo, std::string destinationParentDir)
{
	const DemoSource src = source_;
	const std::string prefix = demo.folder + "/";

	// Re-list rather than carrying the catalogue's blob vector around: the
	// install has to know exactly which files to write, and the listing is
	// one cached-by-GitHub call.
	std::string body, err;
	long code = 0;
	if (!HttpGet(ApiTreeUrl(src), true, body, code, err, &cancel_, NULL))
	{
		std::lock_guard<std::mutex> lk(mutex_);
		pendingInstallError_ = "Could not list the repository: " + err;
		installState_ = InstallState::Failed;
		workerBusy_ = false;
		return;
	}

	std::vector<TreeBlob> files;
	try
	{
		const json j = json::parse(body);
		const json tree = j.value("tree", json::array());
		for (json::const_iterator it = tree.begin(); it != tree.end(); ++it)
		{
			if (it->value("type", std::string()) != "blob") continue;
			if (it->value("mode", std::string()) == "120000") continue;
			TreeBlob b;
			b.path = it->value("path", std::string());
			b.size = it->value("size", (unsigned long long)0);
			if (b.path.rfind(prefix, 0) != 0) continue;
			if (!SafeRepoPath(b.path)) continue;
			files.push_back(b);
		}
	}
	catch (const std::exception& e)
	{
		std::lock_guard<std::mutex> lk(mutex_);
		pendingInstallError_ = std::string("Malformed listing from GitHub: ") + e.what();
		installState_ = InstallState::Failed;
		workerBusy_ = false;
		return;
	}

	if (files.empty())
	{
		std::lock_guard<std::mutex> lk(mutex_);
		pendingInstallError_ = "The example folder '" + demo.folder + "' is empty or gone.";
		installState_ = InstallState::Failed;
		workerBusy_ = false;
		return;
	}
	filesTotal_ = files.size();
	filesDone_ = 0;
	bytesDone_ = 0;

	std::error_code ec;
	const fs::path parent = fs::path(destinationParentDir);
	fs::create_directories(parent, ec);
	if (!fs::is_directory(parent, ec))
	{
		std::lock_guard<std::mutex> lk(mutex_);
		pendingInstallError_ = "Not a directory: " + destinationParentDir;
		installState_ = InstallState::Failed;
		workerBusy_ = false;
		return;
	}

	// Never the same name twice: an install next to an existing copy gets
	// "Starfall (2)" rather than merging into, or overwriting, the project
	// somebody may have been editing.
	fs::path dest = parent / demo.folder;
	for (int n = 2; fs::exists(dest, ec) && n < 100; ++n)
		dest = parent / (demo.folder + " (" + std::to_string(n) + ")");

	// Everything lands here first, so a cancel or a dropped connection
	// leaves no half-project on disk for the user to trip over.
	const fs::path staging = parent / ("." + demo.folder + ".pyros-download");
	fs::remove_all(staging, ec);

	std::string failure;
	for (size_t i = 0; i < files.size(); ++i)
	{
		if (cancel_.load())
		{
			failure = "Cancelled";
			break;
		}
		{
			std::lock_guard<std::mutex> lk(mutex_);
			installFileLabel_ = files[i].path;
		}

		std::string bytes, e2;
		long c2 = 0;
		if (!HttpGet(RawUrl(src, files[i].path), false, bytes, c2, e2, &cancel_, &bytesDone_))
		{
			failure = "Failed to download " + files[i].path + ": " + e2;
			break;
		}

		const std::string rel = files[i].path.substr(prefix.size());
		std::string werr;
		if (!WriteBinaryFile(staging / fs::path(rel), bytes, werr))
		{
			failure = werr;
			break;
		}
		filesDone_ = i + 1;
	}

	if (failure.empty())
	{
		// An author's project.json carries their AI Assistant block, API key
		// and all (see ProjectSettings::aiAssistant). Whoever installs the
		// example should get their own defaults, not somebody's credential.
		const fs::path projectJson = staging / "project.json";
		std::ifstream in(projectJson, std::ios::binary);
		if (in)
		{
			std::stringstream ss;
			ss << in.rdbuf();
			in.close();
			try
			{
				json j = json::parse(ss.str());
				if (j.contains("settings") && j["settings"].is_object())
					j["settings"].erase("aiAssistant");
				std::ofstream out(projectJson, std::ios::binary | std::ios::trunc);
				if (out) out << j.dump(4);
			}
			catch (...) { /* leave a project.json we cannot parse alone */ }
		}

		fs::rename(staging, dest, ec);
		if (ec)
			failure = "Could not move the download into place: " + ec.message();
	}

	if (!failure.empty())
	{
		std::error_code rec;
		fs::remove_all(staging, rec);
		std::lock_guard<std::mutex> lk(mutex_);
		pendingInstallError_ = failure;
		installState_ = InstallState::Failed;
		workerBusy_ = false;
		return;
	}

	{
		std::lock_guard<std::mutex> lk(mutex_);
		pendingInstalledProjectJson_ = (dest / "project.json").string();
	}
	installState_ = InstallState::Done;
	workerBusy_ = false;
}
