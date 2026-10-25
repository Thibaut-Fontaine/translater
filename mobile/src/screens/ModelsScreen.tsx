import React from 'react';
import {Pressable, ScrollView, Text, View} from 'react-native';

import {Locale, t} from '../i18n';
import {ModelStatus} from '../state/useTranslator';
import {colors, spacing, styles} from '../theme';

export function ModelsScreen({
  models,
  directory,
  locale,
  onRetry,
}: {
  models: ModelStatus[];
  directory: string;
  locale: Locale;
  onRetry: () => void;
}) {
  const missing = models.filter(m => !m.present).length;
  return (
    <ScrollView style={styles.screen} contentContainerStyle={{paddingVertical: spacing.m}}>
      <Text style={styles.title}>{t(locale, 'models')}</Text>
      <Text style={[styles.subtitle, {marginTop: spacing.s}]}>{t(locale, 'modelsHint')}</Text>

      <View style={[styles.card, {marginTop: spacing.m}]}>
        <Text style={styles.label}>{t(locale, 'modelsFolder')}</Text>
        <Text style={[styles.body, {fontSize: 13}]} selectable>
          {directory}
        </Text>
      </View>

      <View style={[styles.card, {marginTop: spacing.m}]}>
        {models.length === 0 ? (
          <Text style={styles.body}>{t(locale, 'loadingModels')}</Text>
        ) : (
          models.map(model => (
            <View
              key={model.name}
              style={{
                flexDirection: 'row',
                paddingVertical: spacing.s,
                borderBottomColor: colors.border,
                borderBottomWidth: 1,
              }}>
              <Text style={{color: colors.text, flex: 1, fontSize: 14}} numberOfLines={1}>
                {model.name}
              </Text>
              <Text
                style={{
                  color: model.present ? colors.ok : colors.danger,
                  fontSize: 13,
                  marginLeft: spacing.s,
                }}>
                {model.present
                  ? `${t(locale, 'modelPresent')} · ${formatBytes(model.bytes)}`
                  : t(locale, 'modelMissing')}
              </Text>
            </View>
          ))
        )}
      </View>

      {missing > 0 ? (
        <Text style={[styles.subtitle, {marginTop: spacing.m}]}>
          adb push models/ /sdcard/Android/data/com.xtrans.app/files/models/
        </Text>
      ) : null}

      <Pressable onPress={onRetry} style={[styles.button, {marginTop: spacing.l}]} accessibilityRole="button">
        <Text style={styles.buttonText}>{t(locale, 'retry')}</Text>
      </Pressable>

      <Text style={[styles.subtitle, {marginTop: spacing.l, fontSize: 12}]}>
        encoder_model_int8.onnx · decoder_model_quantized.onnx ·
        decoder_with_past_model_quantized.onnx · nllb_bpe.bin · ggml-base-q5_1.bin
      </Text>
    </ScrollView>
  );
}

function formatBytes(bytes: number): string {
  if (bytes >= 1024 * 1024 * 1024) return `${(bytes / (1024 * 1024 * 1024)).toFixed(1)} GB`;
  if (bytes >= 1024 * 1024) return `${(bytes / (1024 * 1024)).toFixed(0)} MB`;
  return `${bytes} B`;
}
