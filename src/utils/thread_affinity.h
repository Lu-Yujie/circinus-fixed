#pragma once

#include <errno.h>
#include <pthread.h>
#include <sched.h>
#include <thread>
#include <vector>

namespace circinus {

/** Pins thread i to the i-th CPU that the process may run on (sched_getaffinity), so that taskset and cgroup cpusets
 * are respected; with more threads than allowed CPUs the CPUs are reused in turn. Pinning thread i to CPU i instead
 * ignored the allowed set: concurrent processes all used CPUs 0..n-1, and a thread pinned outside the set stayed
 * unpinned (the error was ignored). */
inline bool pinToCores(std::vector<std::thread>& threads) {
  cpu_set_t allowed;
  CPU_ZERO(&allowed);
  if (sched_getaffinity(0, sizeof(cpu_set_t), &allowed) != 0) {
    return false;
  }
  std::vector<int> cpus;
  for (int cpu = 0; cpu < CPU_SETSIZE; ++cpu) {
    if (CPU_ISSET(cpu, &allowed)) cpus.push_back(cpu);
  }
  if (cpus.empty()) return false;
  cpu_set_t cpuset;
  for (uint32_t i = 0; i < threads.size(); ++i) {
    CPU_ZERO(&cpuset);
    CPU_SET(cpus[i % cpus.size()], &cpuset);
    int rc = pthread_setaffinity_np(threads[i].native_handle(), sizeof(cpu_set_t), &cpuset);
    if (rc != 0) {
      return false;
    }
  }
  return true;
}

}  // namespace circinus
