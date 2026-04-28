import { describe, it, expect } from 'vitest';
import { WebSocketServer } from 'ws';
import { connectWithRetry } from '../../src/transport/ws-client.js';
import { runOrchestrator } from '../../src/orchestrator.js';

describe('e2e bridge ↔ mock ESP32', () => {
  it('full hello → state → time-sync → alert → clear lifecycle', async () => {
    const port = 30000 + Math.floor(Math.random() * 1000);
    const wss = new WebSocketServer({ port });
    const inbound = [];
    wss.on('connection', (sock) => {
      sock.on('message', (data) => {
        for (const line of data.toString().split('\n')) {
          if (!line.trim()) continue;
          try { inbound.push(JSON.parse(line)); } catch {}
        }
      });
    });
    await new Promise((r) => wss.once('listening', r));

    const client = await connectWithRetry(`ws://127.0.0.1:${port}/`, {});

    const sources = {
      async snapshot() { return { sessions: [], quota: null }; },
      onEvent(cb) { this._cb = cb; },
      _emit(e) { this._cb?.(e); }
    };

    const orch = await runOrchestrator(client, sources, {
      hostName: 'PC',
      pollIntervalMs: 5000,
      pingIntervalMs: 60_000
    });
    sources._emit({ event: 'Notification', sessionId: 's1', text: 'rm -rf' });
    sources._emit({ event: 'Stop', sessionId: 's1' });

    await new Promise((r) => setTimeout(r, 200));

    const types = inbound.map((m) => m.type);
    expect(types[0]).toBe('hello');
    expect(types[1]).toBe('state');
    expect(types[2]).toBe('time-sync');
    expect(types).toContain('alert');
    expect(types).toContain('alert-clear');

    orch.stop();
    client.close();
    wss.close();
  });
});
