import React from 'react';
import ReactTestRenderer from 'react-test-renderer';

/**
 * Starting the app must not load the 1.4 GB of models.
 *
 * On a phone that answer arrives as an instant exit: three ONNX sessions plus
 * the whisper model, allocated before the first frame is drawn, and the low
 * memory killer takes the process. Checking that the files exist is cheap and
 * allowed; opening them is not.
 *
 * The mocks are created inside the factory on purpose: `jest.mock` factories
 * run before `const` initialisers in the test file, so a captured `jest.fn()`
 * would be `undefined`.
 */
jest.mock('../src/native/NativeXtransSession', () => ({
  __esModule: true,
  nativeModuleAvailable: true,
  default: {
    isReady: jest.fn(() => false),
    modelsDir: jest.fn(() => '/data/user/0/com.xtrans.app/files/models'),
    openModels: jest.fn(async () => true),
    modelsStatus: jest.fn(() => JSON.stringify([{name: 'ggml-base-q5_1.bin', present: true, bytes: 12}])),
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
  },
}));

import App from '../src/App';
import NativeSession from '../src/native/NativeXtransSession';

const openModels = NativeSession!.openModels as jest.Mock;
const modelsStatus = NativeSession!.modelsStatus as jest.Mock;

test('mounting the app checks the model files but never opens them', async () => {
  let tree: ReactTestRenderer.ReactTestRenderer | undefined;
  await ReactTestRenderer.act(async () => {
    tree = ReactTestRenderer.create(<App />);
  });

  expect(modelsStatus).toHaveBeenCalled();
  expect(openModels).not.toHaveBeenCalled();

  await ReactTestRenderer.act(async () => {
    tree!.unmount();
  });
});
