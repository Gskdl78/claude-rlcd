import chokidar from 'chokidar';
import fs from 'node:fs';

export function tailEvents(filePath, onEvent) {
  let position = 0;
  let buffer = '';

  async function readNew() {
    if (!fs.existsSync(filePath)) return;
    const stat = fs.statSync(filePath);
    if (stat.size < position) position = 0;       // file rotated/truncated
    if (stat.size === position) return;
    const stream = fs.createReadStream(filePath, { start: position, end: stat.size - 1 });
    position = stat.size;
    for await (const chunk of stream) buffer += chunk.toString('utf8');
    const lines = buffer.split('\n');
    buffer = lines.pop() ?? '';
    for (const line of lines) {
      const trimmed = line.trim();
      if (!trimmed) continue;
      try { onEvent(JSON.parse(trimmed)); } catch { /* skip malformed */ }
    }
  }

  const watcher = chokidar.watch(filePath, {
    persistent: true,
    awaitWriteFinish: false,
    usePolling: false
  });
  watcher.on('add', readNew).on('change', readNew).on('unlink', () => {
    position = 0;
    buffer = '';
  });

  return { close: () => watcher.close() };
}
