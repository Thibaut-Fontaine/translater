// xtrans-install — write a translator image to an SD card, then verify it.
//
// Safety rules, in order: the device must be named explicitly, must be
// removable, must not be mounted, and must be large enough. Nothing is written
// before every check passes. Read-back verification compares the card against
// the image byte for byte.
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "xtrans/flash.hpp"

namespace {

void usage(const char* argv0) {
  std::fprintf(stderr,
               "usage: %s --image FILE --device NAME [--dry-run] [--yes]\n"
               "\n"
               "  --image FILE   image to write\n"
               "  --device NAME  block device name, e.g. sdb (never a path)\n"
               "  --dry-run      run every check, write nothing\n"
               "  --yes          skip the interactive confirmation\n"
               "\n"
               "Refuses fixed disks, mounted filesystems and images that do not fit.\n",
               argv0);
}

std::uint64_t file_size(const char* path) {
  struct stat st {};
  if (::stat(path, &st) != 0) return 0;
  return static_cast<std::uint64_t>(st.st_size);
}

std::uint64_t device_size(const std::string& name) {
  const std::string path = "/sys/class/block/" + name + "/size";
  std::FILE* f = std::fopen(path.c_str(), "r");
  if (f == nullptr) return 0;
  unsigned long long sectors = 0;
  const int got = std::fscanf(f, "%llu", &sectors);
  std::fclose(f);
  return got == 1 ? static_cast<std::uint64_t>(sectors) * 512ULL : 0;
}

bool device_flag(const std::string& name, const char* flag) {
  const std::string path = "/sys/class/block/" + name + "/" + flag;
  std::FILE* f = std::fopen(path.c_str(), "r");
  if (f == nullptr) return false;
  int value = 0;
  const int got = std::fscanf(f, "%d", &value);
  std::fclose(f);
  return got == 1 && value != 0;
}

bool device_mounted(const std::string& name) {
  std::FILE* f = std::fopen("/proc/mounts", "r");
  if (f == nullptr) return true;  // cannot tell: refuse
  char line[512];
  bool mounted = false;
  while (std::fgets(line, sizeof line, f) != nullptr) {
    if (std::strstr(line, name.c_str()) != nullptr) {
      mounted = true;
      break;
    }
  }
  std::fclose(f);
  return mounted;
}

bool copy_with_progress(const char* image, const std::string& device, std::uint64_t total) {
  std::FILE* in = std::fopen(image, "rb");
  if (in == nullptr) return false;
  const std::string path = "/dev/" + device;
  const int out = ::open(path.c_str(), O_WRONLY | O_SYNC);
  if (out < 0) {
    std::fclose(in);
    return false;
  }

  std::vector<char> buffer(1U << 20);
  std::uint64_t written = 0;
  int last = -1;
  bool ok = true;
  while (ok) {
    const std::size_t n = std::fread(buffer.data(), 1, buffer.size(), in);
    if (n == 0) break;
    std::size_t offset = 0;
    while (offset < n) {
      const ssize_t w = ::write(out, buffer.data() + offset, n - offset);
      if (w <= 0) {
        ok = false;
        break;
      }
      offset += static_cast<std::size_t>(w);
    }
    written += n;
    const int percent = xtrans::progress_percent(written, total);
    if (percent != last) {
      std::fprintf(stderr, "\r%3d%%", percent);
      last = percent;
    }
  }
  std::fputc('\n', stderr);
  if (::fsync(out) != 0) ok = false;
  ::close(out);
  std::fclose(in);
  return ok && written == total;
}

bool verify_readback(const char* image, const std::string& device) {
  std::FILE* in = std::fopen(image, "rb");
  if (in == nullptr) return false;
  const std::string path = "/dev/" + device;
  const int fd = ::open(path.c_str(), O_RDONLY);
  if (fd < 0) {
    std::fclose(in);
    return false;
  }

  std::vector<char> a(1U << 20);
  std::vector<char> b(1U << 20);
  bool same = true;
  while (same) {
    const std::size_t n = std::fread(a.data(), 1, a.size(), in);
    if (n == 0) break;
    const ssize_t got = ::read(fd, b.data(), n);
    if (got != static_cast<ssize_t>(n) || std::memcmp(a.data(), b.data(), n) != 0) same = false;
  }
  ::close(fd);
  std::fclose(in);
  return same;
}

}  // namespace

int main(int argc, char** argv) {
  const char* image = nullptr;
  const char* device_name = nullptr;
  bool dry_run = false;
  bool assume_yes = false;

  for (int i = 1; i < argc; ++i) {
    if (std::strcmp(argv[i], "--image") == 0 && i + 1 < argc) {
      image = argv[++i];
    } else if (std::strcmp(argv[i], "--device") == 0 && i + 1 < argc) {
      device_name = argv[++i];
    } else if (std::strcmp(argv[i], "--dry-run") == 0) {
      dry_run = true;
    } else if (std::strcmp(argv[i], "--yes") == 0) {
      assume_yes = true;
    } else if (std::strcmp(argv[i], "--help") == 0) {
      usage(argv[0]);
      return 0;
    } else {
      usage(argv[0]);
      return 2;
    }
  }
  if (image == nullptr || device_name == nullptr) {
    usage(argv[0]);
    return 2;
  }
  if (std::strchr(device_name, '/') != nullptr) {
    std::fputs("xtrans-install: --device takes a bare name (e.g. sdb), not a path\n", stderr);
    return 2;
  }

  const std::uint64_t image_bytes = file_size(image);
  if (image_bytes == 0) {
    std::fprintf(stderr, "xtrans-install: cannot read image '%s'\n", image);
    return 2;
  }

  xtrans::BlockDevice target;
  target.name = device_name;
  target.bytes = device_size(target.name);
  if (target.bytes == 0) {
    std::fprintf(stderr, "xtrans-install: no such block device: %s\n", device_name);
    return 3;
  }
  target.removable = device_flag(target.name, "removable");
  target.mounted = device_mounted(target.name);

  const xtrans::TargetError verdict = xtrans::assess_target(target, image_bytes);
  if (verdict != xtrans::TargetError::ok) {
    std::fprintf(stderr, "xtrans-install: refusing /dev/%s: %s\n", device_name,
                 std::string(xtrans::target_error_text(verdict)).c_str());
    return 3;
  }

  std::printf("target /dev/%s: %.1f GB removable, image %.1f GB\n", device_name,
              static_cast<double>(target.bytes) / 1e9,
              static_cast<double>(image_bytes) / 1e9);

  if (dry_run) {
    std::puts("dry run: every check passed, nothing written");
    return 0;
  }

  if (!assume_yes) {
    std::printf("This will ERASE /dev/%s. Type the device name to confirm: ", device_name);
    std::fflush(stdout);
    char answer[64];
    if (std::fgets(answer, sizeof answer, stdin) == nullptr) return 3;
    answer[std::strcspn(answer, "\n")] = '\0';
    if (std::strcmp(answer, device_name) != 0) {
      std::fputs("xtrans-install: aborted\n", stderr);
      return 3;
    }
  }

  std::fprintf(stderr, "writing %s to /dev/%s\n", image, device_name);
  if (!copy_with_progress(image, device_name, image_bytes)) {
    std::fputs("xtrans-install: write failed\n", stderr);
    return 4;
  }
  std::fprintf(stderr, "verifying read-back\n");
  if (!verify_readback(image, device_name)) {
    std::fputs("xtrans-install: read-back does not match the image\n", stderr);
    return 4;
  }

  std::printf("done: /dev/%s written and verified; it is safe to remove the card\n",
              device_name);
  return 0;
}
