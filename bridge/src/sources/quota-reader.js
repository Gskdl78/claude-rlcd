import fs from 'node:fs/promises';

export async function readQuota(filePath) {
  try {
    const raw = await fs.readFile(filePath, 'utf8');
    const data = JSON.parse(raw);
    if (!data || !data.fiveHour || !data.sevenDay) return null;
    return {
      fiveHour: data.fiveHour,
      sevenDay: data.sevenDay,
      opus: data.opus ?? null,
      lastUpdate: data.timestamp ? Math.floor(data.timestamp / 1000) : Math.floor(Date.now() / 1000)
    };
  } catch {
    return null;
  }
}
