from fastapi import APIRouter, Depends, HTTPException
from pydantic import BaseModel
from sqlalchemy import select
from sqlalchemy.ext.asyncio import AsyncSession
from typing import Optional
from app.models import Project, Frame, Layer, new_id
from app.deps import get_db

router = APIRouter(prefix="/api/v1/design", tags=["design"])


class ProjectCreate(BaseModel):
    name: str
    description: str = ""
    project_type: str = "web"
    width: int = 1440
    height: int = 900


class FrameCreate(BaseModel):
    name: str = "Frame"
    x: int = 0
    y: int = 0
    width: int = 1440
    height: int = 900
    background: str = "#ffffff"


class FrameUpdate(BaseModel):
    name: Optional[str] = None
    x: Optional[int] = None
    y: Optional[int] = None
    width: Optional[int] = None
    height: Optional[int] = None
    background: Optional[str] = None


class LayerCreate(BaseModel):
    name: str = "Layer"
    layer_type: str = "shape"
    data: dict = {}
    visible: bool = True
    locked: bool = False
    opacity: float = 1.0


@router.get("/projects")
async def list_projects(db: AsyncSession = Depends(get_db)):
    result = await db.execute(select(Project).order_by(Project.updated_at.desc()))
    return result.scalars().all()


@router.post("/projects")
async def create_project(body: ProjectCreate, db: AsyncSession = Depends(get_db)):
    project = Project(
        name=body.name,
        description=body.description,
        project_type=body.project_type,
        width=body.width,
        height=body.height,
    )
    db.add(project)
    await db.commit()
    await db.refresh(project)
    return project


@router.get("/projects/{project_id}")
async def get_project(project_id: str, db: AsyncSession = Depends(get_db)):
    result = await db.execute(select(Project).where(Project.id == project_id))
    project = result.scalar_one_or_none()
    if not project:
        raise HTTPException(404, "Project not found")
    return project


@router.delete("/projects/{project_id}")
async def delete_project(project_id: str, db: AsyncSession = Depends(get_db)):
    result = await db.execute(select(Project).where(Project.id == project_id))
    project = result.scalar_one_or_none()
    if not project:
        raise HTTPException(404, "Project not found")
    await db.delete(project)
    await db.commit()
    return {"ok": True}


@router.get("/projects/{project_id}/frames")
async def list_frames(project_id: str, db: AsyncSession = Depends(get_db)):
    result = await db.execute(
        select(Frame).where(Frame.project_id == project_id).order_by(Frame.order)
    )
    return result.scalars().all()


@router.post("/projects/{project_id}/frames")
async def create_frame(project_id: str, body: FrameCreate, db: AsyncSession = Depends(get_db)):
    project_result = await db.execute(select(Project).where(Project.id == project_id))
    project = project_result.scalar_one_or_none()
    if not project:
        raise HTTPException(404, "Project not found")
    result = await db.execute(select(Frame).where(Frame.project_id == project_id).order_by(Frame.order.desc()))
    last = result.scalar_one_or_none()
    order = (last.order + 1) if last else 0
    frame = Frame(
        project_id=project_id,
        name=body.name,
        x=body.x,
        y=body.y,
        width=body.width,
        height=body.height,
        background=body.background,
        order=order,
    )
    db.add(frame)
    await db.commit()
    await db.refresh(frame)
    return frame


@router.patch("/projects/{project_id}/frames/{frame_id}")
async def update_frame(project_id: str, frame_id: str, body: FrameUpdate, db: AsyncSession = Depends(get_db)):
    result = await db.execute(
        select(Frame).where(Frame.id == frame_id, Frame.project_id == project_id)
    )
    frame = result.scalar_one_or_none()
    if not frame:
        raise HTTPException(404, "Frame not found")
    update_data = body.model_dump(exclude_unset=True)
    for key, val in update_data.items():
        setattr(frame, key, val)
    await db.commit()
    await db.refresh(frame)
    return frame


@router.get("/projects/{project_id}/frames/{frame_id}/layers")
async def list_layers(project_id: str, frame_id: str, db: AsyncSession = Depends(get_db)):
    result = await db.execute(
        select(Frame).where(Frame.id == frame_id, Frame.project_id == project_id)
    )
    frame = result.scalar_one_or_none()
    if not frame:
        raise HTTPException(404, "Frame not found")
    layers_result = await db.execute(
        select(Layer).where(Layer.frame_id == frame_id).order_by(Layer.order)
    )
    return layers_result.scalars().all()


@router.post("/projects/{project_id}/frames/{frame_id}/layers")
async def create_layer(project_id: str, frame_id: str, body: LayerCreate, db: AsyncSession = Depends(get_db)):
    frame_result = await db.execute(
        select(Frame).where(Frame.id == frame_id, Frame.project_id == project_id)
    )
    frame = frame_result.scalar_one_or_none()
    if not frame:
        raise HTTPException(404, "Frame not found")
    result = await db.execute(
        select(Layer).where(Layer.frame_id == frame_id).order_by(Layer.order.desc())
    )
    last = result.scalar_one_or_none()
    order = (last.order + 1) if last else 0
    layer = Layer(
        frame_id=frame_id,
        name=body.name,
        layer_type=body.layer_type,
        data=body.data,
        visible=body.visible,
        locked=body.locked,
        opacity=body.opacity,
        order=order,
    )
    db.add(layer)
    await db.commit()
    await db.refresh(layer)
    return layer
