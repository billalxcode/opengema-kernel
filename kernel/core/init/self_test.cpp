#include "og/boot/init.h"

#include "memory.h"
#include "og/arch/registers.h"
#include "og/core/log.h"
#include "og/core/panic.h"
#include "og/core/status.h"
#include "og/events/workqueue.h"
#include "og/io/print.h"
#include "og/mem/vmm.h"
#include "og/platform/smp.h"
#include "og/sched/sched.h"
#include "og/sync/sync.h"
#include "og/time/timer.h"

namespace og::boot {

namespace {

constexpr uint64_t kTargetTicks = 10;
constexpr uint64_t kTimeoutSeconds = 20;
constexpr uint64_t kAddressSpaceTestVa = mem::kUserSpaceBase + 0x3000000ULL;

uint64_t g_work_runs = 0;
uint64_t g_workers_done = 0;
uint64_t g_worker_cpu_mask = 0;
uint64_t g_mutex_counter = 0;
uint64_t g_mutex_done = 0;
uint64_t g_semaphore_seen = 0;
uint64_t g_once_runs = 0;
uint64_t g_timeout_fired = 0;
sync::Mutex g_test_mutex;
sync::Semaphore g_test_semaphore;
sync::Mutex g_cond_mutex;
sync::ConditionVariable g_cond_var;
sync::RwLock g_test_rwlock;
uint32_t g_cond_ready = 0;
uint32_t g_cond_seen = 0;
sync::Semaphore g_ping_sem;
sync::Semaphore g_pong_sem;
uint32_t g_ping_count = 0;
uint32_t g_pong_count = 0;
uint32_t g_ping_done = 0;
uint32_t g_pong_done = 0;

struct AddressSpaceTestState {
  uint64_t expected;
  uint64_t observed;
  uint32_t done;
};

mem::AddressSpace g_as_a;
mem::AddressSpace g_as_b;
AddressSpaceTestState g_as_state_a{};
AddressSpaceTestState g_as_state_b{};

void CountWork(void* /*context*/) noexcept {
  ++g_work_runs;
}

void MutexWorker(void* /*argument*/) noexcept {
  for (uint32_t index = 0; index < 50U; ++index) {
    if (IsErr(g_test_mutex.lock())) {
      break;
    }
    const uint64_t value = g_mutex_counter;
    g_mutex_counter = value + 1U;
    g_test_mutex.unlock();
    (void)sched::ThreadSleep(1);
  }
  (void)__atomic_add_fetch(&g_mutex_done, 1U, __ATOMIC_RELAXED);
}

void SemaphoreWorker(void* /*argument*/) noexcept {
  (void)g_test_semaphore.wait();
  (void)__atomic_add_fetch(&g_semaphore_seen, 1U, __ATOMIC_RELAXED);
}

void CondWorker(void* /*argument*/) noexcept {
  if (IsErr(g_cond_mutex.lock())) {
    return;
  }
  while (g_cond_ready == 0U) {
    g_cond_var.wait(g_cond_mutex);
  }
  g_cond_mutex.unlock();
  __atomic_store_n(&g_cond_seen, 1U, __ATOMIC_RELEASE);
}

void PingWorker(void* /*argument*/) noexcept {
  for (uint32_t index = 0; index < 200U; ++index) {
    (void)g_ping_sem.wait();
    (void)__atomic_add_fetch(&g_ping_count, 1U, __ATOMIC_RELAXED);
    g_pong_sem.signal();
  }
  (void)__atomic_add_fetch(&g_ping_done, 1U, __ATOMIC_RELEASE);
}

void PongWorker(void* /*argument*/) noexcept {
  for (uint32_t index = 0; index < 200U; ++index) {
    (void)g_pong_sem.wait();
    (void)__atomic_add_fetch(&g_pong_count, 1U, __ATOMIC_RELAXED);
    g_ping_sem.signal();
  }
  (void)__atomic_add_fetch(&g_pong_done, 1U, __ATOMIC_RELEASE);
}

void OnceHandler(void* /*argument*/) noexcept {
  (void)__atomic_add_fetch(&g_once_runs, 1U, __ATOMIC_RELAXED);
}

void TimeoutHandler(void* /*argument*/) noexcept {
  (void)__atomic_add_fetch(&g_timeout_fired, 1U, __ATOMIC_RELAXED);
}

void WorkerA(void* /*argument*/) noexcept {
  for (uint32_t index = 0; index < 3U; ++index) {
    (void)__atomic_or_fetch(&g_worker_cpu_mask,
                            1ULL << smp::CurrentIndex(), __ATOMIC_RELAXED);
    (void)PrintLn("sched: worker");
    (void)sched::ThreadSleep(3);
  }
  (void)__atomic_add_fetch(&g_workers_done, 1U, __ATOMIC_RELAXED);
}

void AddressSpaceWorker(void* argument) noexcept {
  auto* state = static_cast<AddressSpaceTestState*>(argument);
  uint64_t observed = 0;
  mem::Copy(&observed, reinterpret_cast<const void*>(kAddressSpaceTestVa),
            sizeof(observed));
  state->observed = observed;
  __atomic_store_n(&state->done, 1U, __ATOMIC_RELEASE);
}

[[nodiscard]] bool ReclaimThread(sched::Thread* thread) noexcept {
  if (thread == nullptr) {
    return false;
  }
  for (uint32_t waits = 0; waits < 1000U; ++waits) {
    if (sched::ThreadFinished(thread) &&
        sched::ThreadCurrentOnCpu(thread->home_cpu) != thread) {
      return IsOk(sched::ThreadReclaim(thread));
    }
    (void)sched::ThreadSleep(1);
  }
  return false;
}

}  // namespace

void RunTimerSelfTest() noexcept {
  (void)events::ScheduleWork(&CountWork, nullptr);
  arch::aarch64::EnableIrq();

  // Bounded wait so a missing interrupt reports instead of hanging.
  const uint64_t start = arch::aarch64::ReadCntpctEl0();
  const uint64_t timeout = arch::aarch64::ReadCntfrqEl0() * kTimeoutSeconds;
  while (time::TimerTicks() < kTargetTicks) {
    if ((arch::aarch64::ReadCntpctEl0() - start) > timeout) {
      break;
    }
  }
  arch::aarch64::DisableIrq();

  const uint64_t ticks = time::TimerTicks();
  LogDec(LogLevel::kInfo, "timer: ticks", ticks);
  LogDec(LogLevel::kInfo, "timer: work runs", g_work_runs);
  if (ticks == 0U) {
    Log(LogLevel::kError, "timer: no interrupts received\n");
    Panic("timer: irq not working");
  }
  if (ticks < kTargetTicks) {
    Log(LogLevel::kWarn, "timer: few ticks under host load\n");
  } else {
    Log(LogLevel::kInfo, "timer: periodic tick ok\n");
  }
}

void RunSchedulerSelfTest() noexcept {
  const uint32_t online = smp::OnlineCount();
  if (online == 0U || online > 8U) {
    Panic("sched: invalid cpu count");
  }
  sched::Thread* workers[8]{};
  for (uint32_t cpu = 0; cpu < online; ++cpu) {
    const Result<sched::Thread*> worker =
      sched::ThreadCreateOn("worker", &WorkerA, nullptr, cpu);
    if (worker.is_err()) {
      Panic("sched: create worker failed");
    }
    workers[cpu] = worker.value();
  }
  LogDec(LogLevel::kInfo, "sched: threads", sched::ThreadCount());

  arch::aarch64::DisableIrq();
  sched::SchedulerStart();
  arch::aarch64::EnableIrq();

  uint32_t waits = 0;
  while (__atomic_load_n(&g_workers_done, __ATOMIC_ACQUIRE) < online &&
         waits < 2000U) {
    (void)sched::ThreadSleep(1);
    ++waits;
  }
  LogDec(LogLevel::kInfo, "sched: workers done",
         __atomic_load_n(&g_workers_done, __ATOMIC_ACQUIRE));
  if (__atomic_load_n(&g_workers_done, __ATOMIC_ACQUIRE) < online) {
    Panic("sched: workers did not finish");
  }
  const uint64_t worker_cpus =
    __atomic_load_n(&g_worker_cpu_mask, __ATOMIC_ACQUIRE);
  LogDec(LogLevel::kInfo, "smp: worker cpus", worker_cpus);
  if (online < 64U) {
    const uint64_t expected = (1ULL << online) - 1ULL;
    if ((worker_cpus & expected) != expected) {
      Panic("sched: workers stayed on one cpu");
    }
  }
  Log(LogLevel::kInfo, "sched: round robin ok\n");

  g_test_mutex.set_name("self-test");
  g_test_semaphore.init(0);
  const Result<sched::Thread*> mutex_a =
    sched::ThreadCreate("mutex-a", &MutexWorker, nullptr);
  const Result<sched::Thread*> mutex_b =
    sched::ThreadCreate("mutex-b", &MutexWorker, nullptr);
  const Result<sched::Thread*> semaphore =
    sched::ThreadCreate("semaphore", &SemaphoreWorker, nullptr);
  if (mutex_a.is_err() || mutex_b.is_err() || semaphore.is_err()) {
    Panic("sync: thread create failed");
  }
  g_test_semaphore.signal();

  uint32_t sync_waits = 0;
  while ((__atomic_load_n(&g_mutex_done, __ATOMIC_ACQUIRE) < 2U ||
          __atomic_load_n(&g_semaphore_seen, __ATOMIC_ACQUIRE) < 1U) &&
         sync_waits < 5000U) {
    (void)sched::ThreadSleep(1);
    ++sync_waits;
  }
  if (g_mutex_counter != 100U) {
    Panic("sync: mutex lost updates");
  }
  if (__atomic_load_n(&g_semaphore_seen, __ATOMIC_ACQUIRE) != 1U) {
    Panic("sync: semaphore failed");
  }

  sync::RefCount refs;
  refs.reset(1);
  if (refs.add(2) != 3U) {
    Panic("sync: refcount add failed");
  }
  if (refs.release() || refs.release()) {
    Panic("sync: refcount released early");
  }
  if (!refs.release()) {
    Panic("sync: refcount did not reach zero");
  }

  sync::OnceFlag once{};
  sync::CallOnce(once, &OnceHandler, nullptr);
  sync::CallOnce(once, &OnceHandler, nullptr);
  if (__atomic_load_n(&g_once_runs, __ATOMIC_ACQUIRE) != 1U) {
    Panic("sync: once ran twice");
  }
  Log(LogLevel::kInfo, "sync: mutex, semaphore, refcount, once ok\n");

  if (!ReclaimThread(mutex_a.value()) || !ReclaimThread(mutex_b.value()) ||
      !ReclaimThread(semaphore.value())) {
    Panic("sync: thread reclaim failed");
  }

  const Result<time::Timeout*> scheduled_timeout =
    time::ScheduleAfter(5, &TimeoutHandler, nullptr);
  if (scheduled_timeout.is_err()) {
    Panic("time: schedule failed");
  }
  uint32_t timeout_waits = 0;
  while (__atomic_load_n(&g_timeout_fired, __ATOMIC_ACQUIRE) == 0U &&
         timeout_waits < 500U) {
    (void)sched::ThreadSleep(1);
    ++timeout_waits;
  }
  if (__atomic_load_n(&g_timeout_fired, __ATOMIC_ACQUIRE) != 1U) {
    Panic("time: timeout did not fire");
  }
  Log(LogLevel::kInfo, "time: timeout ok\n");

  for (uint32_t cpu = 0; cpu < online; ++cpu) {
    if (!ReclaimThread(workers[cpu])) {
      Panic("sched: worker reclaim failed");
    }
  }

  const Result<sched::Thread*> cond_worker =
    sched::ThreadCreate("cond", &CondWorker, nullptr);
  if (cond_worker.is_err()) {
    Panic("sync: cond create failed");
  }
  (void)sched::ThreadSleep(5);
  if (IsErr(g_cond_mutex.lock())) {
    Panic("sync: cond lock failed");
  }
  g_cond_ready = 1U;
  g_cond_var.signal();
  g_cond_mutex.unlock();
  uint32_t cond_waits = 0;
  while (__atomic_load_n(&g_cond_seen, __ATOMIC_ACQUIRE) == 0U &&
         cond_waits < 1000U) {
    (void)sched::ThreadSleep(1);
    ++cond_waits;
  }
  if (__atomic_load_n(&g_cond_seen, __ATOMIC_ACQUIRE) == 0U) {
    Panic("sync: cond timeout");
  }
  if (!ReclaimThread(cond_worker.value())) {
    Panic("sync: cond reclaim failed");
  }

  g_test_rwlock.lock_read();
  g_test_rwlock.unlock_read();
  g_test_rwlock.lock_write();
  g_test_rwlock.unlock_write();

  uint64_t max_overshoot = 0;
  for (uint32_t index = 0; index < 20U; ++index) {
    const uint64_t started = time::TimeNowMs();
    (void)sched::ThreadSleep(5);
    const uint64_t elapsed = time::TimeNowMs() - started;
    if (elapsed > 5U && (elapsed - 5U) > max_overshoot) {
      max_overshoot = elapsed - 5U;
    }
  }
  LogDec(LogLevel::kInfo, "sched: sleep max overshoot", max_overshoot);
  if (max_overshoot > 1000U) {
    Panic("sched: sleep latency too high");
  }

  g_ping_sem.init(1);
  g_pong_sem.init(0);
  const uint32_t ping_cpu = 0;
  const uint32_t pong_cpu = online > 1U ? 1U : 0U;
  const Result<sched::Thread*> ping =
    sched::ThreadCreateOn("ping", &PingWorker, nullptr, ping_cpu);
  const Result<sched::Thread*> pong =
    sched::ThreadCreateOn("pong", &PongWorker, nullptr, pong_cpu);
  if (ping.is_err() || pong.is_err()) {
    Panic("sync: ping-pong create failed");
  }
  uint32_t ping_waits = 0;
  while ((__atomic_load_n(&g_ping_done, __ATOMIC_ACQUIRE) == 0U ||
          __atomic_load_n(&g_pong_done, __ATOMIC_ACQUIRE) == 0U) &&
         ping_waits < 2000U) {
    (void)sched::ThreadSleep(1);
    ++ping_waits;
  }
  if (__atomic_load_n(&g_ping_done, __ATOMIC_ACQUIRE) == 0U ||
      __atomic_load_n(&g_pong_done, __ATOMIC_ACQUIRE) == 0U ||
      __atomic_load_n(&g_ping_count, __ATOMIC_ACQUIRE) != 200U ||
      __atomic_load_n(&g_pong_count, __ATOMIC_ACQUIRE) != 200U) {
    Panic("sync: ping-pong lost wakeup");
  }
  if (!ReclaimThread(ping.value()) || !ReclaimThread(pong.value())) {
    Panic("sync: ping-pong reclaim failed");
  }
  Log(LogLevel::kInfo, "sched: ping-pong ok\n");
}

void RunAddressSpaceSelfTest() noexcept {
  Status status = mem::AddressSpaceInit(g_as_a);
  if (IsErr(status)) {
    Panic("mem: address space a init failed");
  }
  status = mem::AddressSpaceInit(g_as_b);
  if (IsErr(status)) {
    Panic("mem: address space b init failed");
  }

  constexpr uint64_t kValueA = 0x1122334455667788ULL;
  constexpr uint64_t kValueB = 0x99AABBCCDDEEFF00ULL;
  status = mem::VmmMapUser(g_as_a, kAddressSpaceTestVa, 4096, false);
  if (IsErr(status)) {
    Panic("mem: address space a map failed");
  }
  status = mem::VmmMapUser(g_as_b, kAddressSpaceTestVa, 4096, false);
  if (IsErr(status)) {
    Panic("mem: address space b map failed");
  }
  status = mem::VmmCopyOut(g_as_a, kAddressSpaceTestVa, &kValueA, sizeof(kValueA));
  if (IsErr(status)) {
    Panic("mem: address space a copy failed");
  }
  status = mem::VmmCopyOut(g_as_b, kAddressSpaceTestVa, &kValueB, sizeof(kValueB));
  if (IsErr(status)) {
    Panic("mem: address space b copy failed");
  }

  g_as_state_a.expected = kValueA;
  g_as_state_a.done = 0;
  g_as_state_b.expected = kValueB;
  g_as_state_b.done = 0;

  const Result<sched::Thread*> thread_a =
    sched::ThreadCreateIn("as-a", &AddressSpaceWorker, &g_as_state_a, &g_as_a);
  const Result<sched::Thread*> thread_b =
    sched::ThreadCreateIn("as-b", &AddressSpaceWorker, &g_as_state_b, &g_as_b);
  if (thread_a.is_err() || thread_b.is_err()) {
    Panic("mem: address space thread create failed");
  }

  uint32_t waits = 0;
  while ((__atomic_load_n(&g_as_state_a.done, __ATOMIC_ACQUIRE) == 0U ||
          __atomic_load_n(&g_as_state_b.done, __ATOMIC_ACQUIRE) == 0U) &&
         waits < 1000U) {
    (void)sched::ThreadSleep(1);
    ++waits;
  }
  if (__atomic_load_n(&g_as_state_a.done, __ATOMIC_ACQUIRE) == 0U ||
      __atomic_load_n(&g_as_state_b.done, __ATOMIC_ACQUIRE) == 0U) {
    Panic("mem: address space workers did not finish");
  }
  if (g_as_state_a.observed != kValueA || g_as_state_b.observed != kValueB) {
    Panic("mem: address spaces overlap");
  }

  waits = 0;
  while ((!sched::ThreadFinished(thread_a.value()) ||
          !sched::ThreadFinished(thread_b.value())) &&
         waits < 1000U) {
    (void)sched::ThreadSleep(1);
    ++waits;
  }
  if (!ReclaimThread(thread_a.value()) || !ReclaimThread(thread_b.value())) {
    Panic("mem: address space reclaim failed");
  }
  (void)mem::AddressSpaceDestroy(g_as_a);
  (void)mem::AddressSpaceDestroy(g_as_b);
  Log(LogLevel::kInfo, "mem: address space switch ok\n");
}

}  // namespace og::boot
