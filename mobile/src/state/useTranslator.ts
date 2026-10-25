import {useCallback, useEffect, useRef, useState} from 'react';
import {PermissionsAndroid, Platform} from 'react-native';

import NativeSession from '../native/NativeXtransSession';
import {
  DEFAULT_FROM,
  DEFAULT_TO,
  Language,
  canSpeakIn,
  canTranslate,
  speechTag,
} from '../data/languages';
import {
  POLL_INTERVAL_MS,
  Phase,
  SESSION_STATUS,
  errorForNativeMessage,
  errorForStatus,
  phaseFromState,
} from './sessionMachine';

export type LoadState = 'opening' | 'ready' | 'failed' | 'missing';

const SPEECH_MODE = 1; // speech_to_text
const TEXT_MODE = 2; // text_to_text

async function ensureMicrophone(): Promise<boolean> {
  if (Platform.OS !== 'android') return true;
  const granted = await PermissionsAndroid.check(
    PermissionsAndroid.PERMISSIONS.RECORD_AUDIO,
  );
  if (granted) return true;
  const result = await PermissionsAndroid.request(
    PermissionsAndroid.PERMISSIONS.RECORD_AUDIO,
    {
      title: 'Microphone',
      message:
        'Xtrans listens only while you hold the talk button. Nothing leaves the phone.',
      buttonNegative: 'Not now',
      buttonPositive: 'Allow',
    },
  );
  return result === PermissionsAndroid.RESULTS.GRANTED;
}

/**
 * Owns one session for the whole app: languages, turn state, results and the
 * OS voice. The C++ core is single-threaded, so every call goes through the
 * native module's worker without racing.
 */
export function useTranslator() {
  const [from, setFrom] = useState<Language>(DEFAULT_FROM);
  const [to, setTo] = useState<Language>(DEFAULT_TO);
  const [phase, setPhase] = useState<Phase>('idle');
  const [transcript, setTranscript] = useState('');
  const [translation, setTranslation] = useState('');
  const [error, setError] = useState<string | null>(null);
  const [load, setLoad] = useState<LoadState>('opening');
  const [models, setModels] = useState<ModelStatus[]>([]);
  const [ttsEnabled, setTtsEnabled] = useState(true);
  const [draft, setDraft] = useState('');

  const busy = useRef(false);
  const poll = useRef<ReturnType<typeof setInterval> | null>(null);

  const refreshModels = useCallback(() => {
    try {
      setModels(JSON.parse(NativeSession.modelsStatus()) as ModelStatus[]);
    } catch {
      setModels([]);
    }
  }, []);

  const open = useCallback(async () => {
    setLoad('opening');
    try {
      const dir = NativeSession.modelsDir();
      const ok = await NativeSession.openModels(dir);
      setLoad(ok ? 'ready' : 'failed');
      refreshModels();
    } catch (e) {
      setLoad('failed');
      setError(String(e));
    }
  }, [refreshModels]);

  useEffect(() => {
    open();
  }, [open]);

  const configure = useCallback(
    (a: Language, b: Language, mode: number): boolean => {
      if (!canTranslate(a, b)) {
        setError(errorForStatus(SESSION_STATUS.unsupportedPair));
        return false;
      }
      const ok = NativeSession.configure(a.value, b.value, mode);
      if (!ok) setError(errorForStatus(SESSION_STATUS.unsupportedPair));
      return ok;
    },
    [],
  );

  useEffect(() => {
    if (load !== 'ready') return;
    configure(from, to, canSpeakIn(from) ? SPEECH_MODE : TEXT_MODE);
  }, [configure, from, to, load]);

  const stopPolling = useCallback(() => {
    if (poll.current !== null) {
      clearInterval(poll.current);
      poll.current = null;
    }
  }, []);

  const readResult = useCallback(() => {
    setTranscript(NativeSession.transcript());
    setTranslation(NativeSession.translation());
  }, []);

  const startPolling = useCallback(() => {
    stopPolling();
    poll.current = setInterval(() => {
      const native = NativeSession.state();
      setPhase(phaseFromState(native));
      readResult();
    }, POLL_INTERVAL_MS);
  }, [readResult, stopPolling]);

  useEffect(() => stopPolling, [stopPolling]);

  const speak = useCallback(
    (text: string, language: Language) => {
      if (!ttsEnabled || text.length === 0) return;
      try {
        NativeSession.speak(text, speechTag(language));
      } catch {
        // A device without a voice is not an error: the text is still there.
      }
    },
    [ttsEnabled],
  );

  const beginVoice = useCallback(async () => {
    if (busy.current) return;
    setError(null);
    if (load !== 'ready') {
      setError('Models are not loaded yet.');
      return;
    }
    if (!canSpeakIn(from)) {
      setError('This language has no speech recognition. Type instead.');
      return;
    }
    if (!(await ensureMicrophone())) {
      setError('Microphone permission is required to translate speech.');
      return;
    }
    if (!configure(from, to, SPEECH_MODE)) return;

    busy.current = true;
    setTranscript('');
    setTranslation('');
    try {
      if (!NativeSession.startVoice()) {
        busy.current = false;
        setError(errorForStatus(SESSION_STATUS.busy));
        return;
      }
      setPhase('recording');
      startPolling();
    } catch (e) {
      busy.current = false;
      setError(String(e));
    }
  }, [configure, from, load, startPolling, to]);

  const finishVoice = useCallback(async () => {
    if (!busy.current) return;
    busy.current = false;
    stopPolling();
    try {
      const status = await NativeSession.stopVoice();
      readResult();
      const nativeError = errorForNativeMessage(NativeSession.lastError());
      setError(nativeError ?? errorForStatus(status));
      setPhase('idle');
      if (status === SESSION_STATUS.ok) {
        speak(NativeSession.translation(), to);
      }
    } catch (e) {
      setPhase('idle');
      setError(String(e));
    }
  }, [readResult, speak, stopPolling, to]);

  const translateTyped = useCallback(
    async (text: string) => {
      if (busy.current) return;
      if (load !== 'ready') {
        setError('Models are not loaded yet.');
        return;
      }
      if (!configure(from, to, TEXT_MODE)) return;

      busy.current = true;
      setError(null);
      setPhase('translating');
      try {
        const status = await NativeSession.translate(text);
        readResult();
        setError(errorForStatus(status));
        if (status === SESSION_STATUS.ok) {
          setDraft('');
          speak(NativeSession.translation(), to);
        }
      } catch (e) {
        setError(String(e));
      } finally {
        busy.current = false;
        setPhase('idle');
      }
    },
    [configure, from, load, readResult, speak, to],
  );

  const cancel = useCallback(() => {
    try {
      NativeSession.cancel();
    } catch {
      // Nothing to cancel.
    }
    busy.current = false;
    stopPolling();
    setPhase('idle');
  }, [stopPolling]);

  const swap = useCallback(() => {
    setFrom(to);
    setTo(from);
    setTranscript('');
    setTranslation('');
    setError(null);
  }, [from, to]);

  const chooseFrom = useCallback((language: Language) => {
    setFrom(language);
    setError(null);
  }, []);

  const chooseTo = useCallback((language: Language) => {
    setTo(language);
    setError(null);
  }, []);

  return {
    from,
    to,
    phase,
    transcript,
    translation,
    error,
    load,
    models,
    ttsEnabled,
    draft,
    setDraft,
    setTtsEnabled,
    beginVoice,
    finishVoice,
    translateTyped,
    cancel,
    swap,
    chooseFrom,
    chooseTo,
    retryOpen: open,
    refreshModels,
  };
}

export type ModelStatus = {name: string; present: boolean; bytes: number};

export type Translator = ReturnType<typeof useTranslator>;
