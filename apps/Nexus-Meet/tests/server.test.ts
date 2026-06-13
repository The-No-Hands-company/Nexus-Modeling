import { afterAll, beforeAll, describe, expect, it } from "bun:test";
import { createServer } from "../src/server";

describe("nexus-meet", () => {
  let base = "";
  let handle: ReturnType<typeof createServer>;

  beforeAll(async () => {
    handle = createServer();
    await new Promise((r) => setTimeout(r, 200));
    base = `http://127.0.0.1:${handle.server.port}`;
  });

  afterAll(() => handle.close());

  it("GET /health returns 200", async () => {
    const res = await fetch(`${base}/health`);
    expect(res.status).toBe(200);
    const body = await res.json() as Record<string, unknown>;
    expect(body["service"]).toBe("nexus-meet");
    expect(body["status"]).toBe("ok");
  });

  it("GET /api/v1/status returns capabilities", async () => {
    const res = await fetch(`${base}/api/v1/status`);
    expect(res.status).toBe(200);
    const body = await res.json() as Record<string, unknown>;
    expect(body["service"]).toBe("nexus-meet");
    expect(Array.isArray(body["capabilities"])).toBe(true);
  });

  it("POST /api/v1/meet/rooms creates a room", async () => {
    const res = await fetch(`${base}/api/v1/meet/rooms`, { method: "POST", headers: { "content-type": "application/json" }, body: JSON.stringify({ name: "Team Standup", maxParticipants: 10 }) });
    expect(res.status).toBe(201);
    const body = await res.json() as any;
    expect(body.name).toBe("Team Standup");
  });

  it("POST /api/v1/meet/rooms/join joins a room", async () => {
    const roomRes = await fetch(`${base}/api/v1/meet/rooms`, { method: "POST", headers: { "content-type": "application/json" }, body: JSON.stringify({ name: "Meeting Room" }) });
    const room = await roomRes.json() as any;
    const res = await fetch(`${base}/api/v1/meet/rooms/join`, { method: "POST", headers: { "content-type": "application/json" }, body: JSON.stringify({ roomId: room.id, name: "Alice" }) });
    expect(res.status).toBe(200);
    const body = await res.json() as any;
    expect(body.name).toBe("Alice");
  });

  it("POST /api/v1/meet/schedules creates a schedule", async () => {
    const roomRes = await fetch(`${base}/api/v1/meet/rooms`, { method: "POST", headers: { "content-type": "application/json" }, body: JSON.stringify({ name: "Room" }) });
    const room = await roomRes.json() as any;
    const res = await fetch(`${base}/api/v1/meet/schedules`, { method: "POST", headers: { "content-type": "application/json" }, body: JSON.stringify({ title: "Sprint Review", roomId: room.id, startTime: "2026-06-14T10:00:00Z", duration: 60 }) });
    expect(res.status).toBe(201);
    const body = await res.json() as any;
    expect(body.title).toBe("Sprint Review");
    expect(body.duration).toBe(60);
  });
});
