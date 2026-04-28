import { describe, it, expect } from 'vitest';
import fs from 'node:fs/promises';
import os from 'node:os';
import path from 'node:path';
import { spawnSync } from 'node:child_process';
import { mergeHooks, removeHooks, RLCD_TAG } from '../installer/install.mjs';

const REPO_ROOT = path.resolve(import.meta.dirname, '..');
const INSTALL_MJS   = path.join(REPO_ROOT, 'installer/install.mjs');
const UNINSTALL_MJS = path.join(REPO_ROOT, 'installer/uninstall.mjs');

describe('installer mergeHooks', () => {
  it('adds hooks to empty settings', () => {
    const merged = mergeHooks({}, '/path/to/emit.cmd');
    expect(merged.hooks.Notification[0].hooks[0].command).toContain('emit.cmd Notification');
    expect(merged.hooks.SessionStart).toBeDefined();
    expect(merged.hooks.Stop).toBeDefined();
  });
  it('appends to existing hooks without overwriting', () => {
    const existing = {
      hooks: { Notification: [{ matcher: '', hooks: [{ type: 'command', command: 'OTHER', tag: 'user' }] }] }
    };
    const merged = mergeHooks(existing, '/p/emit.cmd');
    expect(merged.hooks.Notification).toHaveLength(2);
    expect(merged.hooks.Notification[0].hooks[0].command).toBe('OTHER');
    expect(merged.hooks.Notification[1].hooks[0].tag).toBe(RLCD_TAG);
  });
  it('removeHooks strips only our entries', () => {
    const installed = mergeHooks({
      hooks: { Notification: [{ matcher: '', hooks: [{ type: 'command', command: 'OTHER' }] }] }
    }, '/p/emit.cmd');
    const cleaned = removeHooks(installed);
    expect(cleaned.hooks.Notification).toHaveLength(1);
    expect(cleaned.hooks.Notification[0].hooks[0].command).toBe('OTHER');
    expect(cleaned.hooks.SessionStart).toBeUndefined();
  });
});

async function withTempSettings(initialBytes, fn) {
  const dir = await fs.mkdtemp(path.join(os.tmpdir(), 'inst-'));
  const file = path.join(dir, 'settings.json');
  await fs.writeFile(file, initialBytes);
  try { return await fn(file, dir); }
  finally { await fs.rm(dir, { recursive: true, force: true }); }
}

describe('installer BOM handling and round-trip', () => {
  it('reads BOM-prefixed settings and writes back without BOM', async () => {
    const original = '{ "theme": "dark" }';
    const bomBuf = Buffer.concat([Buffer.from([0xEF, 0xBB, 0xBF]), Buffer.from(original, 'utf8')]);
    await withTempSettings(bomBuf, async (file) => {
      const env = { ...process.env, CLAUDE_SETTINGS: file };
      const r = spawnSync(process.execPath, [INSTALL_MJS, '/p/emit.cmd'], { env, encoding: 'utf8' });
      expect(r.status).toBe(0);
      const out = await fs.readFile(file);
      expect(out[0]).not.toBe(0xEF);
      const parsed = JSON.parse(out.toString('utf8'));
      expect(parsed.theme).toBe('dark');
      expect(parsed.hooks.Notification).toBeDefined();
    });
  });

  it('install + uninstall is a round-trip', async () => {
    const original = JSON.stringify({
      theme: 'dark',
      hooks: { Notification: [{ matcher: '', hooks: [{ type: 'command', command: 'OTHER' }] }] }
    }, null, 2);
    await withTempSettings(Buffer.from(original, 'utf8'), async (file) => {
      const env = { ...process.env, CLAUDE_SETTINGS: file };
      const r1 = spawnSync(process.execPath, [INSTALL_MJS,   '/p/emit.cmd'], { env, encoding: 'utf8' });
      const r2 = spawnSync(process.execPath, [UNINSTALL_MJS],                { env, encoding: 'utf8' });
      expect(r1.status).toBe(0);
      expect(r2.status).toBe(0);
      const after  = JSON.parse(await fs.readFile(file, 'utf8'));
      const before = JSON.parse(original);
      expect(after).toEqual(before);
    });
  });
});
