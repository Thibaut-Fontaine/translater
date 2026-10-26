/**
 * Model files as the native downloader reports them.
 *
 * The native side owns the URLs and the SHA-256 digests (one source of truth);
 * this module only labels them and turns the JSON into something renderable.
 */
export type ModelState =
  | 'missing'
  | 'queued'
  | 'downloading'
  | 'verifying'
  | 'ready'
  | 'failed'
  | 'cancelled';

export type ModelStatus = {
  name: string;
  present: boolean;
  bytes: number;
  state: ModelState;
  received: number;
  total: number;
  error: string | null;
};

const STATES: ReadonlySet<string> = new Set([
  'missing',
  'queued',
  'downloading',
  'verifying',
  'ready',
  'failed',
  'cancelled',
]);

/** Display labels; the native manifest is authoritative for names and sizes. */
export const MODEL_LABELS: Record<string, string> = {
  'nllb_bpe.bin': 'NLLB tokenizer',
  'ggml-base-q5_1.bin': 'Whisper base (q5)',
  'encoder_model_int8.onnx': 'NLLB encoder',
  'decoder_model_quantized.onnx': 'NLLB decoder',
  'decoder_with_past_model_quantized.onnx': 'NLLB decoder (KV cache)',
};

export function parseModels(json: string): ModelStatus[] {
  try {
    const raw = JSON.parse(json);
    if (!Array.isArray(raw)) return [];
    return raw
      .filter((entry): entry is Record<string, unknown> => typeof entry === 'object' && entry !== null)
      .map(entry => ({
        name: String(entry.name ?? ''),
        present: entry.present === true,
        bytes: toNumber(entry.bytes),
        state: STATES.has(String(entry.state)) ? (entry.state as ModelState) : 'missing',
        received: toNumber(entry.received),
        total: toNumber(entry.total),
        error: entry.error == null ? null : String(entry.error),
      }))
      .filter(model => model.name.length > 0);
  } catch {
    return [];
  }
}

function toNumber(value: unknown): number {
  const n = typeof value === 'number' ? value : Number(value);
  return Number.isFinite(n) && n > 0 ? n : 0;
}

export function isDownloading(model: ModelStatus): boolean {
  return model.state === 'downloading' || model.state === 'verifying' || model.state === 'queued';
}

export function anyDownloading(models: ModelStatus[]): boolean {
  return models.some(isDownloading);
}

export function allReady(models: ModelStatus[]): boolean {
  return models.length > 0 && models.every(model => model.present || model.state === 'ready');
}

export function percent(model: ModelStatus): number {
  if (model.state === 'ready' || model.present) return 100;
  const total = model.total > 0 ? model.total : model.bytes;
  if (total <= 0) return 0;
  return Math.min(99, Math.max(0, Math.round((model.received / total) * 100)));
}

export function totalBytes(models: ModelStatus[]): number {
  return models.reduce((sum, model) => sum + (model.bytes > 0 ? model.bytes : model.total), 0);
}

export function missingBytes(models: ModelStatus[]): number {
  return models
    .filter(model => !model.present && model.state !== 'ready')
    .reduce((sum, model) => sum + (model.bytes > 0 ? model.bytes : model.total), 0);
}

export function formatBytes(bytes: number): string {
  if (bytes >= 1024 * 1024 * 1024) return `${(bytes / (1024 * 1024 * 1024)).toFixed(1)} GB`;
  if (bytes >= 1024 * 1024) return `${Math.round(bytes / (1024 * 1024))} MB`;
  return `${bytes} B`;
}

export function label(name: string): string {
  return MODEL_LABELS[name] ?? name;
}
