# Third-party sources

Nothing here is committed except the pins. `scripts/fetch_third_party.sh`
downloads each pinned archive, verifies its SHA-256, and extracts it into a
gitignored directory, so a checkout stays small and a build is reproducible
from the pin file alone.

| Project | Pin file | Extract to | Used for |
| --- | --- | --- | --- |
| whisper.cpp | `whisper.pin` | `third_party/whisper.cpp/` | speech recognition |

Current whisper.cpp pin: **v1.9.5**, commit
`d2a764c648779feb36d44363f212a29b00aad1af`, SHA-256 of the GitHub tarball
`f70aa528fcb2c4a3c843485cc8f299e7fee29d87b2b71b3f145504dc4b99a5ba`.

## Rules

- The pin file is the source of truth; never edit an extracted tree.
- Bumping a version means: update `*.pin`, run
  `scripts/fetch_third_party.sh --force`, rebuild, re-run `ctest`, and record
  the measured effect in the commit message.
- Local patches, if ever needed, live in `third_party/patches/` with a README
  line explaining why. Nothing is patched silently.
- `scripts/fetch_third_party.sh --self-test` validates the pins without
  touching the network; run it in CI.

## Build notes

- whisper.cpp is added with `add_subdirectory(... EXCLUDE_FROM_ALL)`,
  examples/tests/server off, shared libs off, `GGML_NATIVE=OFF` so the host and
  the cross build behave the same.
- `XTRANS_WITH_WHISPER` is auto-detected from the presence of the extracted
  tree; without it the daemon builds without recognition rather than failing.
