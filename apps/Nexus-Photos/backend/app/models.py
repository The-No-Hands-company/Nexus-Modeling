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


class Album(Base):
    __tablename__ = "albums"

    id = Column(String(32), primary_key=True, default=new_id)
    name = Column(String(255), nullable=False)
    description = Column(Text, default="")
    cover_url = Column(String(512), default="")
    photo_count = Column(Integer, default=0)
    created_at = Column(DateTime, default=utcnow)
    updated_at = Column(DateTime, default=utcnow, onupdate=utcnow)

    photos = relationship("Photo", back_populates="album", cascade="all, delete-orphan")


class Photo(Base):
    __tablename__ = "photos"

    id = Column(String(32), primary_key=True, default=new_id)
    album_id = Column(String(32), ForeignKey("albums.id"), nullable=True)
    title = Column(String(255), default="")
    description = Column(Text, default="")
    url = Column(String(512), nullable=False)
    thumbnail_url = Column(String(512), default="")
    width = Column(Integer, default=0)
    height = Column(Integer, default=0)
    file_size = Column(Integer, default=0)
    content_type = Column(String(128), default="image/jpeg")
    tags = Column(Text, default="")
    is_favorite = Column(Boolean, default=False)
    exif_data = Column(JSON, default=dict)
    created_at = Column(DateTime, default=utcnow)

    album = relationship("Album", back_populates="photos")


class Asset(Base):
    __tablename__ = "assets"

    id = Column(String(32), primary_key=True, default=new_id)
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
    preset = Column(JSON, default=dict)
    created_at = Column(DateTime, default=utcnow)
