import React from 'react';
import {Pressable, Text, View} from 'react-native';

import {colors, spacing, styles} from '../theme';
import {Phase} from '../state/sessionMachine';
import {Locale, StringKey, t} from '../i18n';
import {Language} from '../data/languages';

export function Pill({text}: {text: string}) {
  return (
    <View style={styles.pill}>
      <Text style={styles.pillText}>{text}</Text>
    </View>
  );
}

export function LanguageBadges({language, locale}: {language: Language; locale: Locale}) {
  return (
    <View style={[styles.row, {flexWrap: 'wrap', marginTop: spacing.xs}]}>
      <Pill text={t(locale, 'textOnly')} />
      {language.stt ? <Pill text={t(locale, 'voiceIn')} /> : null}
    </View>
  );
}

const phaseKey: Record<Phase, StringKey> = {
  idle: 'idleHint',
  recording: 'listening',
  transcribing: 'transcribing',
  translating: 'translating',
  speaking: 'speaking',
};

export function StatusLine({phase, locale}: {phase: Phase; locale: Locale}) {
  const active = phase !== 'idle';
  return (
    <Text
      style={{
        color: active ? colors.accent : colors.muted,
        fontSize: 14,
        textAlign: 'center',
      }}>
      {t(locale, phaseKey[phase])}
    </Text>
  );
}

export function TalkButton({
  phase,
  disabled,
  locale,
  onStart,
  onStop,
}: {
  phase: Phase;
  disabled: boolean;
  locale: Locale;
  onStart: () => void;
  onStop: () => void;
}) {
  const recording = phase === 'recording';
  return (
    <Pressable
      accessibilityRole="button"
      accessibilityLabel={t(locale, recording ? 'releaseToTranslate' : 'holdToTalk')}
      disabled={disabled}
      onPressIn={onStart}
      onPressOut={onStop}
      style={{
        backgroundColor: recording ? colors.recording : colors.accentDim,
        borderColor: recording ? colors.recording : colors.accent,
        borderWidth: 2,
        borderRadius: 999,
        height: 132,
        width: 132,
        alignSelf: 'center',
        alignItems: 'center',
        justifyContent: 'center',
        opacity: disabled ? 0.4 : 1,
      }}>
      <Text style={{color: colors.text, fontSize: 34}}>●</Text>
      <Text
        style={{
          color: colors.text,
          fontSize: 12,
          marginTop: 4,
          textAlign: 'center',
          paddingHorizontal: 8,
        }}>
        {t(locale, recording ? 'releaseToTranslate' : 'holdToTalk')}
      </Text>
    </Pressable>
  );
}
