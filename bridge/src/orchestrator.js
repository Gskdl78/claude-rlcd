import os from 'node:os';
import {
  mkHello, mkState, mkSessionUpdate, mkSessionEnd, mkQuota,
  mkAlert, mkAlertClear, mkPing, mkTimeSync
} from './protocol/encode.js';

const PATCHABLE_FIELDS = ['name', 'path', 'status', 'lastTool', 'lastTarget', 'lastTime', 'toolCount'];

function diffSession(prev, curr) {
  const patch = {};
  for (const k of PATCHABLE_FIELDS) {
    if (prev[k] !== curr[k]) patch[k] = curr[k];
  }
  return Object.keys(patch).length === 0 ? null : patch;
}

function quotaEqual(a, b) {
  if (a === null && b === null) return true;
  if (a === null || b === null) return false;
  return a.fiveHour.utilization === b.fiveHour.utilization &&
         a.sevenDay.utilization === b.sevenDay.utilization &&
         (a.opus?.utilization ?? null) === (b.opus?.utilization ?? null) &&
         a.fiveHour.reset === b.fiveHour.reset &&
         a.sevenDay.reset === b.sevenDay.reset;
}

export async function runOrchestrator(client, sources, opts = {}) {
  const hostName = opts.hostName ?? os.hostname();
  const bridgeVersion = opts.bridgeVersion ?? '1.0.0';
  const POLL_MS = opts.pollIntervalMs ?? 5000;
  const PING_MS = opts.pingIntervalMs ?? 30_000;
  const tz = opts.timezone ?? Intl.DateTimeFormat().resolvedOptions().timeZone;

  client.send(mkHello(bridgeVersion, hostName));

  let lastSnapshot = await sources.snapshot();
  client.send(mkState(
    { active: lastSnapshot.sessions.length, items: lastSnapshot.sessions },
    lastSnapshot.quota,
    null
  ));

  client.send(mkTimeSync(Math.floor(Date.now() / 1000), tz));

  const activeAlerts = new Map(); // sessionId → alertId

  async function pushDeltas() {
    const curr = await sources.snapshot();
    const prevById = new Map(lastSnapshot.sessions.map((s) => [s.id, s]));
    const currById = new Map(curr.sessions.map((s) => [s.id, s]));
    for (const [id, s] of currById) {
      const prev = prevById.get(id);
      if (!prev) {
        client.send(mkSessionUpdate(id, {
          name: s.name, path: s.path, status: s.status,
          lastTool: s.lastTool, lastTarget: s.lastTarget,
          lastTime: s.lastTime, toolCount: s.toolCount
        }));
      } else {
        const patch = diffSession(prev, s);
        if (patch) client.send(mkSessionUpdate(id, patch));
      }
    }
    for (const [id] of prevById) {
      if (!currById.has(id)) client.send(mkSessionEnd(id));
    }
    if (!quotaEqual(lastSnapshot.quota, curr.quota) && curr.quota) {
      client.send(mkQuota(curr.quota.fiveHour, curr.quota.sevenDay, curr.quota.opus));
    }
    lastSnapshot = curr;
  }

  sources.onEvent(async (evt) => {
    if (evt.event === 'Notification') {
      const id = `alert-${evt.sessionId}-${evt.ts ?? Math.floor(Date.now() / 1000)}`;
      activeAlerts.set(evt.sessionId, id);
      client.send(mkAlert({
        id,
        sessionId: evt.sessionId,
        sessionName: evt.sessionName ?? evt.sessionId,
        text: evt.text ?? 'approval needed'
      }));
      return;
    }
    if (evt.event === 'NotificationCleared' || evt.event === 'Stop') {
      const id = activeAlerts.get(evt.sessionId);
      if (id) {
        activeAlerts.delete(evt.sessionId);
        client.send(mkAlertClear(id));
      }
      if (evt.event === 'Stop') await pushDeltas();
      return;
    }
    if (evt.event === 'SessionStart') {
      await pushDeltas();
      return;
    }
  });

  const pollTimer = setInterval(() => pushDeltas().catch(() => {}), POLL_MS);
  const pingTimer = setInterval(() => client.send(mkPing()), PING_MS);

  return {
    stop() {
      clearInterval(pollTimer);
      clearInterval(pingTimer);
    }
  };
}
