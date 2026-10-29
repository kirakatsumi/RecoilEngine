#ifndef THREAD_AFFINITY_GUARD_H__
#define THREAD_AFFINITY_GUARD_H__

#ifdef _WIN32
#include <windows.h>
#elif defined(__APPLE__)
// Darwin kernel manages thread scheduling dynamically across asymmetric cores.
#include <sys/types.h>
#else
#include <sched.h>
#include <sys/types.h>
#endif

class ThreadAffinityGuard {
private:
#ifdef _WIN32
	DWORD_PTR savedAffinity;
	HANDLE threadHandle;
#elif defined(__APPLE__)
	// No-op on macOS
#else
	cpu_set_t savedAffinity;
	pid_t tid;
#endif
	bool affinitySaved;

public:
	// Constructor: Saves the current thread's affinity
	ThreadAffinityGuard();

	// Destructor: Restores the saved affinity if it was successfully stored
	~ThreadAffinityGuard();

	// Delete copy constructor to prevent copying
	ThreadAffinityGuard(const ThreadAffinityGuard&) = delete;

	// Delete copy assignment operator to prevent assignment
	ThreadAffinityGuard& operator=(const ThreadAffinityGuard&) = delete;
};

#endif
