import uuid
from datetime import datetime, timezone
from sqlalchemy import Column, String, Integer, Float, Text, Boolean, DateTime, ForeignKey, JSON
from sqlalchemy.orm import DeclarativeBase, relationship


class Base(DeclarativeBase):
    pass


def utcnow() -> datetime:
    return datetime.now(timezone.utc)


def new_id() -> str:
    return uuid.uuid4().hex


class Project(Base):
    __tablename__ = "projects"

    id = Column(String(32), primary_key=True, default=new_id)
    name = Column(String(255), nullable=False)
    description = Column(Text, default="")
    project_type = Column(String(32), default="vector")  # vector, raster, mixed
    width = Column(Integer, default=1920)
    height = Column(Integer, default=1080)
    thumbnail_url = Column(String(512), default="")
    status = Column(String(32), default="draft")  # draft, in_progress, completed
    created_at = Column(DateTime, default=utcnow)
    updated_at = Column(DateTime, default=utcnow, onupdate=utcnow)

    layers = relationship("Layer", back_populates="project", cascade="all, delete-orphan", order_by="Layer.order")


class Layer(Base):
    __tablename__ = "layers"

    id = Column(String(32), primary_key=True, default=new_id)
    project_id = Column(String(32), ForeignKey("projects.id"), nullable=False)
    name = Column(String(255), default="Layer")
    layer_type = Column(String(32), default="shape")  # shape, text, image, group, adjustment
    visible = Column(Boolean, default=True)
    locked = Column(Boolean, default=False)
    opacity = Column(Float, default=1.0)
    blend_mode = Column(String(32), default="normal")
    order = Column(Integer, default=0)
    transform = Column(JSON, default=dict)  # {a,b,c,d,e,f} matrix
    data = Column(JSON, default=dict)  # shape data, path data, text content, etc
    created_at = Column(DateTime, default=utcnow)
    updated_at = Column(DateTime, default=utcnow, onupdate=utcnow)

    project = relationship("Project", back_populates="layers")


class Asset(Base):
    __tablename__ = "assets"

    id = Column(String(32), primary_key=True, default=new_id)
    project_id = Column(String(32), ForeignKey("projects.id"), nullable=True)
    filename = Column(String(255), nullable=False)
    content_type = Column(String(128), default="application/octet-stream")
    size = Column(Integer, default=0)
    width = Column(Integer, default=0)
    height = Column(Integer, default=0)
    storage_key = Column(String(512), nullable=False)
    storage_backend = Column(String(32), default="s3")  # s3, local, r2
    created_at = Column(DateTime, default=utcnow)


class Template(Base):
    __tablename__ = "templates"

    id = Column(String(32), primary_key=True, default=new_id)
    name = Column(String(255), nullable=False)
    category = Column(String(64), default="general")
    description = Column(Text, default="")
    thumbnail_url = Column(String(512), default="")
    data = Column(JSON, nullable=False)  # Full layer/design JSON
    width = Column(Integer, default=1920)
    height = Column(Integer, default=1080)
    usage_count = Column(Integer, default=0)
    created_at = Column(DateTime, default=utcnow)
