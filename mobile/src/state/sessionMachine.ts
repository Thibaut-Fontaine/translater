/**
 * Pure translations between the native status codes and what the screen shows.
 * Kept free of React so it can be tested without a renderer.
 */

export const NATIVE_STATE = {
  idle: 0,
  recording: 1,
  transcribing: 2,
  translating: 3,
  speaking: 4,
} as const;

export const SESSION_STATUS = {
  ok: 0,
  busy: 1,
  unsupportedPair: 2,
  emptyText: 3,
  noSpeech: 4,
  sttFailed: 5,
  mtFailed: 6,
} as const;

export type Phase = 'idle' | 'recording' | 'transcribing' | 'translating' | 'speaking';

export function phaseFromState(state: number): Phase {
  switch (state) {
    case NATIVE_STATE.recording:
      return 'recording';
    case NATIVE_STATE.transcribing:
      return 'transcribing';
    case NATIVE_STATE.translating:
      return 'translating';
    case NATIVE_STATE.speaking:
      return 'speaking';
    default:
      return 'idle';
  }
}

export function isBusy(phase: Phase): boolean {
  return phase !== 'idle';
}

/** Stable English strings; the UI translates them through `i18n.ts`. */
export function errorForStatus(status: number): string | null {
  switch (status) {
    case SESSION_STATUS.ok:
      return null;
    case SESSION_STATUS.busy:
      return 'A turn is already running.';
    case SESSION_STATUS.unsupportedPair:
      return 'This language pair cannot be translated.';
    case SESSION_STATUS.emptyText:
      return 'Type something first.';
    case SESSION_STATUS.noSpeech:
      return 'Nothing was recognised. Try again closer to the microphone.';
    case SESSION_STATUS.sttFailed:
      return 'Speech recognition failed.';
    case SESSION_STATUS.mtFailed:
      return 'Translation failed.';
    default:
      return 'Unknown error.';
  }
}

export function errorForNativeMessage(message: string): string | null {
  switch (message) {
    case '':
      return null;
    case 'no speech':
      return 'Nothing was recognised. Try again closer to the microphone.';
    case 'recognition failed':
      return 'Speech recognition failed.';
    case 'translation failed':
      return 'Translation failed.';
    default:
      return message;
  }
}

/** Milliseconds between polls while a turn is running. 8 Hz is smooth enough. */
export const POLL_INTERVAL_MS = 120;
