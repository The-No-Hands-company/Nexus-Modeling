from fastapi import APIRouter, Depends, HTTPException, Query
from sqlalchemy import select, delete
from sqlalchemy.ext.asyncio import AsyncSession
from pydantic import BaseModel
from typing import Optional
from app.models import Photo, Album
from app.deps import get_db

router = APIRouter(prefix="/api/v1/photos", tags=["photos"])


class PhotoCreate(BaseModel):
    album_id: Optional[str] = None
    title: str = ""
    description: str = ""
    url: str
    thumbnail_url: str = ""
    width: int = 0
    height: int = 0
    file_size: int = 0
    content_type: str = "image/jpeg"
    tags: str = ""
    is_favorite: bool = False
    exif_data: dict = {}


class PhotoUpdate(BaseModel):
    album_id: Optional[str] = None
    title: Optional[str] = None
    description: Optional[str] = None
    tags: Optional[str] = None
    is_favorite: Optional[bool] = None


class AlbumCreate(BaseModel):
    name: str
    description: str = ""
    cover_url: str = ""


class AlbumUpdate(BaseModel):
    name: Optional[str] = None
    description: Optional[str] = None
    cover_url: Optional[str] = None


@router.get("")
async def list_photos(
    album_id: Optional[str] = Query(None),
    tags: Optional[str] = Query(None),
    is_favorite: Optional[bool] = Query(None),
    skip: int = Query(0, ge=0),
    limit: int = Query(100, ge=1, le=500),
    db: AsyncSession = Depends(get_db),
):
    stmt = select(Photo)
    if album_id:
        stmt = stmt.where(Photo.album_id == album_id)
    if tags:
        stmt = stmt.where(Photo.tags.contains(tags))
    if is_favorite is not None:
        stmt = stmt.where(Photo.is_favorite == is_favorite)
    stmt = stmt.offset(skip).limit(limit).order_by(Photo.created_at.desc())
    result = await db.execute(stmt)
    return result.scalars().all()


@router.post("")
async def create_photo(body: PhotoCreate, db: AsyncSession = Depends(get_db)):
    photo = Photo(**body.model_dump())
    db.add(photo)
    if body.album_id:
        album_result = await db.execute(select(Album).where(Album.id == body.album_id))
        album = album_result.scalar_one_or_none()
        if album:
            album.photo_count = (album.photo_count or 0) + 1
    await db.commit()
    await db.refresh(photo)
    return photo


@router.get("/{photo_id}")
async def get_photo(photo_id: str, db: AsyncSession = Depends(get_db)):
    result = await db.execute(select(Photo).where(Photo.id == photo_id))
    photo = result.scalar_one_or_none()
    if not photo:
        raise HTTPException(404, "Photo not found")
    return photo


@router.put("/{photo_id}")
async def update_photo(photo_id: str, body: PhotoUpdate, db: AsyncSession = Depends(get_db)):
    result = await db.execute(select(Photo).where(Photo.id == photo_id))
    photo = result.scalar_one_or_none()
    if not photo:
        raise HTTPException(404, "Photo not found")
    for key, val in body.model_dump(exclude_none=True).items():
        setattr(photo, key, val)
    await db.commit()
    await db.refresh(photo)
    return photo


@router.delete("/{photo_id}")
async def delete_photo(photo_id: str, db: AsyncSession = Depends(get_db)):
    result = await db.execute(select(Photo).where(Photo.id == photo_id))
    photo = result.scalar_one_or_none()
    if not photo:
        raise HTTPException(404, "Photo not found")
    if photo.album_id:
        album_result = await db.execute(select(Album).where(Album.id == photo.album_id))
        album = album_result.scalar_one_or_none()
        if album and album.photo_count and album.photo_count > 0:
            album.photo_count -= 1
    await db.delete(photo)
    await db.commit()
    return {"status": "deleted"}


@router.post("/{photo_id}/favorite")
async def toggle_favorite(photo_id: str, db: AsyncSession = Depends(get_db)):
    result = await db.execute(select(Photo).where(Photo.id == photo_id))
    photo = result.scalar_one_or_none()
    if not photo:
        raise HTTPException(404, "Photo not found")
    photo.is_favorite = not photo.is_favorite
    await db.commit()
    return {"id": photo.id, "is_favorite": photo.is_favorite}


@router.get("/albums", tags=["albums"])
async def list_albums(skip: int = Query(0, ge=0), limit: int = Query(100, ge=1, le=500), db: AsyncSession = Depends(get_db)):
    result = await db.execute(select(Album).offset(skip).limit(limit).order_by(Album.created_at.desc()))
    return result.scalars().all()


@router.post("/albums", tags=["albums"])
async def create_album(body: AlbumCreate, db: AsyncSession = Depends(get_db)):
    album = Album(**body.model_dump())
    db.add(album)
    await db.commit()
    await db.refresh(album)
    return album


@router.get("/albums/{album_id}", tags=["albums"])
async def get_album(album_id: str, db: AsyncSession = Depends(get_db)):
    result = await db.execute(select(Album).where(Album.id == album_id))
    album = result.scalar_one_or_none()
    if not album:
        raise HTTPException(404, "Album not found")
    return album


@router.put("/albums/{album_id}", tags=["albums"])
async def update_album(album_id: str, body: AlbumUpdate, db: AsyncSession = Depends(get_db)):
    result = await db.execute(select(Album).where(Album.id == album_id))
    album = result.scalar_one_or_none()
    if not album:
        raise HTTPException(404, "Album not found")
    for key, val in body.model_dump(exclude_none=True).items():
        setattr(album, key, val)
    await db.commit()
    await db.refresh(album)
    return album


@router.delete("/albums/{album_id}", tags=["albums"])
async def delete_album(album_id: str, db: AsyncSession = Depends(get_db)):
    result = await db.execute(select(Album).where(Album.id == album_id))
    album = result.scalar_one_or_none()
    if not album:
        raise HTTPException(404, "Album not found")
    await db.delete(album)
    await db.commit()
    return {"status": "deleted"}


@router.post("/albums/{album_id}/photos", tags=["albums"])
async def add_photo_to_album(album_id: str, photo_id: str, db: AsyncSession = Depends(get_db)):
    album_result = await db.execute(select(Album).where(Album.id == album_id))
    album = album_result.scalar_one_or_none()
    if not album:
        raise HTTPException(404, "Album not found")
    photo_result = await db.execute(select(Photo).where(Photo.id == photo_id))
    photo = photo_result.scalar_one_or_none()
    if not photo:
        raise HTTPException(404, "Photo not found")
    photo.album_id = album_id
    album.photo_count = (album.photo_count or 0) + 1
    await db.commit()
    return {"status": "added", "album_id": album_id, "photo_id": photo_id}
