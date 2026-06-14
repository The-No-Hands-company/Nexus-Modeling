import { afterAll, beforeAll, describe, expect, it } from "bun:test";
import { createServer } from "../src/server";

describe("nexus-market", () => {
  let base = "";
  let handle: Awaited<ReturnType<typeof createServer>>;

  beforeAll(async () => {
    handle = await createServer();
    await new Promise((r) => setTimeout(r, 200));
    base = `http://127.0.0.1:${handle.server.port}`;
  });

  afterAll(() => handle.close());

  it("GET /health returns 200", async () => {
    const res = await fetch(`${base}/health`);
    expect(res.status).toBe(200);
    const body = await res.json() as Record<string, unknown>;
    expect(body["service"]).toBe("nexus-market");
    expect(body["status"]).toBe("ok");
    expect(body["phantom"]).toBeDefined();
  });

  it("GET /api/v1/status returns capabilities", async () => {
    const res = await fetch(`${base}/api/v1/status`);
    expect(res.status).toBe(200);
    const body = await res.json() as Record<string, unknown>;
    expect(body["service"]).toBe("nexus-market");
    expect(Array.isArray(body["capabilities"])).toBe(true);
  });

  it("POST /api/v1/market/listings creates a listing", async () => {
    const res = await fetch(`${base}/api/v1/market/listings`, { method: "POST", headers: { "content-type": "application/json" }, body: JSON.stringify({ title: "Vintage Chair", price: 150, seller: "user1", category: "furniture" }) });
    expect(res.status).toBe(201);
    const body = await res.json() as any;
    expect(body.title).toBe("Vintage Chair");
    expect(body.price).toBe(150);
  });

  it("GET /api/v1/market/listings lists listings", async () => {
    const res = await fetch(`${base}/api/v1/market/listings`);
    expect(res.status).toBe(200);
    const body = await res.json() as any[];
    expect(Array.isArray(body)).toBe(true);
  });

  it("POST /api/v1/market/orders creates an order", async () => {
    const listingRes = await fetch(`${base}/api/v1/market/listings`, { method: "POST", headers: { "content-type": "application/json" }, body: JSON.stringify({ title: "Item", price: 50, seller: "seller", category: "general" }) });
    const listing = await listingRes.json() as any;
    const res = await fetch(`${base}/api/v1/market/orders`, { method: "POST", headers: { "content-type": "application/json" }, body: JSON.stringify({ listingId: listing.id, buyer: "buyer1", amount: 50 }) });
    expect(res.status).toBe(201);
    const body = await res.json() as any;
    expect(body.listingId).toBe(listing.id);
    expect(body.buyer).toBe("buyer1");
  });
});
