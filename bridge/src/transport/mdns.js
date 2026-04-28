import { Bonjour } from 'bonjour-service';

const defaultBonjour = () => new Bonjour();

export async function resolveDevice(name, opts = {}) {
  const bonjour = opts.bonjour ?? defaultBonjour();
  const timeoutMs = opts.timeoutMs ?? 5000;
  const browser = bonjour.find({ type: 'http' });
  return new Promise((resolve, reject) => {
    const timer = setTimeout(() => {
      try { browser.stop(); } catch {}
      reject(new Error('mDNS timeout'));
    }, timeoutMs);
    browser.on('up', (svc) => {
      if (svc.name === name && svc.addresses?.length) {
        clearTimeout(timer);
        try { browser.stop(); } catch {}
        const ipv4 = svc.addresses.find((a) => /^\d+\.\d+\.\d+\.\d+$/.test(a)) ?? svc.addresses[0];
        resolve({ host: ipv4, port: svc.port ?? 80 });
      }
    });
  });
}
