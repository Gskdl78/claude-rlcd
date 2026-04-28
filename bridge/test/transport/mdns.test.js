import { describe, it, expect } from 'vitest';
import { resolveDevice } from '../../src/transport/mdns.js';

describe('resolveDevice', () => {
  it('returns address from a fake browser', async () => {
    const fakeBonjour = {
      find: () => ({
        on(event, cb) {
          if (event === 'up') {
            setImmediate(() => cb({ name: 'claude-rlcd', addresses: ['192.168.1.42'], port: 80 }));
          }
        },
        stop() {}
      })
    };
    const res = await resolveDevice('claude-rlcd', { bonjour: fakeBonjour, timeoutMs: 500 });
    expect(res.host).toBe('192.168.1.42');
    expect(res.port).toBe(80);
  });

  it('rejects after timeout', async () => {
    const fakeBonjour = { find: () => ({ on() {}, stop() {} }) };
    await expect(resolveDevice('nope', { bonjour: fakeBonjour, timeoutMs: 100 })).rejects.toThrow(/timeout/i);
  });
});
