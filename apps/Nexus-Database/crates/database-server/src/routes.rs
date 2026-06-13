use axum::{extract::State, http::StatusCode, Json};
use database_core::{
    DatabaseConfig, DatabaseEngine, DatabaseError, DatabaseInfo, DatabaseStatus,
    HealthResponse, ProvisionDatabaseRequest, QueryRequest, QueryResponse,
};
use uuid::Uuid;

use crate::state::AppState;

/// GET /health
pub async fn health(State(state): State<AppState>) -> Json<HealthResponse> {
    Json(HealthResponse {
        status: "ok",
        version: env!("CARGO_PKG_VERSION"),
        managed_databases: state.database_count().await,
        uptime_seconds: state.start_time.elapsed().as_secs_f64(),
        persistence: state.persistence.is_active(),
        cloud_system_id: state.cloud_system_id.clone(),
    })
}

/// GET /databases
pub async fn list_databases(
    State(state): State<AppState>,
) -> Result<Json<Vec<DatabaseInfo>>, DatabaseError> {
    let dbs = state.databases.read().await;
    let infos: Vec<DatabaseInfo> = dbs
        .iter()
        .map(|(id, config)| DatabaseInfo {
            id: *id,
            name: config.name.clone(),
            engine: config.engine.clone(),
            status: DatabaseStatus::Healthy,
            pool_size: config.max_connections,
            created_at: config.created_at,
            tags: config.tags.clone(),
        })
        .collect();
    Ok(Json(infos))
}

/// POST /databases — provision a new database.
pub async fn provision_database(
    State(state): State<AppState>,
    Json(req): Json<ProvisionDatabaseRequest>,
) -> Result<(StatusCode, Json<DatabaseInfo>), DatabaseError> {
    if req.name.trim().is_empty() {
        return Err(DatabaseError::Config("name is required".into()));
    }

    if state.name_exists(&req.name).await {
        return Err(DatabaseError::Config(format!(
            "database '{}' already exists",
            req.name
        )));
    }

    let engine = match req.engine.to_lowercase().as_str() {
        "postgres" | "postgresql" => DatabaseEngine::Postgres,
        "sqlite" => DatabaseEngine::Sqlite,
        other => {
            return Err(DatabaseError::Config(format!(
                "unsupported engine: {}",
                other
            )))
        }
    };

    let now = chrono::Utc::now();
    let id = Uuid::new_v4();
    let config = DatabaseConfig {
        id,
        name: req.name.clone(),
        engine: engine.scheme().to_string(),
        connection_string: req.connection_string.clone().unwrap_or_else(|| match &engine {
            DatabaseEngine::Sqlite => "sqlite::memory:".into(),
            DatabaseEngine::Postgres => format!(
                "postgres://{}:{}@localhost:5432/{}",
                req.name, "changeme", req.name
            ),
        }),
        max_connections: req.max_connections.unwrap_or(10),
        auto_migrate: false,
        tags: req.tags.clone(),
        created_at: now,
        updated_at: now,
    };

    {
        let mut dbs = state.databases.write().await;
        dbs.insert(id, config.clone());
    }

    // Persist to control-plane (fire-and-forget — failure is logged, not fatal)
    state.persistence.insert(&config).await;

    let info = DatabaseInfo {
        id,
        name: req.name,
        engine: engine.scheme().to_string(),
        status: DatabaseStatus::Provisioning,
        pool_size: config.max_connections,
        created_at: now,
        tags: req.tags,
    };

    Ok((StatusCode::CREATED, Json(info)))
}

/// GET /databases/:id
pub async fn get_database(
    State(state): State<AppState>,
    axum::extract::Path(id): axum::extract::Path<Uuid>,
) -> Result<Json<DatabaseInfo>, DatabaseError> {
    let dbs = state.databases.read().await;
    let config = dbs
        .get(&id)
        .ok_or_else(|| DatabaseError::DatabaseNotFound(id.to_string()))?;

    Ok(Json(DatabaseInfo {
        id,
        name: config.name.clone(),
        engine: config.engine.clone(),
        status: DatabaseStatus::Healthy,
        pool_size: config.max_connections,
        created_at: config.created_at,
        tags: config.tags.clone(),
    }))
}

/// POST /databases/:id/query — execute a SQL query against a managed database.
pub async fn query_database(
    State(state): State<AppState>,
    axum::extract::Path(id): axum::extract::Path<Uuid>,
    Json(req): Json<QueryRequest>,
) -> Result<Json<QueryResponse>, DatabaseError> {
    let config = {
        let dbs = state.databases.read().await;
        dbs.get(&id)
            .cloned()
            .ok_or_else(|| DatabaseError::DatabaseNotFound(id.to_string()))?
    };

    let pool = state.get_or_create_pool(&config).await?;

    if req.readonly {
        pool.execute_query(&req.sql).await.map(Json)
    } else {
        let affected = pool.execute_write(&req.sql).await?;
        Ok(Json(QueryResponse {
            columns: vec!["rows_affected".into()],
            rows: vec![vec![serde_json::Value::Number(affected.into())]],
            row_count: 1,
            elapsed_ms: 0.0,
        }))
    }
}

/// DELETE /databases/:id
pub async fn delete_database(
    State(state): State<AppState>,
    axum::extract::Path(id): axum::extract::Path<Uuid>,
) -> Result<StatusCode, DatabaseError> {
    let mut dbs = state.databases.write().await;
    if dbs.remove(&id).is_none() {
        return Err(DatabaseError::DatabaseNotFound(id.to_string()));
    }

    // Remove pool and control-plane record (fire-and-forget)
    state.remove_pool(id).await;
    state.persistence.delete(id).await;

    Ok(StatusCode::NO_CONTENT)
}
