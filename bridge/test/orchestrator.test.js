import { describe, it, expect } from 'vitest';
import { runOrchestrator } from '../src/orchestrator.js';

function makeStub(initial) {
  const state = { ...initial };
  return {
    state,
    async snapshot() {
      return {
        sessions: [...state.sessions],
        quota: state.quota
      };
    },
    onEvent(cb) { this._cb = cb; },
    _emit(e) { this._cb?.(e); },
    setSessions(arr) { state.sessions = arr; },
    setQuota(q) { state.quota = q; }
  };
}

function fakeClient() {
  const sent = [];
  return {
    sent,
    send: (m) => sent.push(m),
    on: () => {},
    close: () => {}
  };
}

describe('orchestrator', () => {
  it('emits hello → state → time-sync on start', async () => {
    const c = fakeClient();
    const sources = makeStub({ sessions: [], quota: null });
    const orch = await runOrchestrator(c, sources, { hostName: 'PC', pollIntervalMs: 5000 });
    expect(c.sent[0].type).toBe('hello');
    expect(c.sent[1].type).toBe('state');
    expect(c.sent[2].type).toBe('time-sync');
    orch.stop();
  });

  it('emits Notification as alert and Stop as alert-clear', async () => {
    const c = fakeClient();
    const sources = makeStub({ sessions: [], quota: null });
    const orch = await runOrchestrator(c, sources, { pollIntervalMs: 1000 });
    sources._emit({ event: 'Notification', sessionId: 's1', text: 'rm -rf' });
    sources._emit({ event: 'Stop', sessionId: 's1' });
    await new Promise((r) => setTimeout(r, 30));
    expect(c.sent.find((m) => m.type === 'alert')?.sessionId).toBe('s1');
    expect(c.sent.find((m) => m.type === 'alert-clear')).toBeDefined();
    orch.stop();
  });

  it('emits session-update on diff after poll', async () => {
    const c = fakeClient();
    const initial = [{
      id: 's1', name: 'p', path: '/p', status: 'active',
      lastTool: 'Edit', lastTarget: 'a.ts',
      lastTime: 100, toolCount: 1, startTime: 50
    }];
    const sources = makeStub({ sessions: initial, quota: null });
    const orch = await runOrchestrator(c, sources, { pollIntervalMs: 30 });
    sources.setSessions([{ ...initial[0], lastTool: 'Bash', lastTarget: 'git', lastTime: 110, toolCount: 2 }]);
    await new Promise((r) => setTimeout(r, 100));
    const upd = c.sent.find((m) => m.type === 'session-update');
    expect(upd).toBeDefined();
    expect(upd.id).toBe('s1');
    expect(upd.patch.lastTool).toBe('Bash');
    expect(upd.patch.toolCount).toBe(2);
    orch.stop();
  });

  it('emits session-end when a session disappears', async () => {
    const c = fakeClient();
    const sources = makeStub({
      sessions: [{
        id: 's1', name: 'p', path: '/p', status: 'active',
        lastTool: 'Edit', lastTarget: '', lastTime: 100, toolCount: 1, startTime: 50
      }],
      quota: null
    });
    const orch = await runOrchestrator(c, sources, { pollIntervalMs: 30 });
    sources.setSessions([]);
    await new Promise((r) => setTimeout(r, 100));
    expect(c.sent.find((m) => m.type === 'session-end' && m.id === 's1')).toBeDefined();
    orch.stop();
  });

  it('emits quota when quota changes', async () => {
    const c = fakeClient();
    const sources = makeStub({ sessions: [], quota: null });
    const orch = await runOrchestrator(c, sources, { pollIntervalMs: 30 });
    sources.setQuota({
      fiveHour: { utilization: 0.4, reset: 1 },
      sevenDay: { utilization: 0.2, reset: 1 },
      opus:     { utilization: 0.1 },
      lastUpdate: 1
    });
    await new Promise((r) => setTimeout(r, 100));
    expect(c.sent.find((m) => m.type === 'quota')).toBeDefined();
    orch.stop();
  });

  it('SessionStart event triggers a snapshot push so a new session appears immediately', async () => {
    const c = fakeClient();
    const sources = makeStub({ sessions: [], quota: null });
    const orch = await runOrchestrator(c, sources, { pollIntervalMs: 5000 });
    c.sent.length = 0;
    sources.setSessions([{
      id: 'newId', name: 'np', path: '/np', status: 'active',
      lastTool: 'Read', lastTarget: '', lastTime: 200, toolCount: 0, startTime: 200
    }]);
    sources._emit({ event: 'SessionStart', sessionId: 'newId' });
    await new Promise((r) => setTimeout(r, 50));
    expect(c.sent.find((m) => m.type === 'session-update' && m.id === 'newId')).toBeDefined();
    orch.stop();
  });
});
