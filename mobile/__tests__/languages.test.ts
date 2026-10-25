import {
  DEFAULT_FROM,
  DEFAULT_TO,
  LANGUAGES,
  badges,
  canSpeakIn,
  canTranslate,
  languageById,
  searchLanguages,
  speechTag,
} from '../src/data/languages';

describe('language catalog', () => {
  it('ships the whole NLLB catalog', () => {
    expect(LANGUAGES.length).toBeGreaterThan(190);
    expect(LANGUAGES.filter(l => l.mt).length).toBeGreaterThan(190);
  });

  it('keeps French and English as usable defaults', () => {
    expect(DEFAULT_FROM.id).toBe('fr');
    expect(DEFAULT_TO.id).toBe('en');
    expect(canTranslate(DEFAULT_FROM, DEFAULT_TO)).toBe(true);
    expect(canSpeakIn(DEFAULT_FROM)).toBe(true);
  });

  it('accepts text-only pairs and rejects pairs without a model', () => {
    const hmong = languageById('hmn')!;
    const khmer = languageById('khm_Khmr')!;
    expect(hmong.mt).toBe(false);
    expect(canTranslate(hmong, DEFAULT_FROM)).toBe(false);
    expect(canTranslate(khmer, DEFAULT_FROM)).toBe(true);
    expect(canSpeakIn(khmer)).toBe(true);  // whisper has km
    expect(canTranslate(khmer, khmer)).toBe(false);
  });

  it('describes capabilities for the picker', () => {
    expect(badges(languageById('fr')!)).toEqual(['text', 'voice in']);
    // Acehnese translates but has neither a whisper code nor a voice.
    expect(badges(languageById('ace_Arab')!)).toEqual(['text']);
  });

  it('searches by name, ISO code and whisper code', () => {
    expect(searchLanguages('viet').map(l => l.id)).toContain('vie_Latn');
    expect(searchLanguages('vie').map(l => l.id)).toContain('vie_Latn');
    expect(searchLanguages('fr').length).toBeGreaterThan(0);
    expect(searchLanguages('  ').length).toBe(LANGUAGES.length);
  });

  it('maps a language to an OS speech tag', () => {
    expect(speechTag(languageById('fr')!)).toBe('fr');
    expect(speechTag(languageById('vie_Latn')!)).toBe('vi');
    expect(speechTag(languageById('zho_Hans')!)).toBe('zh');
  });
});
