import {
  allReady,
  anyDownloading,
  formatBytes,
  label,
  missingBytes,
  parseModels,
  percent,
  totalBytes,
} from '../src/models/manifest';

const json = JSON.stringify([
  {
    name: 'nllb_bpe.bin',
    present: false,
    bytes: 12587091,
    state: 'downloading',
    received: 6293545,
    total: 12587091,
    error: null,
  },
  {
    name: 'ggml-base-q5_1.bin',
    present: true,
    bytes: 59707625,
    state: 'ready',
    received: 59707625,
    total: 59707625,
    error: null,
  },
  {
    name: 'encoder_model_int8.onnx',
    present: false,
    bytes: 415346576,
    state: 'failed',
    received: 0,
    total: 415346576,
    error: 'checksum mismatch',
  },
]);

describe('model status', () => {
  it('parses the native payload and keeps the states', () => {
    const models = parseModels(json);
    expect(models).toHaveLength(3);
    expect(models[0].state).toBe('downloading');
    expect(models[1].present).toBe(true);
    expect(models[2].error).toBe('checksum mismatch');
    expect(anyDownloading(models)).toBe(true);
    expect(allReady(models)).toBe(false);
  });

  it('survives garbage from the bridge', () => {
    expect(parseModels('not json')).toEqual([]);
    expect(parseModels('{"a":1}')).toEqual([]);
    expect(parseModels('[{"name":"x","state":"nonsense"}]')[0].state).toBe('missing');
  });

  it('computes progress, totals and formatting', () => {
    const models = parseModels(json);
    expect(percent(models[0])).toBe(50);
    expect(percent(models[1])).toBe(100);
    expect(percent(models[2])).toBe(0);
    expect(totalBytes(models)).toBe(12587091 + 59707625 + 415346576);
    expect(missingBytes(models)).toBe(12587091 + 415346576);
    expect(formatBytes(415346576)).toBe('396 MB');
    expect(formatBytes(1400000000)).toBe('1.3 GB');
  });

  it('labels the known files and falls back to the file name', () => {
    expect(label('nllb_bpe.bin')).toBe('NLLB tokenizer');
    expect(label('something.bin')).toBe('something.bin');
  });

  it('treats an empty list as not ready', () => {
    expect(allReady([])).toBe(false);
  });
});
