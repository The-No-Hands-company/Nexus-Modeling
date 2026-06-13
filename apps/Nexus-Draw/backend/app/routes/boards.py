from fastapi import APIRouter, Depends, HTTPException
from pydantic import BaseModel
from sqlalchemy import select, update
from sqlalchemy.ext.asyncio import AsyncSession
from app.models import Board, Element
from app.deps import get_db

router = APIRouter(prefix="/api/v1/draw", tags=["boards"])


class BoardCreate(BaseModel):
    name: str
    description: str = ""
    width: int = 1920
    height: int = 1080
    background: str = "#ffffff"
    is_public: bool = False


class ElementCreate(BaseModel):
    element_type: str = "shape"
    data: dict = {}
    style: dict = {}
    transform: dict = {}
    order: int = 0


class ElementUpdate(BaseModel):
    data: dict | None = None
    style: dict | None = None
    transform: dict | None = None
    order: int | None = None


class ReorderRequest(BaseModel):
    element_ids: list[str]


@router.get("/boards")
async def list_boards(db: AsyncSession = Depends(get_db)):
    result = await db.execute(select(Board).order_by(Board.updated_at.desc()))
    return result.scalars().all()


@router.post("/boards")
async def create_board(body: BoardCreate, db: AsyncSession = Depends(get_db)):
    board = Board(**body.model_dump())
    db.add(board)
    await db.commit()
    await db.refresh(board)
    return board


@router.get("/boards/{board_id}")
async def get_board(board_id: str, db: AsyncSession = Depends(get_db)):
    result = await db.execute(select(Board).where(Board.id == board_id))
    board = result.scalar_one_or_none()
    if not board:
        raise HTTPException(404, "Board not found")
    return board


@router.delete("/boards/{board_id}")
async def delete_board(board_id: str, db: AsyncSession = Depends(get_db)):
    result = await db.execute(select(Board).where(Board.id == board_id))
    board = result.scalar_one_or_none()
    if not board:
        raise HTTPException(404, "Board not found")
    await db.delete(board)
    await db.commit()
    return {"status": "deleted"}


@router.get("/boards/{board_id}/elements")
async def list_elements(board_id: str, db: AsyncSession = Depends(get_db)):
    result = await db.execute(select(Element).where(Element.board_id == board_id).order_by(Element.order))
    return result.scalars().all()


@router.post("/boards/{board_id}/elements")
async def create_element(board_id: str, body: ElementCreate, db: AsyncSession = Depends(get_db)):
    board = await db.get(Board, board_id)
    if not board:
        raise HTTPException(404, "Board not found")
    element = Element(board_id=board_id, **body.model_dump())
    db.add(element)
    await db.commit()
    await db.refresh(element)
    return element


@router.patch("/boards/{board_id}/elements/{elem_id}")
async def update_element(board_id: str, elem_id: str, body: ElementUpdate, db: AsyncSession = Depends(get_db)):
    result = await db.execute(
        select(Element).where(Element.id == elem_id, Element.board_id == board_id)
    )
    element = result.scalar_one_or_none()
    if not element:
        raise HTTPException(404, "Element not found")
    patch = {k: v for k, v in body.model_dump().items() if v is not None}
    for key, val in patch.items():
        setattr(element, key, val)
    await db.commit()
    await db.refresh(element)
    return element


@router.put("/boards/{board_id}/elements/reorder")
async def reorder_elements(board_id: str, body: ReorderRequest, db: AsyncSession = Depends(get_db)):
    for idx, elem_id in enumerate(body.element_ids):
        await db.execute(
            update(Element).where(Element.id == elem_id, Element.board_id == board_id).values(order=idx)
        )
    await db.commit()
    return {"status": "reordered"}
