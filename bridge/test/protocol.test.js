import { describe, it, expect } from 'vitest';
import {
  HelloMessage, StateMessage, parseOutbound
} from '../src/protocol/schema.js';
import {
  mkHello, mkSessionUpdate, mkAlert
} from '../src/protocol/encode.js';

describe('protocol schemas', () => {
  it('accepts a valid hello', () => {
    const msg = { v: 1, type: 'hello', bridge: '1.0.0', host: 'PC' };
    expect(HelloMessage.parse(msg)).toEqual(msg);
  });

  it('rejects hello without v', () => {
    expect(() => HelloMessage.parse({ type: 'hello', bridge: '1.0.0', host: 'PC' })).toThrow();
  });

  it('parses state with sessions and quota', () => {
    const msg = {
      v: 1, type: 'state',
      sessions: { active: 1, items: [{
        id: 'abc', name: 'proj', path: '/p', status: 'active',
        lastTool: 'Edit', lastTarget: 'foo.ts',
        lastTime: 1714291200, toolCount: 3, startTime: 1714290000
      }]},
      quota: {
        fiveHour: { utilization: 0.62, reset: 1714305600 },
        sevenDay: { utilization: 0.31, reset: 1714896000 },
        opus:     { utilization: 0.48 },
        lastUpdate: 1714291200
      },
      alert: null
    };
    expect(StateMessage.parse(msg).type).toBe('state');
  });

  it('parseOutbound dispatches by type', () => {
    expect(parseOutbound({ v:1, type:'ping', ts:1 }).type).toBe('ping');
    expect(parseOutbound({ v:1, type:'session-end', id:'a' }).type).toBe('session-end');
  });

  it('parseOutbound throws on unknown type', () => {
    expect(() => parseOutbound({ v:1, type:'nonsense' })).toThrow();
  });
});

describe('protocol encode helpers', () => {
  it('mkHello includes v=1 and host', () => {
    expect(mkHello('1.0.0', 'PC')).toEqual({ v: 1, type: 'hello', bridge: '1.0.0', host: 'PC' });
  });

  it('mkSessionUpdate includes patch only', () => {
    const m = mkSessionUpdate('id1', { lastTool: 'Bash', lastTime: 100 });
    expect(m.patch).toEqual({ lastTool: 'Bash', lastTime: 100 });
  });

  it('mkAlert truncates text to 80 chars', () => {
    const long = 'x'.repeat(200);
    expect(mkAlert({ id: 'a', sessionId: 's', sessionName: 'n', text: long }).text.length).toBe(80);
  });
});
