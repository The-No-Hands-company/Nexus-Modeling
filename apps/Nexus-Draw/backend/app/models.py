import uuid
from datetime import datetime, timezone
from sqlalchemy import Column, String, Integer, Text, Boolean, DateTime, ForeignKey, JSON
from sqlalchemy.orm import DeclarativeBase, relationship


class Base(DeclarativeBase):
    pass


def utcnow() -> datetime:
    return datetime.now(timezone.utc)


def new_id() -> str:
    return uuid.uuid4().hex


class Board(Base):
    __tablename__ = "boards"

    id = Column(String(32), primary_key=True, default=new_id)
    name = Column(String(255), nullable=False)
    description = Column(Text, default="")
    width = Column(Integer, default=1920)
    height = Column(Integer, default=1080)
    background = Column(String(32), default="#ffffff")
    is_public = Column(Boolean, default=False)
    created_at = Column(DateTime, default=utcnow)
    updated_at = Column(DateTime, default=utcnow, onupdate=utcnow)

    elements = relationship("Element", back_populates="board", cascade="all, delete-orphan", order_by="Element.order")


class Element(Base):
    __tablename__ = "elements"

    id = Column(String(32), primary_key=True, default=new_id)
    board_id = Column(String(32), ForeignKey("boards.id"), nullable=False)
    element_type = Column(String(32), nullable=False)  # shape, path, text, image, arrow, sticky
    data = Column(JSON, default=dict)
    style = Column(JSON, default=dict)  # fill, stroke, strokeWidth, opacity, fontFamily, fontSize
    transform = Column(JSON, default=dict)  # {a,b,c,d,e,f}
    order = Column(Integer, default=0)
    created_at = Column(DateTime, default=utcnow)

    board = relationship("Board", back_populates="elements")


class Asset(Base):
    __tablename__ = "assets"

    id = Column(String(32), primary_key=True, default=new_id)
    board_id = Column(String(32), ForeignKey("boards.id"), nullable=True)
    filename = Column(String(255), nullable=False)
    content_type = Column(String(128), default="application/octet-stream")
    size = Column(Integer, default=0)
    storage_key = Column(String(512), nullable=False)
    created_at = Column(DateTime, default=utcnow)


class Template(Base):
    __tablename__ = "templates"

    id = Column(String(32), primary_key=True, default=new_id)
    name = Column(String(255), nullable=False)
    description = Column(Text, default="")
    thumbnail_url = Column(String(512), default="")
    elements = Column(JSON, nullable=False)
    created_at = Column(DateTime, default=utcnow)
