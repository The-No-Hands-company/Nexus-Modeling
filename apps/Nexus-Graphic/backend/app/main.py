from contextlib import asynccontextmanager
from fastapi import FastAPI
from fastapi.middleware.cors import CORSMiddleware
from app.config import settings
from app.routes import projects, assets, ai
from app.models import Base
from app.deps import engine


@asynccontextmanager
async def lifespan(_app: FastAPI):
    async with engine.begin() as conn:
        await conn.run_sync(Base.metadata.create_all)
    yield
    await engine.dispose()


app = FastAPI(
    title="Nexus-Graphic API",
    version="0.1.0",
    lifespan=lifespan,
)

app.add_middleware(
    CORSMiddleware,
    allow_origins=settings.cors_origins.split(","),
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)

app.include_router(projects.router)
app.include_router(assets.router)
app.include_router(ai.router)


@app.get("/health")
async def health():
    return {"service": "nexus-graphic", "status": "ok", "version": "v1"}


@app.get("/api/v1/status")
async def status():
    return {
        "service": "nexus-graphic",
        "status": "ready",
        "capabilities": ["graphic-design", "photo-editing", "ai-generation", "vector", "raster"],
        "cloud_integration": {"enabled": settings.enable_cloud},
    }
