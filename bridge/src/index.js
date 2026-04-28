import os from 'node:os';
import path from 'node:path';
import { resolveDevice } from './transport/mdns.js';
import { connectWithRetry } from './transport/ws-client.js';
import { tailEvents } from './sources/events-tail.js';
import { listSessions } from './sources/projects-reader.js';
import { readQuota } from './sources/quota-reader.js';
import { runOrchestrator } from './orchestrator.js';

const HOME = os.homedir();
const EVENTS_FILE  = path.join(HOME, '.claude', 'claude-rlcd', 'events.jsonl');
const PROJECTS_DIR = path.join(HOME, '.claude', 'projects');
const CACHE_FILE   = path.join(HOME, '.claude', 'status-tracker-cache.json');

async function main() {
  console.log('claude-rlcd-bridge starting');
  let device;
  try {
    device = await resolveDevice('claude-rlcd', { timeoutMs: 5000 });
  } catch {
    device = { host: process.env.CLAUDE_RLCD_HOST ?? 'claude-rlcd.local', port: 80 };
    console.log(`mDNS resolution failed, falling back to ${device.host}:${device.port}`);
  }
  console.log(`device: ${device.host}:${device.port}`);

  let listener = null;
  const sources = {
    async snapshot() {
      const sessions = await listSessions(PROJECTS_DIR);
      const quota = await readQuota(CACHE_FILE);
      return { sessions, quota };
    },
    onEvent(cb) { listener = cb; }
  };

  const tail = tailEvents(EVENTS_FILE, (e) => { if (listener) listener(e); });

  /* Reconnect loop. The device reboots after every flash and on Wi-Fi
   * blips, so we re-establish the WS connection AND re-run the
   * orchestrator (which re-sends hello + initial state + time-sync). */
  let stop = false;
  process.on('SIGINT', () => { stop = true; });
  process.on('SIGTERM', () => { stop = true; });

  while (!stop) {
    let client;
    try {
      client = await connectWithRetry(`ws://${device.host}:${device.port}/`, {});
      console.log('WebSocket connected');
    } catch (e) {
      console.error('connect failed:', e?.message ?? e);
      await new Promise((r) => setTimeout(r, 5000));
      continue;
    }

    const orch = await runOrchestrator(client, sources, {});

    client.on('message', (msg) => {
      if (msg.type === 'pong') return;
      if (msg.type === 'env')  console.log(`env: ${msg.tempC}°C  ${msg.humidity}%`);
      if (msg.type === 'diag') console.log(`diag: heap=${msg.free_heap} rssi=${msg.rssi}`);
      if (msg.type === 'error') console.error(`device error: ${msg.reason}`);
    });

    await new Promise((resolve) => {
      let done = false;
      const finish = () => { if (!done) { done = true; resolve(); } };
      client.on('close', finish);
      client.on('error', (e) => { console.error('ws error:', e?.code ?? e?.message ?? e); finish(); });
    });

    orch.stop();
    try { client.close(); } catch {}
    if (!stop) {
      console.log('WebSocket disconnected; reconnecting in 2s...');
      await new Promise((r) => setTimeout(r, 2000));
    }
  }

  await tail.close();
  console.log('shutdown complete');
  process.exit(0);
}

main().catch((e) => { console.error(e); process.exit(1); });
