import React from 'react';
import ReactTestRenderer from 'react-test-renderer';

/**
 * The Models screen must offer the download and show progress, because a fresh
 * install has no files at all and the app cannot translate without them.
 */
jest.mock('../src/native/NativeXtransSession', () => ({
  __esModule: true,
  nativeModuleAvailable: true,
  default: {
    isReady: jest.fn(() => false),
    modelsDir: jest.fn(() => '/models'),
    openModels: jest.fn(async () => false),
    modelsStatus: jest.fn(() =>
      JSON.stringify([
        {
          name: 'nllb_bpe.bin',
          present: false,
          bytes: 12587091,
          state: 'missing',
          received: 0,
          total: 12587091,
          error: null,
        },
        {
          name: 'ggml-base-q5_1.bin',
          present: false,
          bytes: 59707625,
          state: 'missing',
          received: 0,
          total: 59707625,
          error: null,
        },
      ]),
    ),
    downloadModels: jest.fn(() => true),
    cancelDownloads: jest.fn(),
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

const downloadModels = NativeSession!.downloadModels as jest.Mock;

function buttons(tree: ReactTestRenderer.ReactTestRenderer) {
  return tree.root.findAll(
    node =>
      typeof node.props.onPress === 'function' &&
      (node.props.accessibilityRole === 'button' || node.props.accessibilityRole === 'tab'),
  );
}

async function press(tree: ReactTestRenderer.ReactTestRenderer, matcher: string) {
  const button = buttons(tree).find(
    node =>
      node.props.accessibilityLabel === matcher ||
      flatten(node.props.children).includes(matcher),
  );
  if (button === undefined) throw new Error(`no button matching ${matcher}`);
  await ReactTestRenderer.act(async () => {
    button.props.onPress();
  });
}

function flatten(node: unknown): string {
  if (node == null) return '';
  if (typeof node === 'string') return node;
  if (typeof node === 'number') return String(node);
  if (Array.isArray(node)) return node.map(flatten).join('');
  if (typeof node === 'object') {
    const value = node as {children?: unknown; props?: {children?: unknown}};
    return flatten(value.children) + flatten(value.props?.children);
  }
  return '';
}

test('a fresh install offers to download the models', async () => {
  let tree: ReactTestRenderer.ReactTestRenderer | undefined;
  await ReactTestRenderer.act(async () => {
    tree = ReactTestRenderer.create(<App />);
  });

  await press(tree!, 'Models');
  expect(flatten(tree!.toJSON())).toContain('Download');

  await press(tree!, 'Download models');
  expect(downloadModels).toHaveBeenCalledTimes(1);

  await ReactTestRenderer.act(async () => {
    tree!.unmount();
  });
});
