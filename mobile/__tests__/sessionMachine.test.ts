import {
  NATIVE_STATE,
  SESSION_STATUS,
  errorForNativeMessage,
  errorForStatus,
  isBusy,
  phaseFromState,
} from '../src/state/sessionMachine';

describe('session state mapping', () => {
  it('maps every native state to a phase', () => {
    expect(phaseFromState(NATIVE_STATE.idle)).toBe('idle');
    expect(phaseFromState(NATIVE_STATE.recording)).toBe('recording');
    expect(phaseFromState(NATIVE_STATE.transcribing)).toBe('transcribing');
    expect(phaseFromState(NATIVE_STATE.translating)).toBe('translating');
    expect(phaseFromState(NATIVE_STATE.speaking)).toBe('speaking');
    expect(phaseFromState(99)).toBe('idle');
  });

  it('knows when the UI must stay busy', () => {
    expect(isBusy('idle')).toBe(false);
    expect(isBusy('recording')).toBe(true);
    expect(isBusy('translating')).toBe(true);
  });

  it('only success has no message', () => {
    expect(errorForStatus(SESSION_STATUS.ok)).toBeNull();
    expect(errorForStatus(SESSION_STATUS.noSpeech)).toContain('recognised');
    expect(errorForStatus(SESSION_STATUS.mtFailed)).toBe('Translation failed.');
  });

  it('translates the native error strings', () => {
    expect(errorForNativeMessage('')).toBeNull();
    expect(errorForNativeMessage('no speech')).toContain('recognised');
    expect(errorForNativeMessage('recognition failed')).toBe('Speech recognition failed.');
    expect(errorForNativeMessage('translation failed')).toBe('Translation failed.');
    expect(errorForNativeMessage('weird')).toBe('weird');
  });
});
