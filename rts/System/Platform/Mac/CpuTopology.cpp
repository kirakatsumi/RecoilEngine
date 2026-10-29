/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */

#include "System/Platform/CpuTopology.h"
#include "System/Log/ILog.h"

#include <sys/types.h>
#include <sys/sysctl.h>
#include <string>
#include <vector>
#include <algorithm>
#include <cstdint>

namespace cpu_topology {

static int GetSysctlInt(const char* name) {
	int val = 0;
	size_t size = sizeof(val);
	if (sysctlbyname(name, &val, &size, nullptr, 0) == 0)
		return val;
	return -1;
}

static int64_t GetSysctlInt64(const char* name) {
	int64_t val = 0;
	size_t size = sizeof(val);
	if (sysctlbyname(name, &val, &size, nullptr, 0) == 0)
		return val;
	return -1;
}

static std::string GetSysctlString(const char* name) {
	char buf[128] = {0};
	size_t size = sizeof(buf);
	if (sysctlbyname(name, buf, &size, nullptr, 0) == 0)
		return std::string(buf);
	return "";
}

ThreadPinPolicy GetThreadPinPolicy() {
	// Darwin XNU scheduler manages thread placement dynamically across asymmetric cores.
	return THREAD_PIN_POLICY_NONE;
}

ProcessorMasks GetProcessorMasks() {
	ProcessorMasks processorMasks;

	const int logicalCpus  = GetSysctlInt("hw.logicalcpu");
	const int physicalCpus = GetSysctlInt("hw.physicalcpu");
	const int nperflevels  = GetSysctlInt("hw.nperflevels");

	if (nperflevels > 0) {
		// Apple Silicon heterogeneous topology (Performance & Efficiency cores)
		int currentCpu = 0;
		for (int i = 0; i < nperflevels; ++i) {
			const std::string base = "hw.perflevel" + std::to_string(i);
			const std::string name = GetSysctlString((base + ".name").c_str());
			const int cpus = GetSysctlInt((base + ".logicalcpu").c_str());
			if (cpus <= 0)
				continue;

			uint32_t mask = 0;
			for (int c = 0; c < cpus && (currentCpu + c) < 32; ++c) {
				mask |= (1u << (currentCpu + c));
			}
			currentCpu += cpus;

			if (name == "Performance" || (i == 0 && name.empty())) {
				processorMasks.performanceCoreMask |= mask;
			} else {
				processorMasks.efficiencyCoreMask |= mask;
			}
		}
		// Apple Silicon has no SMT (1 thread per core)
		processorMasks.hyperThreadLowMask = 0;
		processorMasks.hyperThreadHighMask = 0;
	} else {
		// Symmetric topology (Intel x86_64 macOS)
		const int cpus = (logicalCpus > 0) ? std::min(logicalCpus, 32) : 1;
		processorMasks.performanceCoreMask = (cpus >= 32) ? ~0u : ((1u << cpus) - 1);
		processorMasks.efficiencyCoreMask = 0;

		if (logicalCpus > physicalCpus && physicalCpus > 0) {
			// HyperThreading detected: assign alternating low/high SMT masks
			for (int i = 0; i < cpus; i += 2) {
				processorMasks.hyperThreadLowMask |= (1u << i);
				if (i + 1 < cpus)
					processorMasks.hyperThreadHighMask |= (1u << (i + 1));
			}
		}
	}

	return processorMasks;
}

ProcessorCaches GetProcessorCache() {
	ProcessorCaches processorCaches;

	const int logicalCpus = GetSysctlInt("hw.logicalcpu");
	const int nperflevels = GetSysctlInt("hw.nperflevels");

	if (nperflevels > 0) {
		// Apple Silicon: Group by core cluster and assign cluster L2 cache
		int currentCpu = 0;
		for (int i = 0; i < nperflevels; ++i) {
			const std::string base = "hw.perflevel" + std::to_string(i);
			const int cpus = GetSysctlInt((base + ".logicalcpu").c_str());
			const int64_t l2 = GetSysctlInt64((base + ".l2cachesize").c_str());
			const int64_t l3 = GetSysctlInt64((base + ".l3cachesize").c_str());
			if (cpus <= 0)
				continue;

			ProcessorGroupCaches group;
			for (int c = 0; c < cpus && (currentCpu + c) < 32; ++c) {
				group.groupMask |= (1u << (currentCpu + c));
			}
			currentCpu += cpus;

			group.cacheSizes[1] = (l2 > 0) ? static_cast<uint32_t>(l2) : 0;
			// Recoil CPUID sorts by cacheSizes[2] (L3), so populate index 2 with the
			// highest level cache available (L2 on Apple Silicon) so P-cores sort first
			group.cacheSizes[2] = (l3 > 0) ? static_cast<uint32_t>(l3) : group.cacheSizes[1];

			processorCaches.groupCaches.push_back(group);
		}
	} else {
		// Intel Mac: single cache group for all cores
		const int cpus = (logicalCpus > 0) ? std::min(logicalCpus, 32) : 1;
		const int64_t l2 = GetSysctlInt64("hw.l2cachesize");
		const int64_t l3 = GetSysctlInt64("hw.l3cachesize");

		ProcessorGroupCaches group;
		group.groupMask = (cpus >= 32) ? ~0u : ((1u << cpus) - 1);
		group.cacheSizes[1] = (l2 > 0) ? static_cast<uint32_t>(l2) : 0;
		group.cacheSizes[2] = (l3 > 0) ? static_cast<uint32_t>(l3) : 0;

		processorCaches.groupCaches.push_back(group);
	}

	// Sort highest cache size to the front for CPUID/ThreadPool
	std::stable_sort(
		processorCaches.groupCaches.begin(),
		processorCaches.groupCaches.end(),
		[](const auto& lh, const auto& rh) { return lh.cacheSizes[2] > rh.cacheSizes[2]; }
	);

	return processorCaches;
}

} // namespace cpu_topology
