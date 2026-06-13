export type SystemsApiRegistrationPayload = {
  id: string;
  name: string;
  description: string;
  mode: "orchestrated" | "standalone";
  exposed: boolean;
  health: "healthy" | "degraded" | "offline";
  upstreamUrl: string;
  capabilities: string[];
  metadata: {
    filesVersion: string;
    supportsContentUpload: boolean;
    supportsContentDownload: boolean;
    supportsVersioning: boolean;
    supportsS3Backend: boolean;
    supportsSearch: boolean;
    defaultPort: number;
  };
};

export function buildSystemsApiRegistrationPayload(baseUrl: string): SystemsApiRegistrationPayload {
  return {
    id: "nexus-files",
    name: "Nexus Files",
    description: "File storage service with content upload/download, versioning, soft delete, S3/Disk backends, and search — the ecosystem's file layer",
    mode: "orchestrated",
    exposed: false,
    health: "healthy",
    upstreamUrl: baseUrl,
    capabilities: [
      "file-storage",
      "content-upload",
      "content-download",
      "versioning",
      "search",
      "s3-backend",
    ],
    metadata: {
      filesVersion: "v2",
      supportsContentUpload: true,
      supportsContentDownload: true,
      supportsVersioning: true,
      supportsS3Backend: true,
      supportsSearch: true,
      defaultPort: 3033,
    },
  };
}
