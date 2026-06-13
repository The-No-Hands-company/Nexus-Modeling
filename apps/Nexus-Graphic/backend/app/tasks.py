from app.celery_app import celery_app


@celery_app.task(bind=True, max_retries=3)
def generate_image_task(self, prompt: str, width: int, height: int, style: str, project_id: str):
    """AI image generation task — dispatches to ComfyUI / Diffusers worker."""
    try:
        from app.ai_worker import generate_image
        result = generate_image(prompt, width, height, style)
        return {"status": "completed", "url": result, "project_id": project_id, "prompt": prompt}
    except Exception as exc:
        raise self.retry(exc=exc, countdown=30)


@celery_app.task(bind=True, max_retries=3)
def remove_background_task(self, asset_id: str):
    try:
        from app.ai_worker import remove_background
        result = remove_background(asset_id)
        return {"status": "completed", "asset_id": asset_id, "result_url": result}
    except Exception as exc:
        raise self.retry(exc=exc, countdown=30)
