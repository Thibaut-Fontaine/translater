import React from 'react';
import ReactTestRenderer from 'react-test-renderer';

jest.mock('../src/native/NativeXtransSession', () => ({
  __esModule: true,
  default: {
    isReady: () => true,
    modelsDir: () => '/data/user/0/com.xtrans.app/files/models',
    openModels: jest.fn().mockResolvedValue(true),
    modelsStatus: () =>
      JSON.stringify([{name: 'ggml-base-q5_1.bin', present: true, bytes: 59707625}]),
    configure: () => true,
    startVoice: () => true,
    stopVoice: jest.fn().mockResolvedValue(0),
    translate: jest.fn().mockResolvedValue(0),
    cancel: () => {},
    state: () => 0,
    transcript: () => 'bonjour',
    translation: () => 'hello',
    lastError: () => '',
    speak: () => {},
    stopSpeaking: () => {},
  },
}));

import App from '../src/App';

test('renders the translator screen and loads the models', async () => {
  let tree: ReactTestRenderer.ReactTestRenderer | undefined;
  await ReactTestRenderer.act(async () => {
    tree = ReactTestRenderer.create(<App />);
  });
  const text = JSON.stringify(tree!.toJSON());
  expect(text).toContain('Xtrans');
  expect(text).toContain('Hold to talk');
  await ReactTestRenderer.act(async () => {
    tree!.unmount();
  });
});
