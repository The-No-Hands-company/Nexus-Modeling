"""
AI worker — runs on GPU nodes.
For production: uses ComfyUI API, Diffusers, or ONNX Runtime.
This module provides the Python-side interface that Celery tasks call.
"""


def generate_design_layout(prompt: str, project_type: str = "web", width: int = 1440, height: int = 900) -> list:
    """
    Generate UI/UX design layout from a text prompt.
    In production, calls an LLM + layout generator or ComfyUI workflow.
    Returns a list of frame/layer element dicts.
    """
    raise NotImplementedError("AI design generation requires GPU worker node with LLM + layout model")


def remove_background(asset_id: str) -> str:
    """
    Remove background from an image asset.
    In production, uses rembg or ONNX Runtime with RMBG-1.4.
    """
    raise NotImplementedError("Background removal requires rembg or ONNX model")
