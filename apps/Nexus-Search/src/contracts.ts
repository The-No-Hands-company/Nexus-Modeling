export type SystemsApiRegistrationPayload = {
  id: string; name: string; description: string;
  mode: "orchestrated" | "standalone"; exposed: boolean;
  health: "healthy" | "degraded" | "offline"; upstreamUrl: string;
  capabilities: string[]; metadata: Record<string, unknown>;
};

export function buildSystemsApiRegistrationPayload(baseUrl: string): SystemsApiRegistrationPayload {
  return {
    id: "nexus-search", name: "Nexus Search",
    description: "Cross-ecosystem full-text search with FTS5 inverted index, relevance scoring, multi-source federation, and snippet generation",
    mode: "orchestrated", exposed: false, health: "healthy", upstreamUrl: baseUrl,
    capabilities: ["full-text-search", "fts5-index", "relevance-scoring", "multi-source", "snippet-generation", "federated-sources", "auto-sync", "facets"],
    metadata: { searchVersion: "v2", supportsFTS: true, supportsMultiSource: true, supportsFederatedSources: true, supportsAutoSync: true, supportsFacets: true, defaultPort: 3034 },
  };
}
