import os from 'node:os';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { readSettings, writeSettings, removeHooks } from './install.mjs';

const isMain = (() => {
  if (!process.argv[1]) return false;
  try {
    return path.resolve(fileURLToPath(import.meta.url)).toLowerCase() ===
           path.resolve(process.argv[1]).toLowerCase();
  } catch { return false; }
})();

if (isMain) {
  const file = process.env.CLAUDE_SETTINGS ?? path.join(os.homedir(), '.claude', 'settings.json');
  const before = await readSettings(file);
  const after = removeHooks(before);
  await writeSettings(file, after);
  console.log(`hooks removed from ${file}`);
}
