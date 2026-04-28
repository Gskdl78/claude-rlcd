import fs from 'node:fs/promises';
import os from 'node:os';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

export const RLCD_TAG = 'claude-rlcd-display';

const HOOK_EVENTS = ['Notification', 'SessionStart', 'Stop'];

export function mergeHooks(settings, emitterCmd) {
  const next = JSON.parse(JSON.stringify(settings ?? {}));
  next.hooks ??= {};
  for (const evt of HOOK_EVENTS) {
    next.hooks[evt] ??= [];
    next.hooks[evt].push({
      matcher: '',
      hooks: [{
        type: 'command',
        command: `${emitterCmd} ${evt}`,
        tag: RLCD_TAG
      }]
    });
  }
  return next;
}

export function removeHooks(settings) {
  const next = JSON.parse(JSON.stringify(settings ?? {}));
  if (!next.hooks) return next;
  for (const evt of HOOK_EVENTS) {
    if (!next.hooks[evt]) continue;
    next.hooks[evt] = next.hooks[evt].filter((entry) =>
      !entry.hooks?.some((h) => h.tag === RLCD_TAG));
    if (next.hooks[evt].length === 0) delete next.hooks[evt];
  }
  if (Object.keys(next.hooks).length === 0) delete next.hooks;
  return next;
}

export async function readSettings(file) {
  try {
    const buf = await fs.readFile(file);
    const hasBom = buf[0] === 0xEF && buf[1] === 0xBB && buf[2] === 0xBF;
    const text = (hasBom ? buf.slice(3) : buf).toString('utf8');
    return JSON.parse(text || '{}');
  } catch {
    return {};
  }
}

export async function writeSettings(file, obj) {
  await fs.mkdir(path.dirname(file), { recursive: true });
  await fs.writeFile(file, JSON.stringify(obj, null, 2), { encoding: 'utf8' });
}

const isMain = (() => {
  if (!process.argv[1]) return false;
  try {
    return path.resolve(fileURLToPath(import.meta.url)).toLowerCase() ===
           path.resolve(process.argv[1]).toLowerCase();
  } catch { return false; }
})();

if (isMain) {
  const file = process.env.CLAUDE_SETTINGS ?? path.join(os.homedir(), '.claude', 'settings.json');
  const here = path.dirname(fileURLToPath(import.meta.url));
  const emitterCmd = process.argv[2] ?? path.join(here, 'emit.cmd');
  const before = await readSettings(file);
  const after = mergeHooks(before, emitterCmd);
  await writeSettings(file, after);
  console.log(`hooks installed in ${file}`);
}
