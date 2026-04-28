import fs from 'node:fs/promises';
import path from 'node:path';

/* Sessions whose last JSONL row is older than this drop off the dashboard.
 * 10 min was too tight in practice — sessions where the user is reading or
 * the model is thinking would prune before the user expected. 30 min keeps
 * recently-used sessions visible without cluttering with day-old ones. */
const FRESHNESS_S = Number(process.env.CLAUDE_RLCD_FRESHNESS_S) || 1800;

function lastSegment(p) {
  return p.split(/[\\\/]/).filter(Boolean).pop() ?? p;
}

function tsSec(iso) { return Math.floor(new Date(iso).getTime() / 1000); }

function extractTarget(_tool, input) {
  if (!input) return '';
  const cand = input.file_path ?? input.command ?? input.path ?? input.url ?? '';
  return String(cand).slice(0, 60);
}

async function parseSession(filePath) {
  const text = await fs.readFile(filePath, 'utf8');
  let sessionId = null, cwd = null, startTime = null, lastRowTime = null;
  let lastTool = null, lastTarget = null, toolUseTime = null, toolCount = 0;

  for (const line of text.split('\n')) {
    if (!line.trim()) continue;
    let row;
    try { row = JSON.parse(line); } catch { continue; }

    // Real Claude Code JSONL has no "system/init" marker — each row carries
    // sessionId / cwd / timestamp directly. Take the earliest occurrence.
    if (!sessionId && row.sessionId) sessionId = row.sessionId;
    if (!cwd       && row.cwd)       cwd = row.cwd;
    if (row.timestamp) {
      const ts = tsSec(row.timestamp);
      if (!startTime) startTime = ts;
      lastRowTime = ts;
    }

    if (row.type === 'assistant' && Array.isArray(row.message?.content)) {
      for (const block of row.message.content) {
        if (block.type === 'tool_use') {
          toolCount++;
          lastTool = block.name;
          lastTarget = extractTarget(block.name, block.input);
          toolUseTime = row.timestamp ? tsSec(row.timestamp) : toolUseTime;
        }
      }
    }
  }

  if (!sessionId || !cwd || !startTime) return null;
  // lastTime drives the 10-min freshness filter — use the most recent row,
  // not just the most recent tool_use, so a session where the model is
  // currently "thinking" still counts as active.
  const lastTime = lastRowTime ?? toolUseTime ?? startTime;
  return {
    id: sessionId,
    name: lastSegment(cwd),
    path: cwd,
    status: 'active',
    lastTool: lastTool ?? 'idle',
    lastTarget: lastTarget ?? '',
    lastTime,
    toolCount,
    startTime
  };
}

export async function listSessions(rootDir, opts = {}) {
  const now = opts.now ?? Math.floor(Date.now() / 1000);
  const projectDirs = await fs.readdir(rootDir, { withFileTypes: true }).catch(() => []);
  const out = [];
  for (const d of projectDirs) {
    if (!d.isDirectory()) continue;
    const dir = path.join(rootDir, d.name);
    const files = await fs.readdir(dir).catch(() => []);
    for (const f of files) {
      if (!f.endsWith('.jsonl')) continue;
      const session = await parseSession(path.join(dir, f)).catch(() => null);
      if (!session) continue;
      if (now - session.lastTime > FRESHNESS_S) continue;
      out.push(session);
    }
  }
  out.sort((a, b) => b.lastTime - a.lastTime);
  return out;
}
