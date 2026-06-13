use database_core::{DatabaseConfig, ManagedDatabaseRow};
use sqlx::postgres::PgPoolOptions;
use sqlx::PgPool;
use tracing::{info, warn};

/// Control-plane persistence layer.
///
/// Wraps an optional Postgres pool. When `None` the server runs with
/// in-memory state only (no persistence across restarts).
#[derive(Clone)]
pub struct Persistence {
    pool: Option<PgPool>,
}

impl Persistence {
    /// Create a new persistence handle. Connects to `url` if provided.
    pub async fn new(url: Option<&str>) -> Self {
        match url {
            Some(u) => match PgPoolOptions::new()
                .max_connections(5)
                .connect(u)
                .await
            {
                Ok(pool) => {
                    info!("Connected to control-plane Postgres");
                    let p = Self { pool: Some(pool) };
                    if let Err(e) = p.migrate().await {
                        warn!("Control-plane migration failed: {e}");
                    }
                    p
                }
                Err(e) => {
                    warn!("Cannot connect to control-plane Postgres ({e}); running in-memory only");
                    Self { pool: None }
                }
            },
            None => {
                info!("No control-plane URL configured; running in-memory only");
                Self { pool: None }
            }
        }
    }

    /// Whether persistence is active.
    pub fn is_active(&self) -> bool {
        self.pool.is_some()
    }

    /// Create the `managed_databases` table if it does not exist.
    async fn migrate(&self) -> Result<(), sqlx::Error> {
        let pool = self.pool.as_ref().expect("migrate called without pool");
        sqlx::query(
            r#"
            CREATE TABLE IF NOT EXISTS managed_databases (
                id          UUID PRIMARY KEY,
                name        TEXT NOT NULL UNIQUE,
                engine      TEXT NOT NULL,
                connection_string TEXT NOT NULL,
                max_connections INTEGER NOT NULL DEFAULT 10,
                auto_migrate BOOLEAN NOT NULL DEFAULT FALSE,
                tags        JSONB NOT NULL DEFAULT '{}',
                created_at  TIMESTAMPTZ NOT NULL DEFAULT NOW(),
                updated_at  TIMESTAMPTZ NOT NULL DEFAULT NOW()
            )
            "#,
        )
        .execute(pool)
        .await?;
        info!("Control-plane schema migrated");
        Ok(())
    }

    /// Load all managed databases from the control-plane.
    pub async fn load_all(&self) -> Vec<DatabaseConfig> {
        let pool = match &self.pool {
            Some(p) => p,
            None => return vec![],
        };

        match sqlx::query_as::<_, ManagedDatabaseRow>(
            "SELECT * FROM managed_databases ORDER BY created_at",
        )
        .fetch_all(pool)
        .await
        {
            Ok(rows) => rows.into_iter().map(DatabaseConfig::from).collect(),
            Err(e) => {
                warn!("Failed to load managed databases from control-plane: {e}");
                vec![]
            }
        }
    }

    /// Insert a new managed database into the control-plane.
    pub async fn insert(&self, config: &DatabaseConfig) {
        let pool = match &self.pool {
            Some(p) => p,
            None => return,
        };

        let row = ManagedDatabaseRow::from(config.clone());
        if let Err(e) = sqlx::query(
            r#"
            INSERT INTO managed_databases
                (id, name, engine, connection_string, max_connections,
                 auto_migrate, tags, created_at, updated_at)
            VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9)
            "#,
        )
        .bind(row.id)
        .bind(&row.name)
        .bind(&row.engine)
        .bind(&row.connection_string)
        .bind(row.max_connections)
        .bind(row.auto_migrate)
        .bind(&row.tags)
        .bind(row.created_at)
        .bind(row.updated_at)
        .execute(pool)
        .await
        {
            warn!("Failed to persist database '{}': {e}", config.name);
        }
    }

    /// Delete a managed database from the control-plane.
    pub async fn delete(&self, id: uuid::Uuid) {
        let pool = match &self.pool {
            Some(p) => p,
            None => return,
        };

        if let Err(e) = sqlx::query("DELETE FROM managed_databases WHERE id = $1")
            .bind(id)
            .execute(pool)
            .await
        {
            warn!("Failed to delete database {id} from control-plane: {e}");
        }
    }
}
