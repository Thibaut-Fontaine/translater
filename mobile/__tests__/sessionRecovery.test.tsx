import React, {useEffect} from 'react';
import ReactTestRenderer from 'react-test-renderer';

import {useTranslator, Translator} from '../src/state/useTranslator';
import NativeSession from '../src/native/NativeXtransSession';

/**
 * A turn can stay open without the UI knowing: the first press opens the
 * microphone permission dialog, the release is delivered while the dialog has
 * focus, and the recorder starts afterwards. Every later `configure` is then
 * rejected as "busy", which the UI showed as "this pair cannot be translated".
 *
 * The session must recover: a stray release cancels, and a refused configure
 * clears the turn and retries once.
 */
jest.mock('../src/native/NativeXtransSession', () => ({
  __esModule: true,
  nativeModuleAvailable: true,
  default: {
    isReady: jest.fn(() => true),
    modelsDir: jest.fn(() => '/models'),
    openModels: jest.fn(async () => true),
    modelsStatus: jest.fn(() =>
      JSON.stringify([
        {name: 'nllb_bpe.bin', present: true, bytes: 1, state: 'ready', received: 1, total: 1, error: null},
      ]),
    ),
    configure: jest.fn(() => true),
    startVoice: jest.fn(() => true),
    stopVoice: jest.fn(async () => 0),
    translate: jest.fn(async () => 0),
    cancel: jest.fn(),
    state: jest.fn(() => 0),
    transcript: jest.fn(() => ''),
    translation: jest.fn(() => ''),
    lastError: jest.fn(() => ''),
    speak: jest.fn(),
    stopSpeaking: jest.fn(),
    downloadModels: jest.fn(() => true),
    cancelDownloads: jest.fn(),
  },
}));

const configure = NativeSession!.configure as jest.Mock;
const cancel = NativeSession!.cancel as jest.Mock;
const translate = NativeSession!.translate as jest.Mock;

function Harness({expose}: {expose: (t: Translator) => void}) {
  const translator = useTranslator();
  useEffect(() => {
    expose(translator);
  }, [expose, translator]);
  return null;
}

async function mount() {
  let current: Translator | undefined;
  let tree: ReactTestRenderer.ReactTestRenderer | undefined;
  await ReactTestRenderer.act(async () => {
    tree = ReactTestRenderer.create(<Harness expose={t => (current = t)} />);
  });
  return {
    get: () => current!,
    unmount: async () => {
      await ReactTestRenderer.act(async () => {
        tree!.unmount();
      });
    },
  };
}

let busy = true;

beforeEach(() => {
  busy = true;
  configure.mockReset();
  // Mirrors the core: configure refuses while a turn is open, cancel closes it.
  configure.mockImplementation(() => !busy);
  cancel.mockReset();
  cancel.mockImplementation(() => {
    busy = false;
  });
  translate.mockClear();
});

test('a refused configure clears the open turn and retries', async () => {
  const harness = await mount();

  await ReactTestRenderer.act(async () => {
    await harness.get().translateTyped('bonjour');
  });

  expect(cancel).toHaveBeenCalled();
  expect(configure.mock.calls.length).toBeGreaterThanOrEqual(2);
  expect(translate).toHaveBeenCalledWith('bonjour');
  expect(harness.get().error).toBeNull();

  await harness.unmount();
});

test('a pair that still fails after the retry says the engine is not ready', async () => {
  configure.mockImplementation(() => false);
  const harness = await mount();

  await ReactTestRenderer.act(async () => {
    await harness.get().translateTyped('bonjour');
  });

  expect(translate).not.toHaveBeenCalled();
  expect(harness.get().error).toContain('engine');

  await harness.unmount();
});

test('a release with no active turn cancels the stray native turn', async () => {
  const harness = await mount();

  await ReactTestRenderer.act(async () => {
    await harness.get().finishVoice();
  });

  expect(cancel).toHaveBeenCalled();

  await harness.unmount();
});
