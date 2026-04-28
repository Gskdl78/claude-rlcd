import { describe, it, expect } from 'vitest';
import fs from 'node:fs/promises';
import os from 'node:os';
import path from 'node:path';
import { tailEvents } from '../../src/sources/events-tail.js';

async function withTempFile(fn) {
  const dir = await fs.mkdtemp(path.join(os.tmpdir(), 'events-'));
  const file = path.join(dir, 'events.jsonl');
  try { return await fn(file, dir); }
  finally { await fs.rm(dir, { recursive: true, force: true }); }
}

describe('tailEvents', () => {
  it('emits each line as an object after creation', async () => {
    await withTempFile(async (file) => {
      const collected = [];
      const watcher = tailEvents(file, (e) => collected.push(e));
      await new Promise(r => setTimeout(r, 100));
      await fs.writeFile(file, JSON.stringify({ event: 'A', ts: 1 }) + '\n');
      await new Promise(r => setTimeout(r, 200));
      await fs.appendFile(file, JSON.stringify({ event: 'B', ts: 2 }) + '\n');
      await new Promise(r => setTimeout(r, 300));
      await watcher.close();
      expect(collected.map(e => e.event).sort()).toEqual(['A', 'B']);
    });
  });

  it('skips malformed lines but continues', async () => {
    await withTempFile(async (file) => {
      const collected = [];
      const watcher = tailEvents(file, (e) => collected.push(e));
      await new Promise(r => setTimeout(r, 100));
      await fs.writeFile(file, '{"event":"A"}\nNOT_JSON\n{"event":"B"}\n');
      await new Promise(r => setTimeout(r, 300));
      await watcher.close();
      expect(collected.map(e => e.event).sort()).toEqual(['A', 'B']);
    });
  });
});
