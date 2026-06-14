import { describe, it, expect, afterAll } from "bun:test";
import { createMockSDK, hexEncode, hexDecode, type PhantomSDK } from "../src/index";

describe("phantom-sdk (mock)", () => {
  let sdk: PhantomSDK;
  let handle: number;

  it("generates an identity", async () => {
    sdk = createMockSDK();
    const id = await sdk.generateIdentity("test-user");
    expect(id.handle).toBeGreaterThan(0);
    expect(id.did).toStartWith("did:phantom:");
    expect(id.publicKey.length).toBeGreaterThan(0);
    handle = id.handle;
  });

  it("signs and verifies", async () => {
    const msg = new TextEncoder().encode("hello phantom");
    const sig = await sdk.sign(handle, msg);
    expect(sig.length).toBeGreaterThan(0);
    const valid = await sdk.verify(handle, msg, sig);
    expect(valid).toBe(true);
  });

  it("rejects tampered messages", async () => {
    const msg = new TextEncoder().encode("original");
    const sig = await sdk.sign(handle, msg);
    const tampered = await sdk.verify(handle, new TextEncoder().encode("tampered"), sig);
    expect(tampered).toBe(false);
  });

  it("encapsulates and decapsulates", async () => {
    const { ciphertext, sharedSecret } = await sdk.encapsulate(handle);
    const recovered = await sdk.decapsulate(handle, ciphertext);
    expect(recovered).toBe(sharedSecret);
  });

  it("hashes with blake3", async () => {
    const h = await sdk.phantomHash(new TextEncoder().encode("phantom"));
    expect(h.length).toBe(64);
  });

  it("handles multiple identities", async () => {
    const alice = await sdk.generateIdentity("alice");
    const bob = await sdk.generateIdentity("bob");
    expect(alice.handle).not.toBe(bob.handle);
    sdk.release(alice.handle);
    sdk.release(bob.handle);
  });

  it("releases identity", () => {
    const h = handle;
    sdk.release(h);
    expect(sdk.getDID(h)).resolves.toBe("");
  });
});
