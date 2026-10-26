import {StyleSheet} from 'react-native';

export const colors = {
  bg: '#0b0f14',
  surface: '#141b23',
  surfaceAlt: '#1b242e',
  border: '#263340',
  text: '#f2f6fa',
  muted: '#93a4b5',
  accent: '#4cc2ff',
  accentDim: '#1d4d63',
  danger: '#ff6b6b',
  ok: '#5bd67a',
  warning: '#f0b429',
  recording: '#ff5a5f',
};

export const spacing = {
  xs: 4,
  s: 8,
  m: 16,
  l: 24,
  xl: 32,
};

export const styles = StyleSheet.create({
  screen: {
    flex: 1,
    backgroundColor: colors.bg,
    paddingHorizontal: spacing.m,
  },
  title: {
    color: colors.text,
    fontSize: 22,
    fontWeight: '700',
  },
  subtitle: {
    color: colors.muted,
    fontSize: 14,
  },
  card: {
    backgroundColor: colors.surface,
    borderColor: colors.border,
    borderWidth: 1,
    borderRadius: 16,
    padding: spacing.m,
  },
  label: {
    color: colors.muted,
    fontSize: 12,
    letterSpacing: 1,
    textTransform: 'uppercase',
    marginBottom: spacing.xs,
  },
  body: {
    color: colors.text,
    fontSize: 18,
    lineHeight: 26,
  },
  translation: {
    color: colors.text,
    fontSize: 26,
    lineHeight: 34,
    fontWeight: '600',
  },
  button: {
    backgroundColor: colors.accentDim,
    borderColor: colors.accent,
    borderWidth: 1,
    borderRadius: 12,
    paddingVertical: spacing.s,
    paddingHorizontal: spacing.m,
    alignItems: 'center',
  },
  buttonText: {
    color: colors.text,
    fontSize: 16,
    fontWeight: '600',
  },
  ghost: {
    paddingVertical: spacing.s,
    paddingHorizontal: spacing.s,
  },
  ghostText: {
    color: colors.accent,
    fontSize: 15,
  },
  pill: {
    borderRadius: 999,
    borderWidth: 1,
    borderColor: colors.border,
    paddingHorizontal: spacing.s,
    paddingVertical: 2,
    marginRight: spacing.xs,
  },
  pillText: {
    color: colors.muted,
    fontSize: 11,
  },
  row: {
    flexDirection: 'row',
    alignItems: 'center',
  },
  error: {
    color: colors.danger,
    fontSize: 14,
  },
});
