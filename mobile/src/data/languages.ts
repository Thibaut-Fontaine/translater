import raw from './languages.json';

export type Language = {
  /** Index into the C++ enum; what the native module expects. */
  value: number;
  /** Stable enum name, e.g. "vie_Latn". */
  id: string;
  name: string;
  /** ISO 639-3, used for filenames. */
  iso: string;
  /** NLLB code, empty when translation is impossible. */
  nllb: string;
  /** whisper.cpp code, empty when the language cannot be spoken. */
  whisper: string;
  nllbId: number;
  stt: boolean;
  mt: boolean;
  /** A validated voice ships with the Pi build; the phone uses the OS engine. */
  tts: boolean;
};

type RawLanguage = {
  value: number;
  id: string;
  name: string;
  iso: string;
  nllb: string;
  whisper: string;
  nllb_id: number;
  stt: boolean;
  mt: boolean;
  tts: boolean;
};

const RAW = raw as unknown as {languages: RawLanguage[]};

export const LANGUAGES: Language[] = RAW.languages.map(l => ({
  value: l.value,
  id: l.id,
  name: l.name,
  iso: l.iso,
  nllb: l.nllb,
  whisper: l.whisper,
  nllbId: l.nllb_id,
  stt: l.stt,
  mt: l.mt,
  tts: l.tts,
}));

const BY_VALUE = new Map<number, Language>(LANGUAGES.map(l => [l.value, l]));
const BY_ID = new Map<string, Language>(LANGUAGES.map(l => [l.id, l]));

export function languageByValue(value: number): Language | undefined {
  return BY_VALUE.get(value);
}

export function languageById(id: string): Language | undefined {
  return BY_ID.get(id);
}

/** French and English exist in every build; they are the safe defaults. */
export const DEFAULT_FROM: Language = languageById('fr') ?? LANGUAGES[0];
export const DEFAULT_TO: Language = languageById('en') ?? LANGUAGES[1];

/** Both sides need an NLLB token: that is all text-to-text needs. */
export function canTranslate(a: Language, b: Language): boolean {
  return a.value !== b.value && a.mt && b.mt;
}

export function canSpeakIn(language: Language): boolean {
  return language.stt && language.mt;
}

export function isTranslatableTarget(language: Language): boolean {
  return language.mt;
}

/**
 * BCP-47 tag handed to the OS speech engine. whisper codes are two letters for
 * every language we can speak; three-letter codes (NLLB scripted names) fall
 * back to the ISO code and the OS decides whether it has a voice.
 */
export function speechTag(language: Language): string {
  if (language.whisper.length === 2) return language.whisper;
  if (language.whisper.length === 3) return language.whisper;
  return language.iso;
}

/** Search is by English name and by code, case-insensitive. */
export function searchLanguages(query: string, pool: Language[] = LANGUAGES): Language[] {
  const q = query.trim().toLowerCase();
  if (q.length === 0) return pool;
  return pool.filter(
    l =>
      l.name.toLowerCase().includes(q) ||
      l.iso.toLowerCase().includes(q) ||
      l.nllb.toLowerCase().includes(q) ||
      l.whisper.toLowerCase() === q,
  );
}

/** What the picker shows next to a language. */
export function badges(language: Language): string[] {
  const out: string[] = [];
  if (language.mt) out.push('text');
  if (language.stt) out.push('voice in');
  return out;
}
