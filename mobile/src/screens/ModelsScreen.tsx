import React from 'react';
import {Pressable, ScrollView, Text, View} from 'react-native';

import {Locale, StringKey, t} from '../i18n';
import {
  ModelStatus,
  allReady,
  anyDownloading,
  formatBytes,
  label,
  missingBytes,
  percent,
  totalBytes,
} from '../models/manifest';
import {colors, spacing, styles} from '../theme';

const STATE_KEY: Record<ModelStatus['state'], StringKey> = {
  missing: 'missingState',
  queued: 'queuedState',
  downloading: 'downloadingState',
  verifying: 'verifyingState',
  ready: 'readyState',
  failed: 'failedState',
  cancelled: 'cancelledState',
};

export function ModelsScreen({
  models,
  directory,
  locale,
  downloading,
  onRetry,
  onDownload,
  onCancel,
}: {
  models: ModelStatus[];
  directory: string;
  locale: Locale;
  downloading: boolean;
  onRetry: () => void;
  onDownload: () => void;
  onCancel: () => void;
}) {
  const missing = models.filter(model => !model.present && model.state !== 'ready');
  const busy = downloading || anyDownloading(models);
  const installed = allReady(models);

  return (
    <ScrollView style={styles.screen} contentContainerStyle={{paddingVertical: spacing.m}}>
      <Text style={styles.title}>{t(locale, 'models')}</Text>
      <Text style={[styles.subtitle, {marginTop: spacing.s}]}>{t(locale, 'modelsHint')}</Text>

      <View style={[styles.card, {marginTop: spacing.m}]}>
        <Text style={styles.body}>
          {installed
            ? t(locale, 'modelsInstalled')
            : `${models.length - missing.length} / ${models.length} · ${formatBytes(missingBytes(models))} ${t(locale, 'modelsToDownload')}`}
        </Text>
        <Text style={[styles.subtitle, {marginTop: spacing.xs}]}>
          {formatBytes(totalBytes(models))} {t(locale, 'modelsTotal')}
        </Text>

        {missing.length > 0 && !busy ? (
          <Pressable
            accessibilityRole="button"
            accessibilityLabel={t(locale, 'downloadModels')}
            onPress={onDownload}
            style={[styles.button, {marginTop: spacing.m, paddingVertical: spacing.m}]}>
            <Text style={styles.buttonText}>
              {t(locale, 'downloadModels')} · {formatBytes(missingBytes(models))}
            </Text>
          </Pressable>
        ) : null}

        {busy ? (
          <Pressable
            accessibilityRole="button"
            accessibilityLabel={t(locale, 'cancelDownload')}
            onPress={onCancel}
            style={[styles.button, {marginTop: spacing.m, backgroundColor: colors.surfaceAlt}]}>
            <Text style={styles.buttonText}>{t(locale, 'cancelDownload')}</Text>
          </Pressable>
        ) : null}

        <Text style={[styles.subtitle, {marginTop: spacing.s, fontSize: 12}]}>
          {t(locale, 'downloadHint')}
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
                paddingVertical: spacing.s,
                borderBottomColor: colors.border,
                borderBottomWidth: 1,
              }}>
              <View style={{flexDirection: 'row'}}>
                <Text style={{color: colors.text, flex: 1, fontSize: 15}} numberOfLines={1}>
                  {label(model.name)}
                </Text>
                <Text style={{color: colors.muted, fontSize: 13, marginLeft: spacing.s}}>
                  {formatBytes(model.bytes)}
                </Text>
              </View>
              <View style={[styles.row, {marginTop: 2}]}>
                <Text
                  style={{
                    color:
                      model.present || model.state === 'ready'
                        ? colors.ok
                        : model.state === 'failed'
                          ? colors.danger
                          : colors.warning,
                    fontSize: 12,
                    flex: 1,
                  }}>
                  {t(locale, STATE_KEY[model.state])}
                  {model.state === 'downloading' ? ` · ${percent(model)}%` : ''}
                  {model.error != null ? ` · ${model.error}` : ''}
                </Text>
                {model.state === 'downloading' ? (
                  <Text style={{color: colors.muted, fontSize: 12}}>
                    {formatBytes(model.received)} / {formatBytes(model.total)}
                  </Text>
                ) : null}
              </View>
              {model.state === 'downloading' || model.state === 'verifying' ? (
                <View
                  style={{
                    height: 3,
                    backgroundColor: colors.surfaceAlt,
                    borderRadius: 2,
                    marginTop: 4,
                  }}>
                  <View
                    style={{
                      height: 3,
                      width: `${percent(model)}%`,
                      backgroundColor: colors.accent,
                      borderRadius: 2,
                    }}
                  />
                </View>
              ) : null}
            </View>
          ))
        )}
      </View>

      <Text style={[styles.subtitle, {marginTop: spacing.m, fontSize: 12}]}>
        {t(locale, 'downloadLicence')}
      </Text>

      <View style={[styles.card, {marginTop: spacing.m}]}>
        <Text style={styles.label}>{t(locale, 'modelsFolder')}</Text>
        <Text style={[styles.body, {fontSize: 13}]} selectable>
          {directory}
        </Text>
      </View>

      <Pressable
        onPress={onRetry}
        style={[styles.button, {marginTop: spacing.l}]}
        accessibilityRole="button"
        accessibilityLabel={t(locale, 'retry')}>
        <Text style={styles.buttonText}>{t(locale, 'retry')}</Text>
      </Pressable>
    </ScrollView>
  );
}
