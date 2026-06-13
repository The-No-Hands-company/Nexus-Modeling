"""
AI worker — runs on GPU nodes.
For production: use ComfyUI API, Diffusers, or ONNX Runtime.
This module provides the Python-side interface that Celery tasks call.
"""


def generate_image(prompt: str, width: int = 1024, height: int = 1024, style: str = "realistic") -> str:
    raise NotImplementedError("AI generation requires GPU worker node with ComfyUI or Diffusers")


def remove_background(asset_id: str) -> str:
    raise NotImplementedError("Background removal requires rembg or ONNX model")


def enhance_photo(photo_id: str, enhancement: str = "auto") -> str:
    raise NotImplementedError("Photo enhancement requires GPU worker node with AI upscaling/denoising")


def detect_objects(photo_id: str) -> list[dict]:
    raise NotImplementedError("Object detection requires GPU worker node with YOLO or DETR model")
