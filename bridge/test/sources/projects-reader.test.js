import { describe, it, expect } from 'vitest';
import path from 'node:path';
import { listSessions } from '../../src/sources/projects-reader.js';

const FIXTURE_ROOT = path.resolve(import.meta.dirname, '../fixtures/projects');
// 2026-04-28T10:02:00Z fixed
const SAMPLE_NOW = Math.floor(Date.parse('2026-04-28T10:02:30Z') / 1000);

describe('listSessions', () => {
  it('parses sessions with last tool/target', async () => {
    const sessions = await listSessions(FIXTURE_ROOT, { now: SAMPLE_NOW });
    expect(sessions).toHaveLength(1);
    const [s] = sessions;
    expect(s.id).toBe('sess-1');
    expect(s.lastTool).toBe('Bash');
    expect(s.lastTarget).toBe('git status');
    expect(s.toolCount).toBe(2);
    expect(typeof s.lastTime).toBe('number');
    expect(typeof s.startTime).toBe('number');
  });

  it('skips sessions whose lastTime is older than 10 minutes', async () => {
    const sessions = await listSessions(FIXTURE_ROOT, { now: SAMPLE_NOW + 7200 });
    expect(sessions).toHaveLength(0);
  });

  it('returns name as last segment of cwd', async () => {
    const sessions = await listSessions(FIXTURE_ROOT, { now: SAMPLE_NOW });
    expect(sessions[0].name).toBe('claude code小工具');
  });
});
