"""Nexus AI Hub — AI model and provider management."""

from fastapi import FastAPI
from fastapi.middleware.cors import CORSMiddleware
from pydantic import BaseModel
import time
import uuid
from typing import Optional

app = FastAPI(title="Nexus AI Hub", version="0.1.0")
app.add_middleware(CORSMiddleware, allow_origins=["*"], allow_methods=["*"], allow_headers=["*"])

STARTED_AT = time.time()
models: dict[str, dict] = {}
providers: dict[str, dict] = {
    "ollama": {"id": "ollama", "name": "Ollama", "type": "local", "endpoint": "http://localhost:11434", "status": "available", "models": []},
    "groq": {"id": "groq", "name": "Groq", "type": "cloud", "endpoint": "https://api.groq.com/openai/v1", "status": "available", "models": []},
    "gemini": {"id": "gemini", "name": "Google Gemini", "type": "cloud", "endpoint": "https://generativelanguage.googleapis.com/v1beta", "status": "available", "models": []},
    "openrouter": {"id": "openrouter", "name": "OpenRouter", "type": "cloud", "endpoint": "https://openrouter.ai/api/v1", "status": "available", "models": []},
}
rag_pipelines: dict[str, dict] = {}

class ModelRegister(BaseModel):
    name: str
    provider_id: str
    model_id: str
    description: str = ""
    context_length: int = 4096
    capabilities: list[str] = []

class RagPipeline(BaseModel):
    name: str
    description: str = ""
    documents: list[str] = []
    embedding_model: str = "default"
    chunk_size: int = 512

@app.get("/health")
async def health():
    return {"service": "nexus-ai-hub", "status": "ok", "version": "v1", "uptime_seconds": int(time.time() - STARTED_AT)}

@app.get("/api/v1/hub/status")
async def status():
    return {"providers": len(providers), "models": len(models), "rag_pipelines": len(rag_pipelines)}

@app.get("/api/v1/hub/providers")
async def list_providers():
    return {"providers": list(providers.values())}

@app.get("/api/v1/hub/providers/{provider_id}")
async def get_provider(provider_id: str):
    if provider_id not in providers: return {"error": "not found"}
    return {"provider": providers[provider_id]}

@app.get("/api/v1/hub/models")
async def list_models(provider_id: Optional[str] = None):
    if provider_id: return {"models": [m for m in models.values() if m["provider_id"] == provider_id]}
    return {"models": list(models.values())}

@app.post("/api/v1/hub/models")
async def register_model(model: ModelRegister):
    if model.provider_id not in providers: return {"error": "provider not found"}
    m = {"id": str(uuid.uuid4()), "name": model.name, "provider_id": model.provider_id, "model_id": model.model_id, "description": model.description, "context_length": model.context_length, "capabilities": model.capabilities, "status": "active", "registered_at": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime())}
    models[m["id"]] = m
    providers[model.provider_id]["models"].append(m["id"])
    return {"model": m}

@app.delete("/api/v1/hub/models/{model_id}")
async def delete_model(model_id: str):
    if model_id not in models: return {"error": "not found"}
    pid = models[model_id]["provider_id"]
    providers[pid]["models"].remove(model_id)
    del models[model_id]
    return {"deleted": True}

@app.get("/api/v1/hub/rag")
async def list_rag():
    return {"pipelines": list(rag_pipelines.values())}

@app.post("/api/v1/hub/rag")
async def create_rag(pipeline: RagPipeline):
    p = {"id": str(uuid.uuid4()), "name": pipeline.name, "description": pipeline.description, "documents": pipeline.documents, "embedding_model": pipeline.embedding_model, "chunk_size": pipeline.chunk_size, "status": "created", "indexed_count": len(pipeline.documents), "created_at": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime())}
    rag_pipelines[p["id"]] = p
    return {"pipeline": p}

@app.post("/api/v1/hub/route")
async def route_request(request: dict):
    """Route an AI request to the appropriate provider/model."""
    provider_id = request.get("provider", "ollama")
    if provider_id not in providers: return {"error": "provider not found", "status": "failed"}
    return {"status": "routed", "provider": provider_id, "endpoint": providers[provider_id]["endpoint"], "available_models": len(providers[provider_id]["models"])}
