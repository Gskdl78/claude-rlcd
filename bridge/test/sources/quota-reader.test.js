import { describe, it, expect } from 'vitest';
import path from 'node:path';
import fs from 'node:fs/promises';
import { readQuota } from '../../src/sources/quota-reader.js';

const FIX = path.resolve(import.meta.dirname, '../fixtures/cache/sample-cache.json');

describe('readQuota', () => {
  it('returns Quota shape from cache', async () => {
    const q = await readQuota(FIX);
    expect(q.fiveHour.utilization).toBe(0.62);
    expect(q.opus.utilization).toBe(0.48);
    expect(q.lastUpdate).toBe(1714291200);
  });

  it('returns null when file missing', async () => {
    expect(await readQuota('/nope/no-cache.json')).toBeNull();
  });

  it('returns null when JSON invalid', async () => {
    const bad = path.resolve(import.meta.dirname, '../fixtures/cache/bad.json');
    await fs.writeFile(bad, '{not json');
    try { expect(await readQuota(bad)).toBeNull(); }
    finally { await fs.unlink(bad).catch(() => {}); }
  });
});
