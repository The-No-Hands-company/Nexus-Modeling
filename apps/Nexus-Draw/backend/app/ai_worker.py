"""
AI worker — runs on GPU nodes.
For production: use ComfyUI API, Diffusers, or ONNX Runtime.
"""


def generate_diagram(prompt: str, width: int = 1024, height: int = 1024, style: str = "whiteboard") -> str:
    """
    Generate a diagram from a text prompt.
    In production, calls a diagram generation model or LLM-based layout engine.
    Returns the S3 URL of the generated diagram image.
    """
    # TODO: Integrate with diagram generation model
    raise NotImplementedError("AI diagram generation requires GPU worker node")


def remove_background(asset_id: str) -> str:
    """
    Remove background from an image asset for sticky notes / image elements.
    """
    # TODO: Integrate with rembg or ONNX Runtime Web
    raise NotImplementedError("Background removal requires rembg or ONNX model")
