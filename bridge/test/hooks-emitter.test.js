import { describe, it, expect } from 'vitest';
import fs from 'node:fs/promises';
import os from 'node:os';
import path from 'node:path';
import { emit } from '../src/hooks-emitter.js';

describe('hooks-emitter.emit', () => {
  it('appends one JSONL line per call', async () => {
    const dir = await fs.mkdtemp(path.join(os.tmpdir(), 'emit-'));
    const file = path.join(dir, 'events.jsonl');
    try {
      await emit({ event: 'Notification', sessionId: 's1', text: 'x' }, file);
      await emit({ event: 'Stop', sessionId: 's1' }, file);
      const text = await fs.readFile(file, 'utf8');
      const lines = text.trim().split('\n');
      expect(lines).toHaveLength(2);
      expect(JSON.parse(lines[0]).event).toBe('Notification');
      expect(JSON.parse(lines[1]).event).toBe('Stop');
    } finally {
      await fs.rm(dir, { recursive: true, force: true });
    }
  });

  it('completes within 100ms', async () => {
    const dir = await fs.mkdtemp(path.join(os.tmpdir(), 'emit-'));
    const file = path.join(dir, 'events.jsonl');
    try {
      const start = Date.now();
      await emit({ event: 'Notification', sessionId: 's1' }, file);
      const elapsed = Date.now() - start;
      expect(elapsed).toBeLessThan(100);
    } finally {
      await fs.rm(dir, { recursive: true, force: true });
    }
  });
});
