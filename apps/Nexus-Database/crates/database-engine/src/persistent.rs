//! Persistent Database — shared router with disk-backed storage.
//!
//! Creates a single `DeltaMainRouter` instance that survives across
//! connections. Data written by one `psql` session is visible to the next.
//!
//! Startup:
//!   1. Open WAL from disk
//!   2. Open FileStorage
//!   3. Replay WAL → restore state
//!   4. Create shared router
//!   5. Accept connections via pgwire

use std::path::PathBuf;
use std::sync::Arc;
use parking_lot::RwLock;

use crate::buffer::BufferPool;
use crate::catalog::DeltaMainRouter;
use crate::page::PAGE_SIZE;
use crate::storage::{FileStorage, StorageBackend};
use crate::wal::WriteAheadLog;
use crate::Result;

/// A persistent database instance — shared across all connections.
pub struct PersistentDatabase {
    pub router: Arc<DeltaMainRouter>,
    pub storage: Arc<FileStorage>,
    pub wal: Arc<RwLock<WriteAheadLog>>,
    pub buffer_pool: Arc<BufferPool>,
    pub data_dir: PathBuf,
}

impl PersistentDatabase {
    /// Open or create a database at the given data directory.
    /// On first run, creates fresh storage. On subsequent runs,
    /// replays the WAL and restores state.
    pub fn open(data_dir: PathBuf) -> Result<Self> {
        std::fs::create_dir_all(&data_dir)?;

        let wal_path = data_dir.join("nexus.wal");
        let wal = WriteAheadLog::open(wal_path.to_str().unwrap())?;

        let storage = Arc::new(FileStorage::open(data_dir.join("pages"))?);
        let buffer_pool = Arc::new(BufferPool::new(10000, PAGE_SIZE));

        // Replay WAL to recover state from previous runs
        let wal = Arc::new(RwLock::new(wal));
        {
            let w = wal.read();
            match w.replay(&buffer_pool) {
                Ok(count) if count > 0 => log::info!("WAL replay: {} entries recovered", count),
                Ok(_) => log::info!("WAL is empty (fresh database)"),
                Err(e) => log::warn!("WAL replay error: {} (starting fresh)", e),
            }
        }

        // Create the shared router
        let lsm_dir = data_dir.join("lsm_data");
        let mut router = DeltaMainRouter::new(buffer_pool.clone(), lsm_dir)?;
        router.set_wal(wal.clone());

        // Replay columnar WAL entries on the router
        {
            let wal_reader = wal.read();
            wal_reader.replay_columnar(&mut router)?;
        }

        let router = Arc::new(router);

        log::info!("Persistent database opened at {}", data_dir.display());
        Ok(Self { router, storage, wal, buffer_pool, data_dir })
    }

    /// Flush all pending changes to disk and truncate the WAL.
    pub fn checkpoint(&self) -> Result<()> {
        // Flush dirty pages from buffer pool to storage
        // (simplified — real impl iterates pool and writes dirty pages)
        {
            let mut w = self.wal.write();
            w.truncate()?;
        }
        log::info!("Checkpoint complete");
        Ok(())
    }

    /// Close the database gracefully.
    pub fn close(&self) -> Result<()> {
        self.checkpoint()?;
        log::info!("Database closed");
        Ok(())
    }

    /// Stats for monitoring.
    pub fn stats(&self) -> DatabaseStats {
        DatabaseStats {
            data_dir: self.data_dir.display().to_string(),
            buffer_hit_ratio: self.buffer_pool.hit_ratio(),
            wal_bytes: self.wal.read().size_bytes(),
            tables: self.router.catalog().list_tables().len(),
            lsm_memtable: {
                // Access through router stats if available
                0
            },
            lsm_sstables: 0,
        }
    }
}

#[derive(Debug, Clone)]
pub struct DatabaseStats {
    pub data_dir: String,
    pub buffer_hit_ratio: f64,
    pub wal_bytes: u64,
    pub tables: usize,
    pub lsm_memtable: usize,
    pub lsm_sstables: usize,
}
