import { parseOutbound } from './schema.js';

const v = 1;

export const mkHello = (bridge, host) =>
  parseOutbound({ v, type: 'hello', bridge, host });

export const mkState = (sessions, quota, alert = null) =>
  parseOutbound({ v, type: 'state', sessions, quota, alert });

export const mkSessionUpdate = (id, patch) =>
  parseOutbound({ v, type: 'session-update', id, patch });

export const mkSessionEnd = (id) =>
  parseOutbound({ v, type: 'session-end', id });

export const mkQuota = (fiveHour, sevenDay, opus = null) =>
  parseOutbound({ v, type: 'quota', fiveHour, sevenDay, opus });

export const mkAlert = ({ id, sessionId, sessionName, text }) =>
  parseOutbound({
    v, type: 'alert', id, kind: 'approval',
    sessionId, sessionName,
    text: String(text).slice(0, 80),
    ts: Math.floor(Date.now() / 1000)
  });

export const mkAlertClear = (id) =>
  parseOutbound({ v, type: 'alert-clear', id });

export const mkTimeSync = (utc, tz) =>
  parseOutbound({ v, type: 'time-sync', utc, tz });

export const mkPing = (ts = Math.floor(Date.now() / 1000)) =>
  parseOutbound({ v, type: 'ping', ts });
