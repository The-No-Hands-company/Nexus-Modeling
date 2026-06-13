import hashlib
from fastapi import APIRouter, Depends, UploadFile, File, HTTPException
from sqlalchemy.ext.asyncio import AsyncSession
from app.models import Asset
from app.deps import get_db, get_storage

router = APIRouter(prefix="/api/v1/draw/assets", tags=["assets"])


@router.post("/upload")
async def upload_asset(
    file: UploadFile = File(...),
    board_id: str = "",
    db: AsyncSession = Depends(get_db),
    storage=Depends(get_storage),
):
    content = await file.read()
    content_hash = hashlib.sha256(content).hexdigest()
    ext = file.filename.split(".")[-1] if "." in (file.filename or "") else "bin"
    storage_key = f"assets/{content_hash[:4]}/{content_hash}.{ext}"

    await storage.put(storage_key, content, file.content_type or "application/octet-stream")

    asset = Asset(
        board_id=board_id or None,
        filename=file.filename or "untitled",
        content_type=file.content_type or "application/octet-stream",
        size=len(content),
        storage_key=storage_key,
    )
    db.add(asset)
    await db.commit()
    await db.refresh(asset)
    return asset


@router.get("/{asset_id}")
async def get_asset(asset_id: str, db: AsyncSession = Depends(get_db)):
    from sqlalchemy import select
    result = await db.execute(select(Asset).where(Asset.id == asset_id))
    asset = result.scalar_one_or_none()
    if not asset:
        raise HTTPException(404, "Asset not found")
    return asset
