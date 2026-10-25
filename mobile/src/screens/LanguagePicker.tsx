import React, {useMemo, useState} from 'react';
import {FlatList, Modal, Pressable, SafeAreaView, Text, TextInput, View} from 'react-native';

import {Language, LANGUAGES, badges} from '../data/languages';
import {Locale, t} from '../i18n';
import {colors, spacing, styles} from '../theme';

export function LanguagePicker({
  visible,
  side,
  locale,
  onSelect,
  onClose,
}: {
  visible: boolean;
  side: 'from' | 'to';
  locale: Locale;
  onSelect: (language: Language) => void;
  onClose: () => void;
}) {
  const [query, setQuery] = useState('');

  const pool = useMemo(() => {
    const q = query.trim().toLowerCase();
    return LANGUAGES.filter(l => l.mt).filter(
      l =>
        q.length === 0 ||
        l.name.toLowerCase().includes(q) ||
        l.iso.toLowerCase().includes(q) ||
        l.whisper === q,
    );
  }, [query]);

  return (
    <Modal visible={visible} animationType="slide" onRequestClose={onClose}>
      <SafeAreaView style={[styles.screen, {paddingTop: spacing.m}]}>
        <View style={[styles.row, {marginBottom: spacing.m}]}>
          <Text style={[styles.title, {flex: 1}]}>
            {t(locale, 'chooseLanguage')} · {t(locale, side === 'from' ? 'source' : 'target')}
          </Text>
          <Pressable onPress={onClose} style={styles.ghost} accessibilityRole="button">
            <Text style={styles.ghostText}>{t(locale, 'close')}</Text>
          </Pressable>
        </View>

        <TextInput
          value={query}
          onChangeText={setQuery}
          placeholder={t(locale, 'search')}
          placeholderTextColor={colors.muted}
          autoCorrect={false}
          style={{
            backgroundColor: colors.surface,
            borderColor: colors.border,
            borderWidth: 1,
            borderRadius: 12,
            color: colors.text,
            fontSize: 16,
            padding: spacing.m,
            marginBottom: spacing.m,
          }}
        />

        <FlatList
          data={pool}
          keyExtractor={item => item.id}
          keyboardShouldPersistTaps="handled"
          renderItem={({item}) => (
            <Pressable
              accessibilityRole="button"
              onPress={() => {
                onSelect(item);
                setQuery('');
                onClose();
              }}
              style={{
                paddingVertical: spacing.m,
                borderBottomColor: colors.border,
                borderBottomWidth: 1,
              }}>
              <Text style={{color: colors.text, fontSize: 17}}>{item.name}</Text>
              <View style={[styles.row, {marginTop: spacing.xs}]}>
                {badges(item).map(b => (
                  <View key={b} style={styles.pill}>
                    <Text style={styles.pillText}>
                      {b === 'text' ? t(locale, 'textOnly') : t(locale, 'voiceIn')}
                    </Text>
                  </View>
                ))}
                <Text style={[styles.subtitle, {marginLeft: spacing.s}]}>{item.nllb}</Text>
              </View>
            </Pressable>
          )}
        />
      </SafeAreaView>
    </Modal>
  );
}
