import { z } from 'zod';

const v = z.literal(1);

export const SessionStatus = z.enum(['active', 'idle', 'waiting']);

export const Session = z.object({
  id: z.string(),
  name: z.string(),
  path: z.string(),
  status: SessionStatus,
  lastTool: z.string(),
  lastTarget: z.string(),
  lastTime: z.number().int(),
  toolCount: z.number().int().nonnegative(),
  startTime: z.number().int()
});

export const Quota = z.object({
  fiveHour: z.object({ utilization: z.number().min(0).max(1), reset: z.number().int() }),
  sevenDay: z.object({ utilization: z.number().min(0).max(1), reset: z.number().int() }),
  opus:     z.object({ utilization: z.number().min(0).max(1) }).nullable(),
  lastUpdate: z.number().int()
}).nullable();

export const Alert = z.object({
  id: z.string(),
  kind: z.literal('approval'),
  sessionId: z.string(),
  sessionName: z.string(),
  text: z.string().max(80),
  ts: z.number().int()
});

// Bridge → ESP32
export const HelloMessage = z.object({
  v, type: z.literal('hello'), bridge: z.string(), host: z.string()
});
export const StateMessage = z.object({
  v, type: z.literal('state'),
  sessions: z.object({ active: z.number().int(), items: z.array(Session) }),
  quota: Quota,
  alert: Alert.nullable()
});
export const SessionUpdateMessage = z.object({
  v, type: z.literal('session-update'),
  id: z.string(),
  patch: z.object({
    name: z.string().optional(),
    path: z.string().optional(),
    status: SessionStatus.optional(),
    lastTool: z.string().optional(),
    lastTarget: z.string().optional(),
    lastTime: z.number().int().optional(),
    toolCount: z.number().int().optional()
  })
});
export const SessionEndMessage = z.object({
  v, type: z.literal('session-end'), id: z.string()
});
export const QuotaMessage = z.object({
  v, type: z.literal('quota'),
  fiveHour: z.object({ utilization: z.number(), reset: z.number().int() }),
  sevenDay: z.object({ utilization: z.number(), reset: z.number().int() }),
  opus:     z.object({ utilization: z.number() }).nullable()
});
export const AlertMessage = z.object({
  v, type: z.literal('alert'),
  id: z.string(),
  kind: z.literal('approval'),
  sessionId: z.string(),
  sessionName: z.string(),
  text: z.string()
});
export const AlertClearMessage = z.object({
  v, type: z.literal('alert-clear'), id: z.string()
});
export const TimeSyncMessage = z.object({
  v, type: z.literal('time-sync'), utc: z.number().int(), tz: z.string()
});
export const PingMessage = z.object({
  v, type: z.literal('ping'), ts: z.number().int()
});

// ESP32 → Bridge
export const HelloAckMessage = z.object({
  v, type: z.literal('hello-ack'),
  fw: z.string(), lvgl: z.string(), ip: z.string(), mac: z.string()
});
export const PongMessage = z.object({
  v, type: z.literal('pong'), ts: z.number().int()
});
export const EnvMessage = z.object({
  v, type: z.literal('env'), tempC: z.number(), humidity: z.number()
});
export const DiagMessage = z.object({
  v, type: z.literal('diag'), free_heap: z.number().int(), rssi: z.number()
});
export const ErrorMessage = z.object({
  v, type: z.literal('error'), reason: z.string()
});

const outboundUnion = z.discriminatedUnion('type', [
  HelloMessage, StateMessage, SessionUpdateMessage, SessionEndMessage,
  QuotaMessage, AlertMessage, AlertClearMessage, TimeSyncMessage, PingMessage
]);

const inboundUnion = z.discriminatedUnion('type', [
  HelloAckMessage, PongMessage, EnvMessage, DiagMessage, ErrorMessage
]);

export function parseOutbound(obj) { return outboundUnion.parse(obj); }
export function parseInbound(obj)  { return inboundUnion.parse(obj); }
