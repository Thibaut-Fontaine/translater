import type {TurboModule} from 'react-native';
import {TurboModuleRegistry} from 'react-native';

/**
 * The only bridge between the UI and the C++ core.
 *
 * Audio never crosses this boundary: `startVoice` opens the microphone in
 * native code, `stopVoice` hands the captured PCM straight to the session.
 * Everything except the polling getters is asynchronous so the JS thread never
 * waits for a model.
 */
export interface Spec extends TurboModule {
  /** Loads whisper + NLLB from the models directory. Slow: seconds. */
  openModels(modelsDir: string): Promise<boolean>;
  isReady(): boolean;
  /** Where models must be pushed (app-private storage). */
  modelsDir(): string;
  /** JSON array of {name, present, bytes} for the Models screen. */
  modelsStatus(): string;

  /** `from`/`to` are catalog values, `mode` is 1 (speech->text) or 2 (text->text). */
  configure(from: number, to: number, mode: number): boolean;

  startVoice(): boolean;
  /** Stops capture, runs STT + MT; resolves to a SessionStatus code. */
  stopVoice(): Promise<number>;
  /** Typed input; resolves to a SessionStatus code. */
  translate(text: string): Promise<number>;

  cancel(): void;
  state(): number;
  transcript(): string;
  translation(): string;
  lastError(): string;

  /** OS speech engine (Android TextToSpeech / iOS AVSpeechSynthesizer). */
  speak(text: string, languageTag: string): void;
  stopSpeaking(): void;
}

// `get`, not `getEnforcing`: a module that failed to register must become a
// visible error, not an exception thrown while the bundle is still evaluating
// (which is an instant exit on a release build).
const NativeSession: Spec | null = TurboModuleRegistry.get<Spec>('XtransSession') ?? null;

export const nativeModuleAvailable = NativeSession !== null;

export default NativeSession;
