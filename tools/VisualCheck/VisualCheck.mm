//============================================================================
// Name        : VisualCheck.mm
// Author      : Duarte Peixinho
// Description : Visual regression over the DemoLauncher (macOS). Every demo
//               is run in turn - one window at a time, closed after - a frame
//               captured a few seconds in and held against its baseline.
//============================================================================
//
//   VisualCheck check  [--build build_ed_mtl] [--only 3,7] [--wait 7]
//   VisualCheck accept [--build ...] [--only ...]     (the frames now become the baselines)
//   What each demo printed is kept in $TMPDIR/pyros-visual/NN.log
//
// Baselines: tests/visual/<backend>/<index>_<name>.png, 320x180. A demo fails
// when its picture differs on the whole (mean per-channel difference over
// the frame, --tolerance) or in any one place (the mean over a block of
// 10x10, --block-tolerance). The demo runs on a fixed clock that stops at
// 4 s (PYROS_DEMO_FIXED_CLOCK) and with fixed seeds, so a run is the same
// moment every time. The whole-frame mean alone let one side of a
// lamp's shadow map gone black, or a floor banded with acne, pass - a few
// percent of the frame. A demo that failed leaves its frame and a map of
// where it differs in $TMPDIR/pyros-visual.
//
// It replaces tools/visual_regress.py, which compared 96x54 blurred
// thumbnails of the frame's right three quarters.

#import <Foundation/Foundation.h>
#import <CoreGraphics/CoreGraphics.h>
#import <ImageIO/ImageIO.h>
#include <dlfcn.h>
#include <signal.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <regex>
#include <set>
#include <string>
#include <vector>
#include <Pyros3D/Utils/Json/json.hpp>
#define STB_IMAGE_IMPLEMENTATION
#include <Pyros3D/Ext/stb/stb_image.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <Pyros3D/Ext/stb/stb_image_write.h>

extern char** environ;

namespace {

	const int kW = 320, kH = 180, kBlock = 10;
	const int kWindowW = 1280, kWindowH = 748;
	const double kFreezeSeconds = 4.0;

	struct Options
	{
		std::string mode, build, only;
		double wait = 7.0, tolerance = 3.0, blockTolerance = 30.0;
	};

	std::string Run(const std::string &cmd)
	{
		std::string out;
		FILE* p = popen(cmd.c_str(), "r");
		if (!p) return out;
		char buf[4096];
		size_t n;
		while ((n = fread(buf, 1, sizeof(buf), p)) > 0) out.append(buf, n);
		pclose(p);
		return out;
	}

	// The demo's largest on-screen-layer window: its number and size in points.
	bool FindWindow(const pid_t pid, CGWindowID &number, CGRect &bounds)
	{
		bool found = false;
		double best = 0.0;
		CFArrayRef list = CGWindowListCopyWindowInfo(kCGWindowListOptionAll, kCGNullWindowID);
		if (!list) return false;
		for (CFIndex i = 0; i < CFArrayGetCount(list); i++)
		{
			NSDictionary* w = (__bridge NSDictionary*)CFArrayGetValueAtIndex(list, i);
			if ([w[(id)kCGWindowOwnerPID] intValue] != pid || [w[(id)kCGWindowLayer] intValue] != 0) continue;
			CGRect b;
			if (!CGRectMakeWithDictionaryRepresentation((__bridge CFDictionaryRef)w[(id)kCGWindowBounds], &b) || b.size.height <= 200) continue;
			const double area = b.size.width * b.size.height;
			if (area > best) { best = area; number = [w[(id)kCGWindowNumber] unsignedIntValue]; bounds = b; found = true; }
		}
		CFRelease(list);
		return found;
	}

	// The window floated and sized (yabai, where it tiles windows; System Events otherwise): a tiling
	// manager hands a new window a different tile each launch, and the picture is framed by the window.
	void FixSize(const pid_t pid, const int w, const int h)
	{
		const std::string q = Run("yabai -m query --windows 2>/dev/null");
		if (!q.empty())
		{
			nlohmann::json wins = nlohmann::json::parse(q, nullptr, false);
			if (wins.is_array())
				for (const nlohmann::json &wi : wins)
				{
					if (wi.value("pid", -1) != pid) continue;
					const std::string id = std::to_string(wi.value("id", 0));
					if (!wi.value("is-floating", false)) Run("yabai -m window " + id + " --toggle float 2>/dev/null");
					Run("yabai -m window " + id + " --move abs:40:60 2>/dev/null");
					Run("yabai -m window " + id + " --resize abs:" + std::to_string(w) + ":" + std::to_string(h) + " 2>/dev/null");
					return;
				}
		}
		Run("osascript -e 'tell application \"System Events\" to tell (first process whose unix id is " + std::to_string(pid)
			+ ") to set size of window 1 to {" + std::to_string(w) + ", " + std::to_string(h) + "}' >/dev/null 2>&1");
	}

	// The window's picture, without its title bar, scaled to kW x kH (RGB). CGWindowListCreateImage is
	// obsolete in the macOS 15 SDK but still there at run time; asked for by name.
	bool Capture(const CGWindowID number, const CGRect &bounds, std::vector<unsigned char> &rgb, CGImageRef* full)
	{
		typedef CGImageRef (*CreateImage)(CGRect, CGWindowListOption, CGWindowID, CGWindowImageOption);
		static CreateImage create = (CreateImage)dlsym(RTLD_DEFAULT, "CGWindowListCreateImage");
		if (!create) { fprintf(stderr, "CGWindowListCreateImage is not available\n"); return false; }
		CGImageRef img = create(CGRectNull, kCGWindowListOptionIncludingWindow, number, kCGWindowImageBoundsIgnoreFraming);
		if (!img) return false;
		const size_t W = CGImageGetWidth(img), H = CGImageGetHeight(img);
		// (the title bar: 28 points of the window's height, in the image's pixels)
		const size_t bar = (size_t)std::lround(28.0 * (double)H / std::max(1.0, (double)bounds.size.height));
		CGImageRef body = CGImageCreateWithImageInRect(img, CGRectMake(0, bar, W, H > bar ? H - bar : H));
		std::vector<unsigned char> rgba((size_t)kW * kH * 4);
		CGColorSpaceRef cs = CGColorSpaceCreateDeviceRGB();
		CGContextRef ctx = CGBitmapContextCreate(rgba.data(), kW, kH, 8, kW * 4, cs, (CGBitmapInfo)kCGImageAlphaNoneSkipLast);
		CGContextSetInterpolationQuality(ctx, kCGInterpolationHigh);
		CGContextDrawImage(ctx, CGRectMake(0, 0, kW, kH), body);
		CGContextRelease(ctx);
		CGColorSpaceRelease(cs);
		CGImageRelease(body);
		if (full) *full = img; else CGImageRelease(img);
		rgb.resize((size_t)kW * kH * 3);
		for (size_t i = 0; i < (size_t)kW * kH; i++)
			for (int c = 0; c < 3; c++) rgb[i * 3 + c] = rgba[i * 4 + c];
		return true;
	}

	void WriteCGImage(CGImageRef img, const std::string &path)
	{
		CFURLRef url = CFURLCreateFromFileSystemRepresentation(NULL, (const UInt8*)path.c_str(), (CFIndex)path.size(), false);
		CGImageDestinationRef d = CGImageDestinationCreateWithURL(url, CFSTR("public.png"), 1, NULL);
		if (d) { CGImageDestinationAddImage(d, img, NULL); CGImageDestinationFinalize(d); CFRelease(d); }
		CFRelease(url);
	}

	struct Difference { double mean = 0.0, worstBlock = 0.0; int worstX = 0, worstY = 0; };

	// The frame against the baseline: the mean per-channel difference over the frame, and the largest
	// over any one block. Each picture is softened by a 3x3 box first - a texel's shimmer is not news.
	Difference Compare(const std::vector<unsigned char> &a, const std::vector<unsigned char> &b, std::vector<unsigned char>* map)
	{
		auto soften = [](const std::vector<unsigned char> &in) {
			std::vector<float> out(in.size());
			for (int y = 0; y < kH; y++)
				for (int x = 0; x < kW; x++)
					for (int c = 0; c < 3; c++)
					{
						float s = 0.f; int n = 0;
						for (int dy = -1; dy <= 1; dy++)
							for (int dx = -1; dx <= 1; dx++)
							{
								const int xx = x + dx, yy = y + dy;
								if (xx < 0 || yy < 0 || xx >= kW || yy >= kH) continue;
								s += in[((size_t)yy * kW + xx) * 3 + c]; n++;
							}
						out[((size_t)y * kW + x) * 3 + c] = s / (float)n;
					}
			return out;
		};
		const std::vector<float> sa = soften(a), sb = soften(b);
		Difference d;
		double total = 0.0;
		std::vector<double> blocks((kW / kBlock) * (kH / kBlock), 0.0);
		for (int y = 0; y < kH; y++)
			for (int x = 0; x < kW; x++)
			{
				double e = 0.0;
				for (int c = 0; c < 3; c++) e += std::fabs(sa[((size_t)y * kW + x) * 3 + c] - sb[((size_t)y * kW + x) * 3 + c]);
				e /= 3.0;
				total += e;
				if (x / kBlock < kW / kBlock && y / kBlock < kH / kBlock) blocks[(y / kBlock) * (kW / kBlock) + x / kBlock] += e;
				if (map) { const unsigned char v = (unsigned char)std::min(255.0, e * 4.0); (*map)[((size_t)y * kW + x) * 3 + 0] = v; (*map)[((size_t)y * kW + x) * 3 + 1] = v / 4; (*map)[((size_t)y * kW + x) * 3 + 2] = v / 4; }
			}
		d.mean = total / ((double)kW * kH);
		for (size_t k = 0; k < blocks.size(); k++)
		{
			const double m = blocks[k] / (double)(kBlock * kBlock);
			if (m > d.worstBlock) { d.worstBlock = m; d.worstX = (int)(k % (kW / kBlock)) * kBlock; d.worstY = (int)(k / (kW / kBlock)) * kBlock; }
		}
		return d;
	}

	bool ParseArgs(int argc, char** argv, Options &o)
	{
		if (argc < 2) return false;
		o.mode = argv[1];
		if (o.mode != "check" && o.mode != "accept") return false;
		for (int i = 2; i < argc; i++)
		{
			const std::string a = argv[i];
			auto next = [&](std::string &v) { if (i + 1 >= argc) return false; v = argv[++i]; return true; };
			std::string v;
			if (a == "--build" && next(v)) o.build = v;
			else if (a == "--only" && next(v)) o.only = v;
			else if (a == "--wait" && next(v)) o.wait = atof(v.c_str());
			else if (a == "--tolerance" && next(v)) o.tolerance = atof(v.c_str());
			else if (a == "--block-tolerance" && next(v)) o.blockTolerance = atof(v.c_str());
			else return false;
		}
		return true;
	}

	double Now() { return [NSDate timeIntervalSinceReferenceDate]; }
}

int main(int argc, char** argv)
{
	@autoreleasepool
	{
		Options o;
		if (!ParseArgs(argc, argv, o))
		{
			fprintf(stderr, "VisualCheck check|accept [--build build_ed_mtl] [--only 3,7] [--wait 7] [--tolerance 3] [--block-tolerance 30]\n");
			return 2;
		}
		const std::string repo = PYROS_REPO_DIR;
		if (o.build.empty()) o.build = repo + "/build_ed_mtl";
		if (o.build[0] != '/') o.build = std::string([[[NSFileManager defaultManager] currentDirectoryPath] UTF8String]) + "/" + o.build;
		const std::string exe = o.build + "/examples/DemoLauncher";
		const std::string leaf = o.build.substr(o.build.find_last_of('/') + 1);
		const std::string backend = leaf.find("mtl") != std::string::npos ? "metal" : (leaf.find("vk") != std::string::npos ? "vulkan" : "gl");

		std::ifstream mf(repo + "/examples/assets/demos/manifest.json");
		nlohmann::json demos = nlohmann::json::parse(mf, nullptr, false);
		if (!demos.is_array()) { fprintf(stderr, "no demo manifest\n"); return 2; }
		std::set<int> only;
		for (size_t s = 0, e; s < o.only.size(); s = e + 1)
		{
			e = o.only.find(',', s); if (e == std::string::npos) e = o.only.size();
			if (e > s) only.insert(atoi(o.only.substr(s, e - s).c_str()));
		}
		const std::string baseDir = repo + "/tests/visual/" + backend;
		const char* tmpEnv = getenv("TMPDIR");
		const std::string tmp = std::string(tmpEnv ? tmpEnv : "/tmp") + "/pyros-visual";
		[[NSFileManager defaultManager] createDirectoryAtPath:@(baseDir.c_str()) withIntermediateDirectories:YES attributes:nil error:nil];
		[[NSFileManager defaultManager] createDirectoryAtPath:@(tmp.c_str()) withIntermediateDirectories:YES attributes:nil error:nil];

		std::vector<int> failed;
		for (int i = 0; i < (int)demos.size(); i++)
		{
			if (!only.empty() && !only.count(i)) continue;
			const std::string title = demos[i].value("name", std::to_string(i));
			std::string name = std::regex_replace(title, std::regex("[^A-Za-z0-9]+"), "_");
			while (!name.empty() && name.back() == '_') name.pop_back();
			while (!name.empty() && name.front() == '_') name.erase(0, 1);
			char idx[8]; snprintf(idx, sizeof(idx), "%02d", i);

			// (the demo alone - PYROS_DEMO_NO_UI: no panels over it, and no input reaching it)
			std::vector<std::string> env;
			for (char** e = environ; *e; e++) env.push_back(*e);
			env.push_back("PYROS_DEMO_INDEX=" + std::to_string(i));
			env.push_back("PYROS_DEMO_NO_UI=1");
			// (one moment of the demo, the same every run: its clock moves a fixed step a frame and stops)
			env.push_back("PYROS_DEMO_FIXED_CLOCK=" + std::to_string(kFreezeSeconds));
			std::vector<char*> envp; for (std::string &s : env) envp.push_back(&s[0]); envp.push_back(NULL);
			posix_spawn_file_actions_t fa;
			posix_spawn_file_actions_init(&fa);
			// (what the demo printed, beside its frames: the clock's "frozen", a crash, a shader error)
			const std::string logPath = tmp + "/" + idx + ".log";
			posix_spawn_file_actions_addopen(&fa, 1, logPath.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
			posix_spawn_file_actions_adddup2(&fa, 1, 2);
			const std::string cwd = repo + "/resources";
			// (the _np name: its successor is only in the macOS 26 SDK, and CI builds with older ones)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
			posix_spawn_file_actions_addchdir_np(&fa, cwd.c_str());
#pragma clang diagnostic pop
			char* args[] = { (char*)exe.c_str(), NULL };
			pid_t pid = 0;
			if (posix_spawn(&pid, exe.c_str(), &fa, NULL, args, envp.data()) != 0)
			{
				printf("%2d %-34s FAIL (could not start %s)\n", i, title.c_str(), exe.c_str());
				failed.push_back(i); posix_spawn_file_actions_destroy(&fa); continue;
			}
			posix_spawn_file_actions_destroy(&fa);
			auto alive = [&]() { int st = 0; return waitpid(pid, &st, WNOHANG) == 0; };

			usleep(2000000);
			// (until it takes: a demo that bakes on the CPU at start has no window yet at 2 s)
			const double sizedAt = Now();
			CGWindowID wid = 0; CGRect bounds = CGRectZero;
			// (exactly the launcher's own window: 1280x720 of picture under a 28 point title bar. A height
			// allowed to vary by a few points let a tiled-then-resized window render 728 rows and an
			// untouched one 720 - the same demo framed two ways, and SSAO's teapots "differed" half the runs)
			for (int attempt = 0; attempt < 12; attempt++)
			{
				if (FindWindow(pid, wid, bounds) && (int)bounds.size.width == kWindowW && (int)bounds.size.height == kWindowH) break;
				FixSize(pid, kWindowW, kWindowH);
				if (!alive()) break;
				usleep(500000);
			}
			// Until the demo's clock has stopped (it says so), then a little more for what accumulates over
			// frames (temporal antialiasing) to settle. --wait is the most to wait for it.
			(void)sizedAt;
			bool frozen = false;
			for (const double until = Now() + std::max(o.wait, 5.0) + 25.0; Now() < until && alive(); usleep(100000))
			{
				std::ifstream lf(logPath);
				std::string line;
				while (std::getline(lf, line)) if (line.find("[check] frozen") != std::string::npos) { frozen = true; break; }
				if (frozen) break;
			}
			usleep(1500000);
			if (!frozen && alive()) printf("%2d %-34s (its clock never stopped - captured anyway)\n", i, title.c_str());

			std::vector<unsigned char> rgb;
			CGImageRef full = NULL;
			const bool shown = FindWindow(pid, wid, bounds) && Capture(wid, bounds, rgb, &full);
			const bool stillAlive = alive();
			kill(pid, SIGKILL);
			int st = 0; waitpid(pid, &st, 0);
			if (!shown || !stillAlive)
			{
				printf("%2d %-34s FAIL (%s)\n", i, title.c_str(), !stillAlive ? "crashed" : "no window");
				failed.push_back(i);
				if (full) CGImageRelease(full);
				continue;
			}
			const std::string base = baseDir + "/" + idx + "_" + name + ".png";
			if (o.mode == "accept")
			{
				stbi_write_png(base.c_str(), kW, kH, 3, rgb.data(), kW * 3);
				printf("%2d %-34s baseline saved\n", i, title.c_str());
				CGImageRelease(full);
				continue;
			}
			int bw = 0, bh = 0, bc = 0;
			unsigned char* bp = stbi_load(base.c_str(), &bw, &bh, &bc, 3);
			if (!bp || bw != kW || bh != kH)
			{
				printf("%2d %-34s no baseline at %dx%d (run accept)\n", i, title.c_str(), kW, kH);
				if (bp) stbi_image_free(bp);
				CGImageRelease(full);
				continue;
			}
			const std::vector<unsigned char> baseline(bp, bp + (size_t)kW * kH * 3);
			stbi_image_free(bp);
			std::vector<unsigned char> map((size_t)kW * kH * 3);
			const Difference d = Compare(rgb, baseline, &map);
			// (a demo whose picture moves by itself - an animation, particles, things falling - says how
			// much it may differ in the manifest: "check": {"tolerance", "blockTolerance"})
			double tolerance = o.tolerance, blockTolerance = o.blockTolerance;
			if (demos[i].contains("check") && demos[i]["check"].is_object())
			{
				tolerance = std::max(tolerance, demos[i]["check"].value("tolerance", tolerance));
				blockTolerance = std::max(blockTolerance, demos[i]["check"].value("blockTolerance", blockTolerance));
			}
			const bool bad = d.mean > tolerance || d.worstBlock > blockTolerance;
			printf("%2d %-34s %s  (mean %.1f, worst block %.1f at %d,%d)\n", i, title.c_str(), bad ? "DIFFERS" : "ok", d.mean, d.worstBlock, d.worstX, d.worstY);
			if (bad)
			{
				failed.push_back(i);
				stbi_write_png((tmp + "/" + idx + "_now.png").c_str(), kW, kH, 3, rgb.data(), kW * 3);
				stbi_write_png((tmp + "/" + idx + "_where.png").c_str(), kW, kH, 3, map.data(), kW * 3);
				WriteCGImage(full, tmp + "/" + idx + "_full.png");
			}
			CGImageRelease(full);
			fflush(stdout);
		}
		if (o.mode == "check")
		{
			std::string list;
			for (size_t k = 0; k < failed.size(); k++) list += (k ? "," : "") + std::to_string(failed[k]);
			printf("%zu differ: %s%s\n", failed.size(), failed.empty() ? "none" : list.c_str(), failed.empty() ? "" : ("  (frames in " + tmp + ")").c_str());
		}
		return (o.mode == "check" && !failed.empty()) ? 1 : 0;
	}
}
