import { afterAll, beforeAll, describe, expect, it } from "bun:test";
import { createServer } from "../src/server";

describe("nexus-home", () => {
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
    expect(body["service"]).toBe("nexus-home");
    expect(body["status"]).toBe("ok");
  });

  it("GET /api/v1/status returns capabilities", async () => {
    const res = await fetch(`${base}/api/v1/status`);
    expect(res.status).toBe(200);
    const body = await res.json() as Record<string, unknown>;
    expect(body["service"]).toBe("nexus-home");
    expect(Array.isArray(body["capabilities"])).toBe(true);
  });

  it("POST /api/v1/home/rooms creates a room", async () => {
    const res = await fetch(`${base}/api/v1/home/rooms`, { method: "POST", headers: { "content-type": "application/json" }, body: JSON.stringify({ name: "Living Room" }) });
    expect(res.status).toBe(201);
    const body = await res.json() as any;
    expect(body.name).toBe("Living Room");
  });

  it("GET /api/v1/home/rooms lists rooms", async () => {
    const res = await fetch(`${base}/api/v1/home/rooms`);
    expect(res.status).toBe(200);
    const body = await res.json() as any[];
    expect(Array.isArray(body)).toBe(true);
  });

  it("POST /api/v1/home/devices adds a device", async () => {
    const roomRes = await fetch(`${base}/api/v1/home/rooms`, { method: "POST", headers: { "content-type": "application/json" }, body: JSON.stringify({ name: "Test Room" }) });
    const room = await roomRes.json() as any;
    const res = await fetch(`${base}/api/v1/home/devices`, { method: "POST", headers: { "content-type": "application/json" }, body: JSON.stringify({ roomId: room.id, name: "Lamp", type: "light" }) });
    expect(res.status).toBe(201);
    const body = await res.json() as any;
    expect(body.name).toBe("Lamp");
    expect(body.status).toBe("off");
  });

  it("POST toggle device", async () => {
    const roomRes = await fetch(`${base}/api/v1/home/rooms`, { method: "POST", headers: { "content-type": "application/json" }, body: JSON.stringify({ name: "T" }) });
    const room = await roomRes.json() as any;
    const deviceRes = await fetch(`${base}/api/v1/home/devices`, { method: "POST", headers: { "content-type": "application/json" }, body: JSON.stringify({ roomId: room.id, name: "Switch", type: "switch" }) });
    const device = await deviceRes.json() as any;
    const toggleRes = await fetch(`${base}/api/v1/home/devices/${device.id}/toggle`, { method: "POST" });
    expect(toggleRes.status).toBe(200);
    const toggled = await toggleRes.json() as any;
    expect(toggled.status).toBe("on");
  });
});
