// JobSystem: every index is visited exactly once, a job can wait on jobs it
// spawned (the Box3D/Box2D task pattern) without deadlocking, and the
// inline mode behaves the same. Run twice:
//
//   c++ -std=c++17 -I include -I $(pkg-config --variable=includedir freetype2)/freetype2 \
//       tools/tests/job_system.cpp -o /tmp/job_system \
//       -L build_ed_vk -lPyrosEngine -Wl,-rpath,$PWD/build_ed_vk
//   /tmp/job_system && PYROS_JOB_WORKERS=0 /tmp/job_system
#include <Pyros3D/Utils/Jobs/JobSystem.h>

#include <atomic>
#include <cstdio>
#include <vector>

using namespace p3d;

static int failures = 0;
static void check(bool cond, const char* what)
{
	printf("%s  %s\n", cond ? "PASS" : "FAIL", what);
	fflush(stdout);
	if (!cond) failures++;
}

int main()
{
	JobSystem &jobs = JobSystem::Instance();
	printf("workers: %u\n", jobs.WorkerCount());

	for (uint32 n : { 1u, 7u, 1000u, 100003u })
	{
		std::vector<std::atomic<int>> hits(n);
		for (uint32 i = 0; i < n; i++) hits[i] = 0;
		jobs.ParallelFor(n, 16, [&](uint32 b, uint32 e) { for (uint32 i = b; i < e; i++) hits[i]++; });
		bool once = true;
		for (uint32 i = 0; i < n; i++) once = once && hits[i] == 1;
		char what[64];
		snprintf(what, sizeof(what), "ParallelFor(%u) visits each index once", n);
		check(once, what);
	}

	// Many parents, each forking children and waiting on them from inside a
	// job - more parents than workers, so waits must help, not sleep.
	std::atomic<int> leaves(0);
	JobCounter parents;
	for (int p = 0; p < 64; p++)
		jobs.Run([&]()
		{
			JobCounter kids;
			for (int k = 0; k < 32; k++)
				jobs.Run([&]() { leaves++; }, kids);
			jobs.Wait(kids);
		}, parents);
	jobs.Wait(parents);
	check(leaves == 64 * 32, "nested fork/join completes");

	// A counter is reusable once it reaches zero.
	JobCounter c;
	std::atomic<int> sum(0);
	for (int round = 0; round < 3; round++)
	{
		for (int i = 0; i < 100; i++) jobs.Run([&]() { sum++; }, c);
		jobs.Wait(c);
	}
	check(sum == 300 && c.pending == 0, "counter reuse");

	printf("%s (%d failure%s)\n", failures ? "FAILED" : "ALL PASSED", failures, failures == 1 ? "" : "s");
	return failures ? 1 : 0;
}
