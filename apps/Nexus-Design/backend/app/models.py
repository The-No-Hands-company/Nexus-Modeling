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
    project_type = Column(String(32), default="web")  # web, mobile, desktop, other
    width = Column(Integer, default=1440)
    height = Column(Integer, default=900)
    frames = Column(JSON, default=list)
    status = Column(String(32), default="draft")  # draft, in_progress, completed
    created_at = Column(DateTime, default=utcnow)
    updated_at = Column(DateTime, default=utcnow, onupdate=utcnow)

    frames_rel = relationship("Frame", back_populates="project", cascade="all, delete-orphan", order_by="Frame.order")


class Frame(Base):
    __tablename__ = "frames"

    id = Column(String(32), primary_key=True, default=new_id)
    project_id = Column(String(32), ForeignKey("projects.id"), nullable=False)
    name = Column(String(255), default="Frame")
    x = Column(Integer, default=0)
    y = Column(Integer, default=0)
    width = Column(Integer, default=1440)
    height = Column(Integer, default=900)
    background = Column(String(32), default="#ffffff")
    elements = Column(JSON, default=list)
    order = Column(Integer, default=0)
    created_at = Column(DateTime, default=utcnow)

    project = relationship("Project", back_populates="frames_rel")
    layers = relationship("Layer", back_populates="frame", cascade="all, delete-orphan", order_by="Layer.order")


class Layer(Base):
    __tablename__ = "layers"

    id = Column(String(32), primary_key=True, default=new_id)
    frame_id = Column(String(32), ForeignKey("frames.id"), nullable=False)
    name = Column(String(255), default="Layer")
    layer_type = Column(String(32), default="shape")  # shape, text, image, component
    data = Column(JSON, default=dict)
    visible = Column(Boolean, default=True)
    locked = Column(Boolean, default=False)
    opacity = Column(Float, default=1.0)
    order = Column(Integer, default=0)
    created_at = Column(DateTime, default=utcnow)

    frame = relationship("Frame", back_populates="layers")


class Asset(Base):
    __tablename__ = "assets"

    id = Column(String(32), primary_key=True, default=new_id)
    project_id = Column(String(32), ForeignKey("projects.id"), nullable=True)
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
    project_type = Column(String(32), default="web")  # web, mobile, desktop, other
    thumbnail_url = Column(String(512), default="")
    elements = Column(JSON, nullable=False)
    created_at = Column(DateTime, default=utcnow)
