//=============================================================================
// Name        : DemoLibrary.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Browses an "examples" repository on GitHub and installs one
//               of its projects into a local folder (worker thread, libcurl).
//
// There is no catalogue file to keep in sync: every top-level folder of the
// repo that contains a project.json IS an example. Adding one is pushing a
// folder. A folder may describe itself with a demo.json (description, tags,
// author) or just a README.md, and may ship a preview.png - all optional.
//
// Files are fetched individually rather than as an archive. One Git Trees
// call (?recursive=1) returns the whole repo listing - paths, blob sizes and
// shas - and each wanted blob then comes from raw.githubusercontent.com,
// which is CDN-served and outside the API's hourly quota. That downloads one
// example's folder instead of the entire repository (which, with a 100 MB
// example in it, is what a tarball would cost every time), gives real
// per-file progress, and needs neither tar nor a bundled zip decoder.
//=============================================================================

#ifndef DEMOLIBRARY_H
#define	DEMOLIBRARY_H

#include <atomic>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

// Which repository the browser lists. Defaults to the engine's own examples
// repo; overridable in the browser so a team can point at their own.
struct DemoSource {
	std::string owner  = "Peixinho";
	std::string repo   = "pyros3d-examples";
	std::string branch = "main";

	// "owner/repo" or "owner/repo@branch" - what the UI shows and stores.
	std::string Slug() const;
	// Tolerates a full https://github.com/owner/repo URL too, since that is
	// what anyone actually has on their clipboard.
	static DemoSource Parse(const std::string& text);
	bool Valid() const { return !owner.empty() && !repo.empty(); }
};

// One example: a top-level folder of the repo holding a project.json.
struct DemoEntry {
	std::string folder;       // top-level folder name, e.g. "Starfall"
	std::string name;         // demo.json "name", else the folder name
	std::string description;  // demo.json, else the README's first paragraph
	std::string author;
	std::vector<std::string> tags;
	// Repo path of the preview image, empty when the folder ships none.
	std::string thumbnailRepoPath;
	// Local cache file, filled in once the image has been fetched.
	std::string thumbnailFile;
	size_t fileCount = 0;
	unsigned long long totalBytes = 0;
};

class DemoLibrary {
public:

	enum class CatalogState { Idle, Loading, Ready, Failed };
	enum class InstallState { Idle, Running, Done, Failed };

	DemoLibrary();
	~DemoLibrary();
	DemoLibrary(const DemoLibrary&) = delete;
	DemoLibrary& operator=(const DemoLibrary&) = delete;

	// ---------------------------------------------------------------- source
	const DemoSource& Source() const { return source_; }
	// Persists to the support directory and drops the loaded catalogue.
	void SetSource(const DemoSource& s);

	// --------------------------------------------------------------- catalog
	// Starts (or restarts) the listing fetch. Ignored while one is running.
	void Refresh();
	CatalogState Catalog() const { return catalogState_; }
	const std::string& CatalogError() const { return catalogError_; }
	// UI thread only, and only valid between Pump() calls.
	const std::vector<DemoEntry>& Entries() const { return entries_; }

	// --------------------------------------------------------------- install
	// Downloads `demo`'s folder into destinationParentDir/<folder>. The
	// destination is only created once every file has arrived (see the temp
	// directory in the .cpp), so a cancelled or failed download leaves
	// nothing half-written behind.
	void Install(const DemoEntry& demo, const std::string& destinationParentDir);
	void CancelInstall();
	InstallState Installing() const { return installState_; }
	float InstallProgress() const;    // 0..1, 0 when the total is not known yet
	// "24 / 310 files - 12.4 MB" plus the file in flight on a second line.
	std::string InstallStatus() const;
	// Absolute path of the installed project.json - set while state is Done.
	const std::string& InstalledProjectJson() const { return installedProjectJson_; }
	const std::string& InstallError() const { return installError_; }
	// Back to Idle once the UI has consumed a Done/Failed result.
	void ClearInstallResult();

	// Publishes whatever the worker finished. Call once a frame, UI thread.
	void Pump();

	// True when this build can reach the network at all (false on the web
	// build, which has no libcurl - see PYROS_EDITOR_NO_CURL).
	static bool Available();

	// <editor support dir>/demo-cache - downloaded preview images.
	static std::string CacheDirectory();

private:

	void StopWorker();
	void CatalogWorker(DemoSource src);
	void InstallWorker(DemoEntry demo, std::string destinationParentDir);
	void LoadSource();
	void SaveSource() const;
	static std::string SourceFilePath();

	DemoSource source_;

	std::thread worker_;
	std::atomic<bool> workerBusy_{false};
	std::atomic<bool> cancel_{false};

	std::atomic<CatalogState> catalogState_{CatalogState::Idle};
	std::atomic<InstallState> installState_{InstallState::Idle};

	std::vector<DemoEntry> entries_;
	std::string catalogError_;
	std::string installError_;
	std::string installedProjectJson_;

	// Worker -> UI handoff, published by Pump().
	mutable std::mutex mutex_;
	std::vector<DemoEntry> pendingEntries_;
	bool pendingEntriesValid_ = false;
	// (folder, cached file) pairs for thumbnails that arrived after the
	// listing was published - the list must not wait on images.
	std::vector<std::pair<std::string, std::string> > pendingThumbs_;
	std::string pendingCatalogError_;
	std::string pendingInstallError_;
	std::string pendingInstalledProjectJson_;
	std::string installFileLabel_;

	std::atomic<size_t> filesDone_{0};
	std::atomic<size_t> filesTotal_{0};
	std::atomic<unsigned long long> bytesDone_{0};
};

#endif	/* DEMOLIBRARY_H */
