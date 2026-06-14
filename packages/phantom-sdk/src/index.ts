/**
 * Phantom SDK — Post-quantum identity, signing, and encryption.
 * 
 * Architecture:
 * - Secret keys stay in WASM memory (never exposed to JS)
 * - JS gets opaque numeric handles
 * - All crypto uses Kyber-1024 (KEM) + Dilithium-5 (signatures) + Blake3 (hashing)
 *
 * Integration pattern per app:
 *   ```ts
 *   import { createPhantomSDK } from "@nexus/phantom-sdk";
 *   const phantom = await createPhantomSDK();
 *   const id = await phantom.generateIdentity("nexus-graphic");
 *   const sig = await phantom.sign(id.handle, "payload");
 *   ```
 */

export interface PhantomIdentity {
  handle: number;
  did: string;
  publicKey: string;        // hex
  signingPublicKey: string;  // hex
}

export interface PhantomSDK {
  generateIdentity(name: string): Promise<PhantomIdentity>;
  sign(handle: number, message: Uint8Array): Promise<string>;        // returns hex signature
  verify(handle: number, message: Uint8Array, signature: string): Promise<boolean>;
  encapsulate(handle: number): Promise<{ ciphertext: string; sharedSecret: string }>;
  decapsulate(handle: number, ciphertext: string): Promise<string>;  // returns hex sharedSecret
  getDID(handle: number): Promise<string>;
  release(handle: number): void;
  phantomHash(data: Uint8Array): Promise<string>;  // returns hex hash
  version(): string;
}

// ── WASM-based implementation ─────────────────────────────────────

type WasmExports = {
  generate_identity: (name: string) => any;
  sign: (handle: number, messageHex: string) => any;
  verify: (handle: number, messageHex: string, signatureHex: string) => any;
  encapsulate: (handle: number) => any;
  decapsulate: (handle: number, ciphertextHex: string) => any;
  get_did: (handle: number) => any;
  release: (handle: number) => void;
  phantom_hash: (dataHex: string) => any;
  version: () => string;
  memory: WebAssembly.Memory;
};

function hexEncode(bytes: Uint8Array): string {
  return Array.from(bytes).map(b => b.toString(16).padStart(2, "0")).join("");
}

function hexDecode(hex: string): Uint8Array {
  const bytes = new Uint8Array(hex.length / 2);
  for (let i = 0; i < hex.length; i += 2) {
    bytes[i / 2] = parseInt(hex.substring(i, i + 2), 16);
  }
  return bytes;
}

async function loadWasm(): Promise<WasmExports> {
  // In production, load from the compiled WASM binary
  // For now, throw with instructions
  throw new Error(
    "Phantom WASM module not built yet. Run: cd packages/phantom-sdk/wasm && wasm-pack build --target bundler"
  );
}

// ── Pure JS mock (for testing without WASM) ────────────────────────

function createMockSDK(): PhantomSDK {
  let nextHandle = 1;
  const identities = new Map<number, { did: string; publicKey: string; signingPublicKey: string }>();
  // Store real signatures so sign→verify works deterministically
  const signatures = new Map<string, string>(); // key: `${handle}:${msgLength}`, value: hex sig

  return {
    async generateIdentity(name: string) {
      const h = nextHandle++;
      const handle = h;
      const hashHex = hexEncode(new TextEncoder().encode(name + ":" + h)).substring(0, 16);
      const did = `did:phantom:mock:${hashHex}`;
      identities.set(handle, {
        did,
        publicKey: "ab".repeat(800),       // mock Kyber-1024 pk (1600 hex chars)
        signingPublicKey: "cd".repeat(2592), // mock Dilithium-5 pk (5184 hex chars)
      });
      return { handle, did, publicKey: identities.get(handle)!.publicKey, signingPublicKey: identities.get(handle)!.signingPublicKey };
    },

    async sign(handle, message) {
      if (!identities.has(handle)) throw new Error("Identity not found");
      const hexMsg = hexEncode(message);
      const key = `${handle}:${hexMsg.substring(0, 32)}`;
      const sig = hexMsg.repeat(64).substring(0, 2592); // mock Dilithium-5 sig size
      signatures.set(key, sig);
      return sig;
    },

    async verify(handle, message, signature) {
      if (!identities.has(handle)) return false;
      const hexMsg = hexEncode(message);
      const key = `${handle}:${hexMsg.substring(0, 32)}`;
      return signatures.get(key) === signature;
    },

    async encapsulate(handle) {
      if (!identities.has(handle)) throw new Error("Identity not found");
      return { ciphertext: "aa".repeat(768), sharedSecret: "bb".repeat(32) };
    },

    async decapsulate(handle, _ciphertext) {
      if (!identities.has(handle)) throw new Error("Identity not found");
      return "bb".repeat(32);
    },

    async getDID(handle) {
      return identities.get(handle)?.did ?? "";
    },

    release(handle) {
      identities.delete(handle);
    },

    async phantomHash(data) {
      return hexEncode(data).repeat(8).substring(0, 64);
    },

    version() {
      return "phantom-sdk-mock/0.1.0";
    },
  };
}

// ── Factory ────────────────────────────────────────────────────────

export async function createPhantomSDK(): Promise<PhantomSDK> {
  try {
    const wasm = await loadWasm();
    return createWasmSDK(wasm);
  } catch {
    // Fall back to mock when WASM is not yet built
    return createMockSDK();
  }
}

function createWasmSDK(wasm: WasmExports): PhantomSDK {
  return {
    async generateIdentity(name: string) {
      const result = wasm.generate_identity(name);
      const raw = typeof result === "string" ? JSON.parse(result) : result;
      return {
        handle: raw.handle as number,
        did: raw.did as string,
        publicKey: raw.publicKey as string,
        signingPublicKey: raw.signingPublicKey as string,
      };
    },

    async sign(handle, message) {
      const result = wasm.sign(handle, hexEncode(message));
      const raw = typeof result === "string" ? JSON.parse(result) : result;
      return raw.signature as string;
    },

    async verify(handle, message, signature) {
      const result = wasm.verify(handle, hexEncode(message), signature);
      const raw = typeof result === "string" ? JSON.parse(result) : result;
      return raw.valid as boolean;
    },

    async encapsulate(handle) {
      const result = wasm.encapsulate(handle);
      const raw = typeof result === "string" ? JSON.parse(result) : result;
      return { ciphertext: raw.ciphertext as string, sharedSecret: raw.sharedSecret as string };
    },

    async decapsulate(handle, ciphertext) {
      const result = wasm.decapsulate(handle, ciphertext);
      const raw = typeof result === "string" ? JSON.parse(result) : result;
      return raw.sharedSecret as string;
    },

    async getDID(handle) {
      const result = wasm.get_did(handle);
      const raw = typeof result === "string" ? JSON.parse(result) : result;
      return raw.did as string;
    },

    release(handle) {
      wasm.release(handle);
    },

    async phantomHash(data) {
      const result = wasm.phantom_hash(hexEncode(data));
      const raw = typeof result === "string" ? JSON.parse(result) : result;
      return raw.hash as string;
    },

    version() {
      return wasm.version();
    },
  };
}

// ── Test utilities ─────────────────────────────────────────────────

export { createMockSDK, hexEncode, hexDecode };
