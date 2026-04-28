import fs from 'node:fs/promises';
import os from 'node:os';
import path from 'node:path';

const DEFAULT = path.join(os.homedir(), '.claude', 'claude-rlcd', 'events.jsonl');

export async function emit(record, file = DEFAULT) {
  await fs.mkdir(path.dirname(file), { recursive: true });
  const line = JSON.stringify({
    ...record,
    ts: record.ts ?? Math.floor(Date.now() / 1000)
  }) + '\n';
  await fs.appendFile(file, line, { encoding: 'utf8' });
}

import { fileURLToPath } from 'node:url';
const isMainModule = (() => {
  if (!process.argv[1]) return false;
  try {
    return path.resolve(fileURLToPath(import.meta.url)).toLowerCase() ===
           path.resolve(process.argv[1]).toLowerCase();
  } catch { return false; }
})();

if (isMainModule) {
  const evt = process.argv[2] ?? 'Unknown';
  let stdin = '';
  process.stdin.setEncoding('utf8');
  for await (const chunk of process.stdin) stdin += chunk;
  let payload = {};
  try { payload = stdin ? JSON.parse(stdin) : {}; } catch {}
  await emit({ event: evt, ...payload });
}
