#include "xtrans/adapters/alsa.hpp"

#include <alsa/asoundlib.h>
#include <pthread.h>

#include <array>
#include <atomic>
#include <cerrno>
#include <cstdio>

#include "xtrans/audio.hpp"

namespace xtrans {
namespace {

constexpr snd_pcm_uframes_t kPeriodFrames = kFrameSamples;
constexpr unsigned kLatencyUs = 100000;
constexpr unsigned kMaxRecoveries = 8;  // per call; a wedged device must fail

const char* alsa_error(int rc) { return snd_strerror(rc); }

}  // namespace

struct AlsaIn::Impl {
  snd_pcm_t* pcm = nullptr;
  std::array<std::int16_t, kPeriodFrames> period{};
  std::array<std::int16_t, kSampleRate> ring_storage{};
  Ring ring{ring_storage};

  std::atomic<bool> stop{false};
  std::atomic<bool> failed{false};
  std::atomic<std::uint32_t> overruns{0};
  pthread_t thread{};
  bool started = false;

  static void* run(void* self) noexcept {
    auto* impl = static_cast<Impl*>(self);
    while (!impl->stop.load(std::memory_order_relaxed)) {
      const snd_pcm_sframes_t n = snd_pcm_readi(impl->pcm, impl->period.data(), kPeriodFrames);
      if (n > 0) {
        impl->ring.push(impl->period.data(), static_cast<std::size_t>(n));
        continue;
      }
      if (n == -EPIPE) {
        impl->overruns.fetch_add(1, std::memory_order_relaxed);
        if (snd_pcm_prepare(impl->pcm) < 0) impl->failed.store(true);
        continue;
      }
      if (n == -EAGAIN) continue;
      if (n == -ESTRPIPE) {
        // Suspended (power management): resume, drop, re-prepare once.
        if (snd_pcm_resume(impl->pcm) < 0 && snd_pcm_prepare(impl->pcm) < 0) {
          impl->failed.store(true);
        }
        continue;
      }
      impl->failed.store(true);
      break;
    }
    return nullptr;
  }
};

AlsaIn::AlsaIn() noexcept : impl_(std::make_unique<Impl>()) {}

AlsaIn::~AlsaIn() {
  if (impl_->pcm != nullptr) {
    impl_->stop.store(true);
    snd_pcm_drop(impl_->pcm);  // unblocks readi() so the join is immediate
    if (impl_->started) pthread_join(impl_->thread, nullptr);
    snd_pcm_close(impl_->pcm);
    impl_->pcm = nullptr;
  }
}

std::unique_ptr<AlsaIn> AlsaIn::open(const char* device, std::string& error) noexcept {
  std::unique_ptr<AlsaIn> self(new (std::nothrow) AlsaIn());
  if (!self) {
    error = "out of memory";
    return nullptr;
  }

  int rc = snd_pcm_open(&self->impl_->pcm, device, SND_PCM_STREAM_CAPTURE, 0);
  if (rc < 0) {
    error = alsa_error(rc);
    return nullptr;
  }

  rc = snd_pcm_set_params(self->impl_->pcm, SND_PCM_FORMAT_S16_LE, SND_PCM_ACCESS_RW_INTERLEAVED,
                          1, kSampleRate, 1, kLatencyUs);
  if (rc < 0) {
    error = alsa_error(rc);
    snd_pcm_close(self->impl_->pcm);
    self->impl_->pcm = nullptr;
    return nullptr;
  }

  rc = pthread_create(&self->impl_->thread, nullptr, &Impl::run, self->impl_.get());
  if (rc != 0) {
    error = "cannot start capture thread";
    snd_pcm_close(self->impl_->pcm);
    self->impl_->pcm = nullptr;
    return nullptr;
  }
  self->impl_->started = true;
  return self;
}

std::size_t AlsaIn::read(std::span<std::int16_t> out) noexcept {
  return impl_->ring.pop(out.data(), out.size());
}

bool AlsaIn::failed() const noexcept { return impl_->failed.load(std::memory_order_relaxed); }

std::uint32_t AlsaIn::overruns() const noexcept {
  return impl_->overruns.load(std::memory_order_relaxed);
}

struct AlsaOut::Impl {
  snd_pcm_t* pcm = nullptr;
  std::uint32_t underruns = 0;
};

AlsaOut::AlsaOut() noexcept : impl_(std::make_unique<Impl>()) {}

AlsaOut::~AlsaOut() {
  if (impl_->pcm != nullptr) {
    snd_pcm_close(impl_->pcm);
    impl_->pcm = nullptr;
  }
}

std::unique_ptr<AlsaOut> AlsaOut::open(const char* device, std::string& error) noexcept {
  std::unique_ptr<AlsaOut> self(new (std::nothrow) AlsaOut());
  if (!self) {
    error = "out of memory";
    return nullptr;
  }

  int rc = snd_pcm_open(&self->impl_->pcm, device, SND_PCM_STREAM_PLAYBACK, 0);
  if (rc < 0) {
    error = alsa_error(rc);
    return nullptr;
  }

  rc = snd_pcm_set_params(self->impl_->pcm, SND_PCM_FORMAT_S16_LE, SND_PCM_ACCESS_RW_INTERLEAVED,
                          1, kSampleRate, 1, kLatencyUs);
  if (rc < 0) {
    error = alsa_error(rc);
    snd_pcm_close(self->impl_->pcm);
    self->impl_->pcm = nullptr;
    return nullptr;
  }
  return self;
}

bool AlsaOut::write(std::span<const std::int16_t> pcm) noexcept {
  std::size_t done = 0;
  unsigned recoveries = 0;
  while (done < pcm.size()) {
    const snd_pcm_sframes_t n = snd_pcm_writei(
        impl_->pcm, pcm.data() + done, static_cast<snd_pcm_uframes_t>(pcm.size() - done));
    if (n > 0) {
      done += static_cast<std::size_t>(n);
      continue;
    }
    if (n == -EPIPE || n == -ESTRPIPE) {
      if (++recoveries > kMaxRecoveries) return false;
      ++impl_->underruns;
      if (snd_pcm_prepare(impl_->pcm) < 0) return false;
      continue;
    }
    if (n == -EAGAIN) {
      snd_pcm_wait(impl_->pcm, 100);
      continue;
    }
    return false;
  }
  return true;
}

void AlsaOut::drain() noexcept {
  snd_pcm_drain(impl_->pcm);
}

std::uint32_t AlsaOut::underruns() const noexcept { return impl_->underruns; }

}  // namespace xtrans
