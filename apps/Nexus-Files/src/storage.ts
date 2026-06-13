import { type BinaryLike, createHash, createHmac } from "node:crypto";

// ── Core types (defined here to avoid circular imports) ────────────────────

export type FileStatus = "active" | "deleted";

export interface FileRecord {
  id: string;
  name: string;
  size: number;
  contentType: string;
  sha256: string;
  version: number;
  status: FileStatus;
  createdAt: string;
  updatedAt: string;
}

export interface FileVersionRecord {
  fileId: string;
  version: number;
  size: number;
  sha256: string;
  createdAt: string;
}

// ── Storage Backend Interface ──────────────────────────────────────────────

export interface StorageBackend {
  init(): Promise<void>;
  storeFile(fileId: string, data: Buffer): Promise<void>;
  readFile(fileId: string): Promise<Buffer | null>;
  deleteFile(fileId: string): Promise<void>;
  fileExists(fileId: string): Promise<boolean>;
  deleteVersionFile(fileId: string, version: number): Promise<void>;
  loadMetadata(): Promise<{ files: FileRecord[]; versions: FileVersionRecord[] }>;
  saveMetadata(files: FileRecord[], versions: FileVersionRecord[]): Promise<void>;
  getStorageInfo(): Record<string, unknown>;
}

// ── Disk Storage Backend ───────────────────────────────────────────────────

import { existsSync } from "node:fs";
import { readFile as fsReadFile, mkdir, rename, unlink, writeFile } from "node:fs/promises";
import { join } from "node:path";

export interface DiskStorageOptions {
  root: string;
}

export function createDiskStorage(options: DiskStorageOptions): StorageBackend {
  const root = options.root;

  async function ensureRoot(): Promise<void> {
    if (!existsSync(root)) {
      await mkdir(root, { recursive: true });
    }
  }

  async function loadMetadataFile(): Promise<{
    files: FileRecord[];
    versions: FileVersionRecord[];
  }> {
    const metaPath = join(root, "metadata.json");
    if (!existsSync(metaPath)) {
      return { files: [], versions: [] };
    }
    const raw = await fsReadFile(metaPath, "utf8");
    return JSON.parse(raw) as { files: FileRecord[]; versions: FileVersionRecord[] };
  }

  async function saveMetadataFile(
    files: FileRecord[],
    versions: FileVersionRecord[],
  ): Promise<void> {
    const metaPath = join(root, "metadata.json");
    const tmp = metaPath + ".tmp";
    await writeFile(tmp, JSON.stringify({ files, versions }, null, 2), "utf8");
    await rename(tmp, metaPath);
  }

  return {
    init: ensureRoot,

    async storeFile(fileId: string, data: Buffer): Promise<void> {
      const filePath = join(root, fileId);
      await writeFile(filePath, data);
    },

    async readFile(fileId: string): Promise<Buffer | null> {
      const filePath = join(root, fileId);
      if (!existsSync(filePath)) return null;
      return fsReadFile(filePath);
    },

    async deleteFile(fileId: string): Promise<void> {
      const filePath = join(root, fileId);
      if (existsSync(filePath)) {
        await unlink(filePath);
      }
    },

    async fileExists(fileId: string): Promise<boolean> {
      return existsSync(join(root, fileId));
    },

    async deleteVersionFile(fileId: string, version: number): Promise<void> {
      const vPath = join(root, `${fileId}.v${version}`);
      if (existsSync(vPath)) {
        await unlink(vPath).catch(() => {});
      }
    },

    loadMetadata: loadMetadataFile,
    saveMetadata: saveMetadataFile,

    getStorageInfo(): Record<string, unknown> {
      return { engine: "disk", root };
    },
  };
}

// ── S3/MinIO Storage Backend (SigV4-signed fetch, zero new deps) ───────────

function sha256Hex(data: BinaryLike): string {
  return createHash("sha256").update(data).digest("hex");
}

function hmacSha256(key: BinaryLike, data: string): Buffer {
  return createHmac("sha256", key).update(data).digest();
}

function hmacSha256Hex(key: BinaryLike, data: string): string {
  return hmacSha256(key, data).toString("hex");
}

interface SigV4SigningParams {
  method: string;
  bucket: string;
  key: string;
  region: string;
  accessKey: string;
  secretKey: string;
  payload: Buffer | null;
  contentType: string;
  amzDate: string;
  dateStamp: string;
}

function sigV4SignRequest(params: SigV4SigningParams): {
  authorization: string;
  payloadHash: string;
} {
  const {
    method,
    bucket,
    key,
    region,
    accessKey,
    secretKey,
    payload,
    contentType,
    amzDate,
    dateStamp,
  } = params;
  const service = "s3";
  const host = `${bucket}.s3.${region}.amazonaws.com`;
  const payloadHash = sha256Hex(payload ?? Buffer.alloc(0));

  // Canonical request
  const canonicalUri = `/${key}`;
  const canonicalQueryString = "";
  const canonicalHeaders =
    [
      `content-type:${contentType}`,
      `host:${host}`,
      `x-amz-content-sha256:${payloadHash}`,
      `x-amz-date:${amzDate}`,
    ].join("\n") + "\n";
  const signedHeaders = "content-type;host;x-amz-content-sha256;x-amz-date";
  const canonicalRequest = [
    method,
    canonicalUri,
    canonicalQueryString,
    canonicalHeaders,
    signedHeaders,
    payloadHash,
  ].join("\n");

  // String to sign
  const credentialScope = `${dateStamp}/${region}/${service}/aws4_request`;
  const stringToSign = [
    "AWS4-HMAC-SHA256",
    amzDate,
    credentialScope,
    sha256Hex(canonicalRequest),
  ].join("\n");

  // Signing key
  const kDate = hmacSha256(`AWS4${secretKey}`, dateStamp);
  const kRegion = hmacSha256(kDate, region);
  const kService = hmacSha256(kRegion, service);
  const kSigning = hmacSha256(kService, "aws4_request");
  const signature = hmacSha256Hex(kSigning, stringToSign);

  const authorization = [
    `AWS4-HMAC-SHA256 Credential=${accessKey}/${credentialScope}`,
    `SignedHeaders=${signedHeaders}`,
    `Signature=${signature}`,
  ].join(", ");

  return { authorization, payloadHash };
}

export interface S3StorageOptions {
  endpoint: string; // e.g. "http://localhost:9000"
  bucket: string; // e.g. "nexus-files"
  region: string; // e.g. "us-east-1"
  accessKey: string;
  secretKey: string;
}

export function createS3Storage(options: S3StorageOptions): StorageBackend {
  const { endpoint, bucket, region, accessKey, secretKey } = options;
  const normalizedEndpoint = endpoint.replace(/\/+$/, "");

  async function s3Request(
    method: string,
    objectKey: string,
    body: Buffer | null = null,
    contentType = "application/octet-stream",
  ): Promise<Response> {
    const now = new Date();
    const dateStamp = now.toISOString().slice(0, 10).replace(/-/g, "");
    const amzDate = dateStamp + "T" + now.toISOString().slice(11, 19).replace(/:/g, "") + "Z";

    // For custom endpoints (MinIO), use path-style addressing
    const url = `${normalizedEndpoint}/${bucket}/${objectKey}`;
    const { authorization, payloadHash } = sigV4SignRequest({
      method,
      bucket,
      key: objectKey,
      region,
      accessKey,
      secretKey,
      payload: body,
      contentType:
        method === "GET" || method === "HEAD" || method === "DELETE"
          ? "application/octet-stream"
          : contentType,
      amzDate,
      dateStamp,
    });

    const headers: Record<string, string> = {
      Authorization: authorization,
      "x-amz-content-sha256": payloadHash,
      "x-amz-date": amzDate,
      Host: `${bucket}.s3.${region}.amazonaws.com`,
    };

    if (body && method !== "GET" && method !== "HEAD" && method !== "DELETE") {
      headers["Content-Type"] = contentType;
    }

    return fetch(url, { method, headers, body });
  }

  async function ensureBucket(): Promise<void> {
    // Bucket creation is handled externally (docker setup or admin).
    // We just verify connectivity by listing the bucket.
    try {
      const res = await s3Request("HEAD", "");
      // 200 or 404 both mean the endpoint is reachable
      if (!res.ok && res.status !== 404) {
        const text = await res.text().catch(() => "");
        throw new Error(`S3 bucket check failed: ${res.status} ${text.slice(0, 200)}`);
      }
    } catch (err) {
      if (err instanceof Error && err.message.startsWith("S3 bucket check failed")) throw err;
      throw new Error(
        `S3/MinIO unreachable at ${normalizedEndpoint}: ${err instanceof Error ? err.message : String(err)}`,
      );
    }
  }

  return {
    async init(): Promise<void> {
      await ensureBucket();
    },

    async storeFile(fileId: string, data: Buffer): Promise<void> {
      const res = await s3Request("PUT", fileId, data);
      if (!res.ok) {
        const text = await res.text().catch(() => "");
        throw new Error(`S3 PutObject failed: ${res.status} ${text.slice(0, 200)}`);
      }
    },

    async readFile(fileId: string): Promise<Buffer | null> {
      const res = await s3Request("GET", fileId);
      if (res.status === 404) return null;
      if (!res.ok) {
        const text = await res.text().catch(() => "");
        throw new Error(`S3 GetObject failed: ${res.status} ${text.slice(0, 200)}`);
      }
      return Buffer.from(await res.arrayBuffer());
    },

    async deleteFile(fileId: string): Promise<void> {
      const res = await s3Request("DELETE", fileId);
      if (!res.ok && res.status !== 404) {
        const text = await res.text().catch(() => "");
        throw new Error(`S3 DeleteObject failed: ${res.status} ${text.slice(0, 200)}`);
      }
    },

    async fileExists(fileId: string): Promise<boolean> {
      const res = await s3Request("HEAD", fileId);
      return res.status === 200;
    },

    async deleteVersionFile(_fileId: string, _version: number): Promise<void> {
      // S3 backend stores version history in metadata only — versions
      // are logical, not separate objects. This is a no-op.
    },

    async loadMetadata(): Promise<{ files: FileRecord[]; versions: FileVersionRecord[] }> {
      const res = await s3Request("GET", "metadata.json");
      if (res.status === 404) return { files: [], versions: [] };
      if (!res.ok) {
        const text = await res.text().catch(() => "");
        throw new Error(`S3 GetObject(metadata) failed: ${res.status} ${text.slice(0, 200)}`);
      }
      const json = await res.json();
      return json as { files: FileRecord[]; versions: FileVersionRecord[] };
    },

    async saveMetadata(files: FileRecord[], versions: FileVersionRecord[]): Promise<void> {
      const data = Buffer.from(JSON.stringify({ files, versions }, null, 2), "utf8");
      const res = await s3Request("PUT", "metadata.json", data, "application/json");
      if (!res.ok) {
        const text = await res.text().catch(() => "");
        throw new Error(`S3 PutObject(metadata) failed: ${res.status} ${text.slice(0, 200)}`);
      }
    },

    getStorageInfo(): Record<string, unknown> {
      return {
        engine: "s3",
        endpoint: normalizedEndpoint,
        bucket,
        region,
      };
    },
  };
}

// ── Backend Detection ──────────────────────────────────────────────────────

export function detectBackend(diskRoot?: string): StorageBackend {
  const s3Endpoint = process.env["S3_ENDPOINT"];
  if (s3Endpoint) {
    const backend = createS3Storage({
      endpoint: s3Endpoint,
      bucket: process.env["S3_BUCKET"] ?? "nexus-files",
      region: process.env["S3_REGION"] ?? "us-east-1",
      accessKey: process.env["S3_ACCESS_KEY"] ?? "",
      secretKey: process.env["S3_SECRET_KEY"] ?? "",
    });
    return backend;
  }

  return createDiskStorage({ root: diskRoot ?? join(process.cwd(), "data", "files") });
}

export function getDefaultDiskRoot(): string {
  return join(process.cwd(), "data", "files");
}
