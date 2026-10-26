import React from 'react';
import ReactTestRenderer from 'react-test-renderer';

/**
 * A missing TurboModule must not take the process down.
 *
 * `TurboModuleRegistry.getEnforcing` throws while the JS bundle is still being
 * evaluated, before any React tree exists: on a release build that is an
 * instant exit with nothing on screen. The app has to render an explanation
 * instead, because "the module did not register" is a build bug the user can
 * report, not a reason to close.
 */
jest.mock('../src/native/NativeXtransSession', () => ({
  __esModule: true,
  nativeModuleAvailable: false,
  default: null,
}));

import App from '../src/App';

test('renders an explanation instead of crashing when the module is absent', async () => {
  let tree: ReactTestRenderer.ReactTestRenderer | undefined;
  await ReactTestRenderer.act(async () => {
    tree = ReactTestRenderer.create(<App />);
  });

  const rendered = JSON.stringify(tree!.toJSON());
  expect(rendered).toMatch(/native/i);
  expect(rendered).toMatch(/missing|unavailable/i);

  await ReactTestRenderer.act(async () => {
    tree!.unmount();
  });
});
