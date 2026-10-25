# Xtrans for Android and iOS

React Native shell around the same C++ core as the Raspberry Pi build. The phone
shows the transcript and the translation, so a language pair only needs NLLB:
text-only languages (Acehnese, Khmer, Burmese, …) work, and typing replaces the
microphone when a language has no whisper code.

```
RN (TypeScript) ── TurboModule ── JNI ── Session ── WhisperStt + NllbMt
       ▲                              │
       └──── polling getters ─────────┘   audio is captured natively
```

## Status

| Piece | State |
| --- | --- |
| Android app (UI, state, TurboModule, JNI, capture, OS TTS) | **builds**, debug and release APK; not yet run on a device |
| Shared C++ core (202 languages, capabilities, typed turns) | tested on the host (137 tests) |
| NLLB tokenizer without SentencePiece | verified against HuggingFace `tokenizers` on 5 languages |
| iOS | the JS/UI is shared; the Objective-C++ bridge is not written yet (needs macOS), see `docs/mobile-ios.md` |

The 1.4 GB of models are **not** in the app: push them once with adb.

## Prerequisites

- Node ≥ 22.11, npm
- JDK 17, Android SDK 36/37, NDK 27.1 (the version in `android/build.gradle`)
- CMake 3.22.1 from the SDK (`sdkmanager "cmake;3.22.1"`)
- `arm64-v8a` device with ≥ 6 GB of RAM (NLLB int8 + whisper q5)

## One-time setup

```bash
# from the repository root
scripts/fetch_third_party.sh      # whisper.cpp (pinned)
scripts/fetch_models.sh           # NLLB int8 + whisper + voices, then the BPE blob
scripts/fetch_ort_android.sh      # ONNX Runtime 1.21 AAR -> headers + arm64 .so

cd mobile
npm install
```

`scripts/fetch_ort_android.sh` writes into `android/app/src/main/jniLibs/` and
`android/app/src/main/cpp/ort/`; both are gitignored. `scripts/gen_nllb_bpe.py`
turns `models/nllb/tokenizer.json` into `models/nllb_bpe.bin`.

## Build and install

```bash
cd mobile/android
./gradlew assembleRelease          # 73 MB APK, bundled JS, debug keystore
adb install -r app/build/outputs/apk/release/app-release.apk

# models: ~1.4 GB over USB
cd ../..
scripts/push_models_android.sh
```

Open the app: the dot next to the title turns green when the models load.
`Models` lists each file with its size.

The release build is signed with the template's debug keystore so it can be
sideloaded. Generate a real keystore before distributing it to anyone else;
nothing in the app needs a Play Store upload.

Development build with Metro:

```bash
cd mobile
npm start
cd android && ./gradlew installDebug
```

## Tests

```bash
cd mobile
npx tsc --noEmit       # types, including the TurboModule spec
npx jest               # 11 tests: catalog, state mapping, screen smoke test
```

The C++ side keeps its own suite at the repository root (`ctest`).

## Where things live

| Path | Purpose |
| --- | --- |
| `src/App.tsx` | tabs, model dot, language picker |
| `src/screens/` | Translate, Models, Settings, language picker |
| `src/state/useTranslator.ts` | the only place that talks to the native module |
| `src/state/sessionMachine.ts` | pure status/phase mapping (tested) |
| `src/data/languages.json` | generated catalog (202 languages, capabilities) |
| `src/native/NativeXtransSession.ts` | TurboModule spec (codegen) |
| `android/app/src/main/cpp/jni_bridge.cpp` | JNI, model loading, snapshots |
| `android/app/src/main/cpp/CMakeLists.txt` | core + whisper.cpp + ONNX Runtime |
| `android/.../XtransModule.kt` | capture, worker thread, OS TTS |

## Design notes

- **No PCM on the bridge.** `startVoice`/`stopVoice` capture and hand samples
  straight to C++; JS only sees text.
- **Polling, not events.** Sync getters read a mutex-guarded snapshot, so a
  2-second translation never blocks the UI thread. 8 Hz is enough for the
  transcript card.
- **NFKC in the platform.** `java.text.Normalizer` normalises before the C++
  BPE encoder; SentencePiece is not built for Android, and its exact `nmt_nfkc`
  table is replaced by the platform's Unicode implementation.
- **OS speech.** Android `TextToSpeech` / iOS `AVSpeechSynthesizer`; Piper stays
  a Pi feature because GPL-3 inside an app store build is a different problem.
