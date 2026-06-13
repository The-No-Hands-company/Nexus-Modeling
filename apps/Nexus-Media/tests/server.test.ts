import { afterAll, beforeAll, describe, expect, it } from "bun:test";
import { createServer } from "../src/server";

describe("nexus-media", () => {
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
    expect(body["service"]).toBe("nexus-media");
    expect(body["status"]).toBe("ok");
  });

  it("GET /api/v1/status returns capabilities", async () => {
    const res = await fetch(`${base}/api/v1/status`);
    expect(res.status).toBe(200);
    const body = await res.json() as Record<string, unknown>;
    expect(body["service"]).toBe("nexus-media");
    expect(Array.isArray(body["capabilities"])).toBe(true);
  });

  it("POST /api/v1/media/albums creates an album", async () => {
    const res = await fetch(`${base}/api/v1/media/albums`, { method: "POST", headers: { "content-type": "application/json" }, body: JSON.stringify({ name: "Vacation Photos", description: "Summer 2026" }) });
    expect(res.status).toBe(201);
    const body = await res.json() as any;
    expect(body.name).toBe("Vacation Photos");
  });

  it("POST /api/v1/media/assets adds an asset", async () => {
    const albumRes = await fetch(`${base}/api/v1/media/albums`, { method: "POST", headers: { "content-type": "application/json" }, body: JSON.stringify({ name: "Album" }) });
    const album = await albumRes.json() as any;
    const res = await fetch(`${base}/api/v1/media/assets`, { method: "POST", headers: { "content-type": "application/json" }, body: JSON.stringify({ name: "sunset.jpg", type: "image", url: "/media/sunset.jpg", size: 1024, tags: ["nature", "sunset"], albumId: album.id }) });
    expect(res.status).toBe(201);
    const body = await res.json() as any;
    expect(body.name).toBe("sunset.jpg");
    expect(body.type).toBe("image");
  });

  it("GET /api/v1/media/assets lists assets", async () => {
    const res = await fetch(`${base}/api/v1/media/assets`);
    expect(res.status).toBe(200);
    const body = await res.json() as any[];
    expect(Array.isArray(body)).toBe(true);
  });
});
