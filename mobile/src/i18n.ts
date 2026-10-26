export type Locale = 'en' | 'fr';

const en = {
  appName: 'Xtrans',
  tagline: 'Offline translator',
  holdToTalk: 'Hold to talk',
  releaseToTranslate: 'Release to translate',
  listening: 'Listening…',
  transcribing: 'Recognising…',
  translating: 'Translating…',
  speaking: 'Speaking…',
  idleHint: 'Hold the button and speak, or type.',
  transcript: 'Original',
  translation: 'Translation',
  noTranscript: 'Nothing yet.',
  noTranslation: 'Nothing yet.',
  type: 'Type',
  typePlaceholder: 'Type text to translate…',
  translate: 'Translate',
  cancel: 'Cancel',
  swap: 'Swap',
  chooseLanguage: 'Choose a language',
  source: 'From',
  target: 'To',
  search: 'Search 200+ languages',
  models: 'Models',
  modelsHint:
    'Everything runs on this phone. Download the models once; nothing is uploaded.',
  modelPresent: 'installed',
  modelMissing: 'missing',
  modelsInstalled: 'All model files are installed.',
  modelsTotal: 'total, downloaded once',
  modelsToDownload: 'to download',
  downloadModels: 'Download models',
  cancelDownload: 'Cancel download',
  downloadHint:
    'Over Wi-Fi, one file at a time. Downloads resume where they stopped; keep the app open until the last file is verified.',
  downloadLicence:
    'NLLB-200 is CC-BY-NC 4.0 (personal, non-commercial use). Whisper models are MIT. Files are verified against a SHA-256 digest.',
  readyState: 'Installed',
  missingState: 'Missing',
  queuedState: 'Waiting…',
  downloadingState: 'Downloading',
  verifyingState: 'Verifying…',
  failedState: 'Failed',
  cancelledState: 'Cancelled',
  modelsNotInstalled: 'Models are not installed. Open the Models tab to download them.',
  notEnoughMemory:
    'Not enough free memory to load the models. Close other apps and try again.',

  modelsFolder: 'Models folder',
  retry: 'Reload models',
  settings: 'Settings',
  voiceOutput: 'Speak the translation',
  language: 'App language',
  privacyTitle: 'Privacy',
  privacyBody:
    'No account, no telemetry, no network at runtime. The microphone is only open while you hold the button. Transcripts stay in memory unless you copy them.',
  licensingTitle: 'Licences',
  licensingBody:
    'App: MIT. Translation model: NLLB-200 (CC-BY-NC 4.0), personal use only. Speech: whisper.cpp (MIT).',
  loadingModels: 'Loading models…',
  modelsFailed: 'Models could not be loaded.',
  notReady: 'Models are not ready.',
  speechUnavailable: 'This language has no speech recognition. Type instead.',
  micDenied: 'Microphone permission is required to translate speech.',
  busy: 'A turn is already running.',
  textOnly: 'text only',
  voiceIn: 'voice in',
  unsupportedPair: 'This pair cannot be translated.',
  emptyText: 'Type something first.',
  noSpeech: 'Nothing was recognised. Try again closer to the microphone.',
  sttFailed: 'Speech recognition failed.',
  mtFailed: 'Translation failed.',
  unknownError: 'Unknown error.',
  nativeMissing: 'Native module missing',
  nativeMissingHint:
    'The XtransSession module did not register in this build. Reinstall a complete APK; the app cannot translate without it.',
  copied: 'Copied',
  copy: 'Copy',
  speakAgain: 'Speak again',
  showText: 'Text mode',
  showVoice: 'Voice mode',
  close: 'Close',
  version: 'Version 0.1.0 · MIT',
  piperNote: 'Voices come from the system speech engine; install one for the target language to hear it.',
};

const fr: typeof en = {
  appName: 'Xtrans',
  tagline: 'Traducteur hors ligne',
  holdToTalk: 'Maintenir pour parler',
  releaseToTranslate: 'Relâcher pour traduire',
  listening: 'Écoute…',
  transcribing: 'Reconnaissance…',
  translating: 'Traduction…',
  speaking: 'Lecture…',
  idleHint: 'Maintenez le bouton et parlez, ou écrivez.',
  transcript: 'Original',
  translation: 'Traduction',
  noTranscript: 'Rien pour le moment.',
  noTranslation: 'Rien pour le moment.',
  type: 'Écrire',
  typePlaceholder: 'Texte à traduire…',
  translate: 'Traduire',
  cancel: 'Annuler',
  swap: 'Inverser',
  chooseLanguage: 'Choisir une langue',
  source: 'De',
  target: 'Vers',
  search: 'Rechercher parmi 200 langues',
  models: 'Modèles',
  modelsHint:
    'Tout tourne sur ce téléphone. Téléchargez les modèles une fois ; rien n’est envoyé.',
  modelPresent: 'installé',
  modelMissing: 'manquant',
  modelsInstalled: 'Tous les modèles sont installés.',
  modelsTotal: 'au total, téléchargés une fois',
  modelsToDownload: 'à télécharger',
  downloadModels: 'Télécharger les modèles',
  cancelDownload: 'Annuler le téléchargement',
  downloadHint:
    'En Wi-Fi, un fichier à la fois. Le téléchargement reprend où il s’est arrêté ; gardez l’application ouverte jusqu’à la vérification du dernier fichier.',
  downloadLicence:
    'NLLB-200 est sous CC-BY-NC 4.0 (usage personnel, non commercial). Les modèles Whisper sont MIT. Chaque fichier est vérifié par empreinte SHA-256.',
  readyState: 'Installé',
  missingState: 'Manquant',
  queuedState: 'En attente…',
  downloadingState: 'Téléchargement',
  verifyingState: 'Vérification…',
  failedState: 'Échec',
  cancelledState: 'Annulé',
  modelsNotInstalled:
    "Les modèles ne sont pas installés. Ouvrez l'onglet Modèles pour les télécharger.",
  notEnoughMemory:
    "Mémoire libre insuffisante pour charger les modèles. Fermez d'autres applications et réessayez.",

  modelsFolder: 'Dossier des modèles',
  retry: 'Recharger',
  settings: 'Réglages',
  voiceOutput: 'Lire la traduction',
  language: "Langue de l'application",
  privacyTitle: 'Confidentialité',
  privacyBody:
    "Pas de compte, pas de télémétrie, pas de réseau à l'exécution. Le microphone n'est ouvert que pendant l'appui. Les textes restent en mémoire.",
  licensingTitle: 'Licences',
  licensingBody:
    'Application : MIT. Modèle de traduction : NLLB-200 (CC-BY-NC 4.0), usage personnel. Reconnaissance : whisper.cpp (MIT).',
  loadingModels: 'Chargement des modèles…',
  modelsFailed: 'Impossible de charger les modèles.',
  notReady: 'Les modèles ne sont pas prêts.',
  speechUnavailable:
    "Cette langue n'a pas de reconnaissance vocale. Écrivez plutôt.",
  micDenied: 'Le micro est nécessaire pour traduire la parole.',
  busy: 'Une traduction est déjà en cours.',
  textOnly: 'texte seul',
  voiceIn: 'voix',
  unsupportedPair: 'Cette paire ne peut pas être traduite.',
  emptyText: "Écrivez d'abord quelque chose.",
  noSpeech: 'Rien reconnu. Réessayez plus près du micro.',
  sttFailed: 'La reconnaissance a échoué.',
  mtFailed: 'La traduction a échoué.',
  unknownError: 'Erreur inconnue.',
  nativeMissing: 'Module natif absent',
  nativeMissingHint:
    "Le module XtransSession ne s'est pas enregistré dans cette version. Réinstallez une APK complète ; sans lui, l'application ne peut pas traduire.",
  copied: 'Copié',
  copy: 'Copier',
  speakAgain: 'Réécouter',
  showText: 'Mode texte',
  showVoice: 'Mode voix',
  close: 'Fermer',
  version: 'Version 0.1.0 · MIT',
  piperNote:
    "La voix vient du moteur du système ; installez une voix pour la langue cible pour l'entendre.",
};

const tables = {en, fr};

export type StringKey = keyof typeof en;

export function t(locale: Locale, key: StringKey): string {
  return tables[locale][key] ?? tables.en[key] ?? key;
}

/** Maps the native English message to a localized key. */
const errorKeys: Record<string, StringKey> = {
  'A turn is already running.': 'busy',
  'This language pair cannot be translated.': 'unsupportedPair',
  'Type something first.': 'emptyText',
  'Nothing was recognised. Try again closer to the microphone.': 'noSpeech',
  'Speech recognition failed.': 'sttFailed',
  'Translation failed.': 'mtFailed',
  'Models are not loaded yet.': 'notReady',
  'This language has no speech recognition. Type instead.': 'speechUnavailable',
  'Microphone permission is required to translate speech.': 'micDenied',
  'Models are not installed.': 'modelsNotInstalled',
  'Not enough free memory (need about 800 MB).': 'notEnoughMemory',
};

export function errorKey(message: string | null): StringKey {
  if (message === null) return 'unknownError';
  return errorKeys[message] ?? 'unknownError';
}
