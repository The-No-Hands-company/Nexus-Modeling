from app.celery_app import celery_app


@celery_app.task(bind=True, max_retries=3)
def generate_diagram_task(self, prompt: str, width: int, height: int, style: str, board_id: str):
    """AI diagram generation task."""
    try:
        from app.ai_worker import generate_diagram
        result = generate_diagram(prompt, width, height, style)
        return {"status": "completed", "url": result, "board_id": board_id, "prompt": prompt}
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
