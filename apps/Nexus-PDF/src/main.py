"""Nexus PDF — Browser-based PDF toolkit."""

from fastapi import FastAPI
from fastapi.middleware.cors import CORSMiddleware
import time

app = FastAPI(title="Nexus PDF", version="0.1.0")
app.add_middleware(CORSMiddleware, allow_origins=["*"], allow_methods=["*"], allow_headers=["*"])

STARTED_AT = time.time()


@app.get("/health")
async def health():
    return {
        "service": "nexus-pdf",
        "status": "ok",
        "version": "v1",
        "uptime_seconds": int(time.time() - STARTED_AT),
    }


@app.get("/api/v1/status")
async def status():
    return {
        "service": "nexus-pdf",
        "status": "ready",
        "capabilities": ["pdf-merge", "pdf-sign", "pdf-ocr", "ai-summarization", "pdf-convert"],
    }
