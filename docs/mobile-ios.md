# iOS build notes

Status: **not built.** This workstation is Linux; Xcode, `pod`, and an iOS
simulator do not exist here. The React Native app (`mobile/src`) is shared and
already platform-neutral; what is missing is the Objective-C++ bridge and the
pod packaging around the same C++ core.

Do not treat anything below as verified. It is the exact sequence to follow on a
Mac, with the interface the bridge must expose.

## What already exists

| Piece | Portable? |
| --- | --- |
| `include/xtrans/*.hpp`, `src/*.cpp` (core, Session, BPE) | yes, plain C++20 |
| `src/adapters/whisper.cpp` | yes, whisper.cpp has an iOS build |
| `src/adapters/nllb.cpp` | yes, with `XTRANS_NLLB_TOKENIZER_BPE=1` and ONNX Runtime iOS |
| `src/bpe.cpp` + `models/nllb_bpe.bin` | yes, no dependencies |
| `mobile/src/**` (UI, state, TurboModule spec) | yes |
| `jni_bridge.cpp`, `XtransModule.kt` | no, Android only |

## What the bridge must expose

The TurboModule spec in `mobile/src/native/NativeXtransSession.ts` is the
contract. On iOS the generated protocol is `NativeXtransSessionSpec`; implement
it in Objective-C++ and forward to the same C++ calls the JNI bridge uses:

```
openModels(modelsDir) -> Promise<boolean>   // WhisperStt::open + NllbMt::open
modelsDir() -> string                       // NSApplicationSupportDirectory/models
modelsStatus() -> string                    // JSON [{name,present,bytes}]
configure(from, to, mode) -> boolean
startVoice() -> boolean                     // AVAudioEngine, 16 kHz mono
stopVoice() -> Promise<number>              // Session::finish_voice
translate(text) -> Promise<number>          // NFKC via NSString, then translate_text
cancel(), state(), transcript(), translation(), lastError()
speak(text, tag)                            // AVSpeechSynthesizer
```

Audio: `AVAudioEngine` input node at the hardware rate, `AVAudioConverter` to
16 kHz mono Int16, appended to a `std::vector<int16_t>`; the buffer is passed to
`finish_voice`. Never cross the JS bridge with samples.

Normalisation: `[text precomposedStringWithCompatibilityMapping]` before
`translate_text`, to match the NFKC step Android does in Kotlin.

## Build outline

1. `models` in Application Support, marked `NSURLIsExcludedFromBackupKey`.
2. `Info.plist`: `NSMicrophoneUsageDescription`, no `UIBackgroundModes`.
3. ONNX Runtime via CocoaPods (`onnxruntime-c`, 1.21.x) or the official iOS
   package; headers must match `nllb.cpp`.
4. Add the core sources to a local pod:
   `mobile/ios/XtransNative/XtransNative.podspec` with
   `source_files = ['../../../../include/**/*.hpp', '../../../../src/**/*.cpp', 'XtransModule.mm']`
   and `pod 'XtransNative', :path => './XtransNative'` in the Podfile.
5. `pod install`, then `npx react-native run-ios --device`.
6. Measure peak RSS: NLLB int8 is ~1.3 GB of weights, and iOS jetsam on older
   devices will kill the app before Android would.

## Known differences to expect

- iOS cannot background-translate; the app stops when it leaves the foreground.
- `AVSpeechSynthesizer` voice availability differs per iOS version: query
  `[AVSpeechSynthesisVoice voiceWithLanguage:]` and fall back to text.
- The App Store distribution question (NLLB CC-BY-NC) is unchanged: personal
  builds only until the model is replaced.
