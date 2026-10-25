import React from 'react';
import {Pressable, ScrollView, Switch, Text, View} from 'react-native';

import {Locale, t} from '../i18n';
import {colors, spacing, styles} from '../theme';

export function SettingsScreen({
  locale,
  onLocale,
  ttsEnabled,
  onTts,
}: {
  locale: Locale;
  onLocale: (locale: Locale) => void;
  ttsEnabled: boolean;
  onTts: (enabled: boolean) => void;
}) {
  return (
    <ScrollView style={styles.screen} contentContainerStyle={{paddingVertical: spacing.m}}>
      <Text style={styles.title}>{t(locale, 'settings')}</Text>

      <View style={[styles.card, {marginTop: spacing.m}]}>
        <View style={[styles.row, {justifyContent: 'space-between'}]}>
          <Text style={styles.body}>{t(locale, 'voiceOutput')}</Text>
          <Switch value={ttsEnabled} onValueChange={onTts} />
        </View>
        <Text style={[styles.subtitle, {marginTop: spacing.s}]}>{t(locale, 'piperNote')}</Text>
      </View>

      <View style={[styles.card, {marginTop: spacing.m}]}>
        <Text style={styles.label}>{t(locale, 'language')}</Text>
        <View style={[styles.row, {marginTop: spacing.s}]}>
          {(['en', 'fr'] as const).map(code => (
            <Pressable
              key={code}
              onPress={() => onLocale(code)}
              accessibilityRole="button"
              style={[
                styles.button,
                {
                  marginRight: spacing.s,
                  backgroundColor: locale === code ? colors.accentDim : colors.surfaceAlt,
                },
              ]}>
              <Text style={styles.buttonText}>{code.toUpperCase()}</Text>
            </Pressable>
          ))}
        </View>
      </View>

      <View style={[styles.card, {marginTop: spacing.m}]}>
        <Text style={styles.title}>{t(locale, 'privacyTitle')}</Text>
        <Text style={[styles.body, {marginTop: spacing.s, fontSize: 15}]}>
          {t(locale, 'privacyBody')}
        </Text>
      </View>

      <View style={[styles.card, {marginTop: spacing.m}]}>
        <Text style={styles.title}>{t(locale, 'licensingTitle')}</Text>
        <Text style={[styles.body, {marginTop: spacing.s, fontSize: 15}]}>
          {t(locale, 'licensingBody')}
        </Text>
      </View>

      <Text style={[styles.subtitle, {marginTop: spacing.l, textAlign: 'center'}]}>
        {t(locale, 'version')}
      </Text>
    </ScrollView>
  );
}
