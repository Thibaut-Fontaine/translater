import React, {useEffect} from 'react';
import ReactTestRenderer from 'react-test-renderer';

import {useTranslator, Translator} from '../src/state/useTranslator';
import NativeSession from '../src/native/NativeXtransSession';

/**
 * Models are opened on first use, not at launch, and only once.
 *
 * Loading them lazily is what keeps the process alive on a phone: the first
 * translation pays for it, the following ones do not, and a failure leaves the
 * UI in a state that can be retried rather than killing the app.
 */
jest.mock('../src/native/NativeXtransSession', () => ({
  __esModule: true,
  nativeModuleAvailable: true,
  default: {
    isReady: jest.fn(() => true),
    modelsDir: jest.fn(() => '/models'),
    openModels: jest.fn(async () => true),
    modelsStatus: jest.fn(() => JSON.stringify([{name: 'nllb_bpe.bin', present: true, bytes: 1, state: 'ready', received: 1, total: 1, error: null}])),
    configure: jest.fn(() => true),
    startVoice: jest.fn(() => true),
    stopVoice: jest.fn(async () => 0),
    translate: jest.fn(async () => 0),
    cancel: jest.fn(),
    state: jest.fn(() => 0),
    transcript: jest.fn(() => 'bonjour'),
    translation: jest.fn(() => 'hello'),
    lastError: jest.fn(() => ''),
    speak: jest.fn(),
    stopSpeaking: jest.fn(),
  },
}));

const openModels = NativeSession!.openModels as jest.Mock;
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
  if (current === undefined) throw new Error('hook did not render');
  return {
    get: () => current!,
    unmount: async () => {
      await ReactTestRenderer.act(async () => {
        tree!.unmount();
      });
    },
  };
}

beforeEach(() => {
  openModels.mockClear();
  translate.mockClear();
  openModels.mockResolvedValue(true);
});

test('the first translation opens the models, the second reuses them', async () => {
  const harness = await mount();
  expect(openModels).not.toHaveBeenCalled();

  await ReactTestRenderer.act(async () => {
    await harness.get().translateTyped('bonjour');
  });
  expect(openModels).toHaveBeenCalledTimes(1);
  expect(translate).toHaveBeenCalledWith('bonjour');
  expect(harness.get().load).toBe('ready');
  expect(harness.get().translation).toBe('hello');

  await ReactTestRenderer.act(async () => {
    await harness.get().translateTyped('encore');
  });
  expect(openModels).toHaveBeenCalledTimes(1);
  expect(translate).toHaveBeenCalledTimes(2);

  await harness.unmount();
});

test('a failed model load is reported and nothing is translated', async () => {
  openModels.mockResolvedValue(false);
  const harness = await mount();

  await ReactTestRenderer.act(async () => {
    await harness.get().translateTyped('bonjour');
  });

  expect(translate).not.toHaveBeenCalled();
  expect(harness.get().load).toBe('failed');
  expect(harness.get().error).not.toBeNull();

  await harness.unmount();
});
