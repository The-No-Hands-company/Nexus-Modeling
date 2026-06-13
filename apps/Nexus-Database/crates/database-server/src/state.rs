use database_core::{DatabaseConfig, DatabasePool};
use std::sync::Arc;
use tokio::sync::RwLock;
use std::collections::HashMap;

use crate::persistence::Persistence;

/// Shared application state for the database server.
#[derive(Clone)]
pub struct AppState {
    /// Map of managed databases by ID.
    pub databases: Arc<RwLock<HashMap<uuid::Uuid, DatabaseConfig>>>,
    /// Connection pools for managed databases (lazily created on first query).
    pub pools: Arc<RwLock<HashMap<uuid::Uuid, DatabasePool>>>,
    /// Optional control-plane persistence.
    pub persistence: Persistence,
    /// Server start time for uptime tracking.
    pub start_time: std::time::Instant,
    /// Nexus-Cloud system ID (set after successful registration).
    pub cloud_system_id: Option<String>,
}

impl AppState {
    /// Build state with an optional persistence layer.
    /// If persistence is active, pre-loads existing databases from the control-plane.
    pub async fn new(persistence: Persistence) -> Self {
        let dbs = persistence.load_all().await;
        let map: HashMap<uuid::Uuid, DatabaseConfig> =
            dbs.into_iter().map(|c| (c.id, c)).collect();

        if !map.is_empty() {
            tracing::info!("Loaded {} managed databases from control-plane", map.len());
        }

        Self {
            databases: Arc::new(RwLock::new(map)),
            pools: Arc::new(RwLock::new(HashMap::new())),
            persistence,
            start_time: std::time::Instant::now(),
            cloud_system_id: None,
        }
    }

    /// Return the number of managed databases.
    pub async fn database_count(&self) -> usize {
        self.databases.read().await.len()
    }

    /// Check if a database with the given name already exists.
    pub async fn name_exists(&self, name: &str) -> bool {
        let dbs = self.databases.read().await;
        dbs.values().any(|db| db.name.eq_ignore_ascii_case(name))
    }

    /// Get or lazily create a connection pool for the given database config.
    /// Returns an error if the pool cannot be established.
    pub async fn get_or_create_pool(
        &self,
        config: &DatabaseConfig,
    ) -> database_core::DatabaseResult<DatabasePool> {
        // Fast path: pool already exists
        {
            let pools = self.pools.read().await;
            if let Some(pool) = pools.get(&config.id) {
                return Ok(pool.clone());
            }
        }

        // Slow path: connect and insert
        let pool = DatabasePool::connect(config).await?;
        {
            let mut pools = self.pools.write().await;
            pools.insert(config.id, pool.clone());
        }
        Ok(pool)
    }

    /// Remove a pool from the cache and close it gracefully.
    pub async fn remove_pool(&self, id: uuid::Uuid) {
        if let Some(pool) = {
            let mut pools = self.pools.write().await;
            pools.remove(&id)
        } {
            pool.close().await;
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use database_core::DatabaseConfig;

    fn dummy_config(name: &str, engine: &str) -> DatabaseConfig {
        DatabaseConfig {
            id: uuid::Uuid::new_v4(),
            name: name.into(),
            engine: engine.into(),
            connection_string: format!("{engine}:///test"),
            max_connections: 10,
            auto_migrate: false,
            tags: Default::default(),
            created_at: chrono::Utc::now(),
            updated_at: chrono::Utc::now(),
        }
    }

    #[tokio::test]
    async fn new_app_state() {
        let persistence = Persistence::new(None).await;
        let state = AppState::new(persistence).await;
        assert_eq!(state.database_count().await, 0);
        assert!(!state.persistence.is_active());
    }

    #[tokio::test]
    async fn name_exists_detects_duplicate() {
        let persistence = Persistence::new(None).await;
        let state = AppState::new(persistence).await;

        let cfg = dummy_config("mydb", "postgres");
        {
            let mut dbs = state.databases.write().await;
            dbs.insert(cfg.id, cfg.clone());
        }

        assert!(state.name_exists("mydb").await);
        assert!(state.name_exists("MYDB").await); // case-insensitive
        assert!(!state.name_exists("otherdb").await);
    }

    #[tokio::test]
    async fn name_exists_empty_state() {
        let persistence = Persistence::new(None).await;
        let state = AppState::new(persistence).await;
        assert!(!state.name_exists("anything").await);
    }

    #[tokio::test]
    async fn database_count_reflects_insertions() {
        let persistence = Persistence::new(None).await;
        let state = AppState::new(persistence).await;

        {
            let mut dbs = state.databases.write().await;
            dbs.insert(uuid::Uuid::new_v4(), dummy_config("a", "postgres"));
            dbs.insert(uuid::Uuid::new_v4(), dummy_config("b", "sqlite"));
        }

        assert_eq!(state.database_count().await, 2);
    }

    #[tokio::test]
    async fn remove_pool_nonexistent_does_not_panic() {
        let persistence = Persistence::new(None).await;
        let state = AppState::new(persistence).await;
        state.remove_pool(uuid::Uuid::new_v4()).await; // should not panic
    }
}
