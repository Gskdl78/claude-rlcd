import { describe, it, expect } from 'vitest';
import { WebSocketServer } from 'ws';
import { connectWithRetry } from '../../src/transport/ws-client.js';

function startServer(port, onConnection) {
  return new Promise((resolve) => {
    const wss = new WebSocketServer({ port });
    wss.on('listening', () => resolve(wss));
    wss.on('connection', onConnection);
  });
}

describe('connectWithRetry', () => {
  it('connects to a live server and exchanges a line', async () => {
    const port = 30000 + Math.floor(Math.random() * 1000);
    const received = [];
    const wss = await startServer(port, (sock) => {
      sock.on('message', (data) => received.push(data.toString()));
      sock.send('{"v":1,"type":"hello-ack","fw":"x","lvgl":"x","ip":"x","mac":"x"}\n');
    });

    const client = await connectWithRetry(`ws://127.0.0.1:${port}/`, {
      backoffMs: [10, 20], maxAttempts: 3
    });

    const inbound = [];
    client.on('message', (m) => inbound.push(m));
    client.send({ v: 1, type: 'ping', ts: 1 });
    await new Promise((r) => setTimeout(r, 150));

    expect(received.some((l) => l.includes('"type":"ping"'))).toBe(true);
    expect(inbound[0]?.type).toBe('hello-ack');

    client.close();
    wss.close();
  });

  it('throws after maxAttempts on unreachable server', async () => {
    await expect(connectWithRetry('ws://127.0.0.1:1/', { backoffMs: [5, 5], maxAttempts: 2 }))
      .rejects.toThrow();
  });
});
