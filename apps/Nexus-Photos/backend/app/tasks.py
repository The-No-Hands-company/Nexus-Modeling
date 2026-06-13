from app.celery_app import celery_app


@celery_app.task(bind=True, max_retries=3)
def generate_image_task(self, prompt: str, width: int, height: int, style: str, project_id: str):
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


@celery_app.task(bind=True, max_retries=3)
def enhance_photo_task(self, photo_id: str, enhancement: str):
    try:
        from app.ai_worker import enhance_photo
        result = enhance_photo(photo_id, enhancement)
        return {"status": "completed", "photo_id": photo_id, "result_url": result}
    except Exception as exc:
        raise self.retry(exc=exc, countdown=30)


@celery_app.task(bind=True, max_retries=3)
def detect_objects_task(self, photo_id: str):
    try:
        from app.ai_worker import detect_objects
        result = detect_objects(photo_id)
        return {"status": "completed", "photo_id": photo_id, "objects": result}
    except Exception as exc:
        raise self.retry(exc=exc, countdown=30)
