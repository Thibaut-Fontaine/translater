import React, {useState} from 'react';
import {Pressable, SafeAreaView, StatusBar, Text, View} from 'react-native';

import NativeSession from './native/NativeXtransSession';
import {Locale, t} from './i18n';
import {LanguagePicker} from './screens/LanguagePicker';
import {ModelsScreen} from './screens/ModelsScreen';
import {SettingsScreen} from './screens/SettingsScreen';
import {TranslateScreen} from './screens/TranslateScreen';
import {useTranslator} from './state/useTranslator';
import {colors, spacing, styles} from './theme';

type Tab = 'translate' | 'models' | 'settings';

export default function App() {
  const translator = useTranslator();
  const [tab, setTab] = useState<Tab>('translate');
  const [locale, setLocale] = useState<Locale>('en');
  const [picking, setPicking] = useState<'from' | 'to' | null>(null);

  return (
    <SafeAreaView style={{flex: 1, backgroundColor: colors.bg}}>
     <StatusBar barStyle="light-content" />
      <View style={{flex: 1, paddingHorizontal: spacing.m}}>
        <View style={[styles.row, {paddingTop: spacing.s}]}>
          <Text style={styles.title}>{t(locale, 'appName')}</Text>
          <Text style={[styles.subtitle, {marginLeft: spacing.s, flex: 1}]}>
            {t(locale, 'tagline')}
          </Text>
          <View
            style={{
              width: 8,
              height: 8,
              borderRadius: 4,
              backgroundColor: translator.load === 'ready' ? colors.ok : colors.danger,
            }}
          />
        </View>

        {tab === 'translate' ? (
          <TranslateScreen
            translator={translator}
            locale={locale}
            onPick={side => setPicking(side)}
          />
        ) : null}
        {tab === 'models' ? (
          <ModelsScreen
            models={translator.models}
            directory={safeModelsDir()}
            locale={locale}
            onRetry={translator.retryOpen}
          />
        ) : null}
        {tab === 'settings' ? (
          <SettingsScreen
            locale={locale}
            onLocale={setLocale}
            ttsEnabled={translator.ttsEnabled}
            onTts={translator.setTtsEnabled}
          />
        ) : null}
      </View>

      <View
        style={{
          flexDirection: 'row',
          borderTopColor: colors.border,
          borderTopWidth: 1,
          paddingBottom: spacing.s,
        }}>
        {(
          [
            ['translate', 'translate'],
            ['models', 'models'],
            ['settings', 'settings'],
          ] as const
        ).map(([key, label]) => (
          <Pressable
            key={key}
            accessibilityRole="tab"
            accessibilityState={{selected: tab === key}}
            onPress={() => setTab(key)}
            style={{flex: 1, paddingVertical: spacing.m, alignItems: 'center'}}>
            <Text
              style={{
                color: tab === key ? colors.accent : colors.muted,
                fontSize: 14,
                fontWeight: tab === key ? '700' : '400',
              }}>
              {t(locale, label)}
            </Text>
          </Pressable>
        ))}
      </View>

      <LanguagePicker
        visible={picking !== null}
        side={picking ?? 'from'}
        locale={locale}
        onSelect={language => {
          if (picking === 'to') translator.chooseTo(language);
          else translator.chooseFrom(language);
        }}
        onClose={() => setPicking(null)}
      />
    </SafeAreaView>
  );
}

function safeModelsDir(): string {
  try {
    return NativeSession.modelsDir();
  } catch {
    return 'models';
  }
}
