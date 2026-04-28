import { WebSocket } from 'ws';
import { EventEmitter } from 'node:events';

export async function connectWithRetry(url, opts = {}) {
  const backoffMs = opts.backoffMs ?? [1000, 2000, 5000, 10000, 30000];
  const maxAttempts = opts.maxAttempts ?? Infinity;
  let attempt = 0;

  while (attempt < maxAttempts) {
    try {
      return await tryConnect(url);
    } catch (err) {
      attempt++;
      if (attempt >= maxAttempts) throw err;
      const wait = backoffMs[Math.min(attempt - 1, backoffMs.length - 1)];
      await new Promise((r) => setTimeout(r, wait));
    }
  }
  throw new Error('exhausted attempts');
}

function tryConnect(url) {
  return new Promise((resolve, reject) => {
    const ws = new WebSocket(url);
    const emitter = new EventEmitter();
    const earlyQueue = { message: [], close: [], error: [] };
    let buffer = '';

    function emit(ev, payload) {
      if (emitter.listenerCount(ev) > 0) emitter.emit(ev, payload);
      else earlyQueue[ev]?.push(payload);
    }

    ws.once('open', () => {
      const client = {
        send(obj) { ws.send(JSON.stringify(obj) + '\n'); },
        close()    { ws.close(); },
        on(ev, cb) {
          emitter.on(ev, cb);
          // Replay any buffered events of this kind
          if (earlyQueue[ev]?.length) {
            const pending = earlyQueue[ev].splice(0);
            for (const payload of pending) emitter.emit(ev, payload);
          }
        }
      };
      ws.on('message', (data) => {
        buffer += data.toString('utf8');
        const lines = buffer.split('\n');
        buffer = lines.pop() ?? '';
        for (const line of lines) {
          if (!line.trim()) continue;
          try { emit('message', JSON.parse(line)); }
          catch { /* skip */ }
        }
      });
      ws.on('close', () => emit('close', undefined));
      ws.on('error', (e) => emit('error', e));
      resolve(client);
    });

    ws.once('error', (e) => reject(e));
  });
}
