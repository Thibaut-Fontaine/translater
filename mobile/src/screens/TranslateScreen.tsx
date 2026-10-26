import React, {useState} from 'react';
import {
  Clipboard,
  Pressable,
  ScrollView,
  Text,
  TextInput,
  View,
} from 'react-native';

import {TalkButton, StatusLine} from '../components/UI';
import {Language, canSpeakIn} from '../data/languages';
import {Locale, t} from '../i18n';
import {Translator} from '../state/useTranslator';
import {colors, spacing, styles} from '../theme';

export function TranslateScreen({
  translator,
  locale,
  onPick,
}: {
  translator: Translator;
  locale: Locale;
  onPick: (side: 'from' | 'to') => void;
}) {
  const [typing, setTyping] = useState(false);
  const [copied, setCopied] = useState(false);
  const {
    from,
    to,
    phase,
    transcript,
    translation,
    error,
    load,
    draft,
    setDraft,
    beginVoice,
    finishVoice,
    translateTyped,
    cancel,
    swap,
  } = translator;

  const ready = load === 'ready' || load === 'idle';
  const busy = phase !== 'idle';
  const canVoice = canSpeakIn(from);

  const copy = () => {
    if (translation.length === 0) return;
    Clipboard.setString(translation);
    setCopied(true);
    setTimeout(() => setCopied(false), 1200);
  };

  return (
    <View style={{flex: 1}}>
      <View style={[styles.row, {marginVertical: spacing.m}]}>
        <LanguageButton language={from} onPress={() => onPick('from')} label={t(locale, 'source')} />
        <Pressable
          accessibilityRole="button"
          accessibilityLabel={t(locale, 'swap')}
          onPress={swap}
          style={styles.ghost}>
          <Text style={{color: colors.accent, fontSize: 22}}>⇄</Text>
        </Pressable>
        <LanguageButton language={to} onPress={() => onPick('to')} label={t(locale, 'target')} />
      </View>

      {error !== null ? (
        <Text style={[styles.error, {marginBottom: spacing.s}]}>{describeError(error, locale)}</Text>
      ) : null}

      <ScrollView
        style={{flex: 1}}
        contentContainerStyle={{paddingBottom: spacing.m}}
        keyboardShouldPersistTaps="handled">
        {typing ? (
          <View style={{marginBottom: spacing.m}}>
            <TextInput
              value={draft}
              onChangeText={setDraft}
              placeholder={t(locale, 'typePlaceholder')}
              placeholderTextColor={colors.muted}
              multiline
              autoCorrect={false}
              style={{
                backgroundColor: colors.surfaceAlt,
                borderColor: colors.border,
                borderWidth: 1,
                borderRadius: 12,
                color: colors.text,
                fontSize: 18,
                minHeight: 88,
                padding: spacing.m,
                textAlignVertical: 'top',
              }}
            />
          </View>
        ) : null}

        <View style={[styles.card, {marginBottom: spacing.m}]}>
          <Text style={styles.label}>{t(locale, 'transcript')}</Text>
          <Text style={styles.body} selectable>
            {transcript.length > 0 ? transcript : t(locale, 'noTranscript')}
          </Text>
        </View>

        <View style={styles.card}>
          <View style={styles.row}>
            <Text style={[styles.label, {flex: 1}]}>{t(locale, 'translation')}</Text>
            {translation.length > 0 ? (
              <Pressable onPress={copy} style={styles.ghost} accessibilityRole="button">
                <Text style={styles.ghostText}>
                  {copied ? t(locale, 'copied') : t(locale, 'copy')}
                </Text>
              </Pressable>
            ) : null}
          </View>
          <Text style={styles.translation} selectable>
            {translation.length > 0 ? translation : t(locale, 'noTranslation')}
          </Text>
        </View>

      </ScrollView>

      <View style={{paddingBottom: spacing.l, paddingTop: spacing.s}}>
        {load === 'loading' ? (
          <Text style={[styles.subtitle, {textAlign: 'center', marginBottom: spacing.s}]}>
            {t(locale, 'loadingModels')}
          </Text>
        ) : null}
        <StatusLine phase={phase} locale={locale} />
        <View style={{height: spacing.m}} />
        {typing ? (
          <Pressable
            accessibilityRole="button"
            accessibilityLabel={t(locale, 'translate')}
            onPress={() => translateTyped(draft)}
            disabled={busy || draft.trim().length === 0}
            style={[
              styles.button,
              {paddingVertical: spacing.m, opacity: busy || draft.trim().length === 0 ? 0.5 : 1},
            ]}>
            <Text style={styles.buttonText}>{t(locale, 'translate')}</Text>
          </Pressable>
        ) : (
          <TalkButton
            phase={phase}
            disabled={!ready || !canVoice}
            locale={locale}
            onStart={beginVoice}
            onStop={finishVoice}
          />
        )}
        <View style={[styles.row, {justifyContent: 'center', marginTop: spacing.m}]}>
          <Pressable
            onPress={() => {
              if (busy) cancel();
              setTyping(v => !v);
            }}
            style={styles.ghost}
            accessibilityRole="button"
            accessibilityLabel={typing ? t(locale, 'showVoice') : t(locale, 'showText')}>
            <Text style={styles.ghostText}>
              {typing ? t(locale, 'showVoice') : t(locale, 'showText')}
            </Text>
          </Pressable>
          {busy ? (
            <Pressable
              onPress={cancel}
              style={styles.ghost}
              accessibilityRole="button"
              accessibilityLabel={t(locale, 'cancel')}>
              <Text style={[styles.ghostText, {color: colors.danger}]}>{t(locale, 'cancel')}</Text>
            </Pressable>
          ) : null}
        </View>
        {!canVoice ? (
          <Text style={[styles.subtitle, {textAlign: 'center'}]}>
            {t(locale, 'speechUnavailable')}
          </Text>
        ) : null}
      </View>
    </View>
  );
}

function LanguageButton({
  language,
  label,
  onPress,
}: {
  language: Language;
  label: string;
  onPress: () => void;
}) {
  return (
    <Pressable
      accessibilityRole="button"
      onPress={onPress}
      style={{flex: 1, alignItems: 'center'}}>
      <Text style={styles.label}>{label}</Text>
      <Text style={{color: colors.text, fontSize: 18, fontWeight: '600'}} numberOfLines={1}>
        {language.name}
      </Text>
    </Pressable>
  );
}

// The C++ core reports stable English strings; the app localizes them.
function describeError(message: string, locale: Locale): string {
  const map: Record<string, Parameters<typeof t>[1]> = {
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
    'Translation engine is not ready.': 'engineNotReady',
  };
  const key = map[message];
  return key !== undefined ? t(locale, key) : message;
}
