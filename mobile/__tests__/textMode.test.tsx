import React from 'react';
import ReactTestRenderer from 'react-test-renderer';

/**
 * Text mode must be usable end to end from the screen: type, press Translate,
 * get the call through. The button used to live inside the scroll view, below
 * the fold and behind a duplicate mode switch — reachable by scrolling only.
 */
jest.mock('../src/native/NativeXtransSession', () => ({
  __esModule: true,
  nativeModuleAvailable: true,
  default: {
    isReady: jest.fn(() => true),
    modelsDir: jest.fn(() => '/models'),
    openModels: jest.fn(async () => true),
    modelsStatus: jest.fn(() => '[]'),
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

import App from '../src/App';
import NativeSession from '../src/native/NativeXtransSession';

const translate = NativeSession!.translate as jest.Mock;

function pressables(tree: ReactTestRenderer.ReactTestRenderer) {
  return tree.root.findAll(
    node =>
      typeof node.props.onPress === 'function' && node.props.accessibilityRole === 'button',
  );
}

async function pressByLabel(tree: ReactTestRenderer.ReactTestRenderer, label: string) {
  const button = pressables(tree).find(node => node.props.accessibilityLabel === label);
  if (button === undefined) throw new Error(`no button labelled ${label}`);
  await ReactTestRenderer.act(async () => {
    button.props.onPress();
  });
}

test('typing a sentence and pressing Translate reaches the native module', async () => {
  let tree: ReactTestRenderer.ReactTestRenderer | undefined;
  await ReactTestRenderer.act(async () => {
    tree = ReactTestRenderer.create(<App />);
  });

  await pressByLabel(tree!, 'Text mode');

  const input = tree!.root.find(
    node => typeof node.props.onChangeText === 'function',
  );
  await ReactTestRenderer.act(async () => {
    input.props.onChangeText('bonjour');
  });

  await pressByLabel(tree!, 'Translate');

  expect(translate).toHaveBeenCalledWith('bonjour');

  await ReactTestRenderer.act(async () => {
    tree!.unmount();
  });
});
