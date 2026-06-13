from app.celery_app import celery_app


@celery_app.task(bind=True, max_retries=3)
def generate_design_task(self, prompt: str, project_type: str, width: int, height: int, project_id: str):
    """AI design layout generation task."""
    try:
        from app.ai_worker import generate_design_layout
        result = generate_design_layout(prompt, project_type, width, height)
        return {"status": "completed", "elements": result, "project_id": project_id, "prompt": prompt}
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
