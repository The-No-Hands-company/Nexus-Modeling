from fastapi import APIRouter, HTTPException
from pydantic import BaseModel
from app.tasks import generate_image_task, remove_background_task, enhance_photo_task, detect_objects_task

router = APIRouter(prefix="/api/v1/photos/ai", tags=["ai"])


class GenerateRequest(BaseModel):
    prompt: str
    width: int = 1024
    height: int = 1024
    style: str = "realistic"


class RemoveBgRequest(BaseModel):
    asset_id: str


class EnhanceRequest(BaseModel):
    photo_id: str
    enhancement: str = "auto"


class DetectRequest(BaseModel):
    photo_id: str


@router.post("/generate")
async def generate_image(body: GenerateRequest):
    if not body.prompt:
        raise HTTPException(400, "prompt is required")
    task = generate_image_task.delay(body.prompt, body.width, body.height, body.style, "")
    return {"task_id": task.id, "status": "queued"}


@router.post("/remove-bg")
async def remove_background(body: RemoveBgRequest):
    if not body.asset_id:
        raise HTTPException(400, "asset_id is required")
    task = remove_background_task.delay(body.asset_id)
    return {"task_id": task.id, "status": "queued"}


@router.post("/enhance")
async def enhance_photo(body: EnhanceRequest):
    if not body.photo_id:
        raise HTTPException(400, "photo_id is required")
    task = enhance_photo_task.delay(body.photo_id, body.enhancement)
    return {"task_id": task.id, "status": "queued"}


@router.post("/detect-objects")
async def detect_objects(body: DetectRequest):
    if not body.photo_id:
        raise HTTPException(400, "photo_id is required")
    task = detect_objects_task.delay(body.photo_id)
    return {"task_id": task.id, "status": "queued"}


@router.get("/tasks/{task_id}")
async def get_task_status(task_id: str):
    from celery.result import AsyncResult
    from app.celery_app import celery_app
    result = AsyncResult(task_id, app=celery_app)
    return {"task_id": task_id, "status": result.status, "result": result.result if result.ready() else None}
