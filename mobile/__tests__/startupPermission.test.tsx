import React, {useEffect} from 'react';
import {PermissionsAndroid, Platform} from 'react-native';
import ReactTestRenderer from 'react-test-renderer';

import {useTranslator, Translator} from '../src/state/useTranslator';

/**
 * The microphone permission is requested once, when the app starts, so the
 * dialog never interrupts a press-to-talk gesture (which used to leave a turn
 * open and make every later translation fail).
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

import NativeSession from '../src/native/NativeXtransSession';

const startVoice = NativeSession!.startVoice as jest.Mock;

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

beforeEach(() => {
  jest.replaceProperty(Platform, 'OS', 'android');
  startVoice.mockClear();
});

afterEach(() => {
  jest.restoreAllMocks();
});

test('asks for the microphone once at startup when it is missing', async () => {
  const check = jest.spyOn(PermissionsAndroid, 'check').mockResolvedValue(false);
  const request = jest
    .spyOn(PermissionsAndroid, 'request')
    .mockResolvedValue(PermissionsAndroid.RESULTS.GRANTED);

  const harness = await mount();

  expect(check).toHaveBeenCalledWith(PermissionsAndroid.PERMISSIONS.RECORD_AUDIO);
  expect(request).toHaveBeenCalledTimes(1);
  expect(harness.get().micPermission).toBe('granted');

  await harness.unmount();
});

test('does not ask again when the permission is already granted', async () => {
  jest.spyOn(PermissionsAndroid, 'check').mockResolvedValue(true);
  const request = jest.spyOn(PermissionsAndroid, 'request');

  const harness = await mount();

  expect(request).not.toHaveBeenCalled();
  expect(harness.get().micPermission).toBe('granted');

  await harness.unmount();
});

test('a refusal at startup keeps voice disabled but leaves text mode usable', async () => {
  jest.spyOn(PermissionsAndroid, 'check').mockResolvedValue(false);
  jest
    .spyOn(PermissionsAndroid, 'request')
    .mockResolvedValue(PermissionsAndroid.RESULTS.DENIED);

  const harness = await mount();
  expect(harness.get().micPermission).toBe('denied');

  await ReactTestRenderer.act(async () => {
    await harness.get().beginVoice();
  });

  expect(startVoice).not.toHaveBeenCalled();
  expect(harness.get().error).toContain('Microphone');

  await harness.unmount();
});
