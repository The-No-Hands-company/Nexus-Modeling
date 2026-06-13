"""Nexus Email — Sovereign private email server."""

from fastapi import FastAPI
from fastapi.middleware.cors import CORSMiddleware
import os
import time

app = FastAPI(title="Nexus Email", version="0.1.0")
app.add_middleware(CORSMiddleware, allow_origins=["*"], allow_methods=["*"], allow_headers=["*"])

STARTED_AT = time.time()


@app.get("/health")
async def health():
    return {
        "service": "nexus-email",
        "status": "ok",
        "version": "v1",
        "uptime_seconds": int(time.time() - STARTED_AT),
    }


@app.get("/api/v1/status")
async def status():
    return {
        "service": "nexus-email",
        "status": "ready",
        "capabilities": ["email", "smtp", "imap", "ai-filtering", "calendar-integration"],
    }
