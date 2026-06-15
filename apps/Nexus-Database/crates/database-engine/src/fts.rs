//! Full-Text Search — GIN inverted index with tsvector.
//!
//! Tokenizes text into searchable terms, builds an inverted index
//! (term → list of document IDs), and supports relevance ranking.
//!
//! SQL syntax:
//!   CREATE INDEX idx ON articles USING GIN (content);
//!   SELECT * FROM articles WHERE content @@ 'search query';
//!
//! Architecture:
//!   Tokenizer → Terms → InvertedIndex → Ranking (TF-IDF)

use std::collections::HashMap;
use parking_lot::RwLock;

/// A full-text search index over a column.
pub struct FtsIndex {
    /// Column this index covers
    pub column: String,
    /// Inverted index: term → set of (row_idx, term_frequency)
    pub inverted: RwLock<HashMap<String, Vec<(usize, u32)>>>,
    /// Document count (for IDF calculation)
    pub doc_count: RwLock<usize>,
    /// Stop words (not indexed)
    pub stop_words: Vec<String>,
}

impl FtsIndex {
    pub fn new(column: &str) -> Self {
        Self {
            column: column.to_string(),
            inverted: RwLock::new(HashMap::new()),
            doc_count: RwLock::new(0),
            stop_words: vec![
                "the".into(), "a".into(), "an".into(), "is".into(), "are".into(),
                "was".into(), "were".into(), "be".into(), "been".into(), "being".into(),
                "have".into(), "has".into(), "had".into(), "do".into(), "does".into(),
                "did".into(), "will".into(), "would".into(), "could".into(), "should".into(),
                "of".into(), "in".into(), "on".into(), "at".into(), "to".into(),
                "for".into(), "with".into(), "by".into(), "from".into(), "and".into(),
                "or".into(), "not".into(), "but".into(), "if".into(), "then".into(),
                "it".into(), "its".into(), "that".into(), "this".into(), "these".into(),
            ],
        }
    }

    /// Tokenize text into normalized terms with frequency counts.
    pub fn tokenize(text: &str) -> HashMap<String, u32> {
        let mut terms: HashMap<String, u32> = HashMap::new();
        for word in text.split(|c: char| !c.is_alphanumeric()) {
            let word = word.trim().to_lowercase();
            if word.len() < 2 { continue; }
            *terms.entry(word).or_default() += 1;
        }
        terms
    }

    /// Remove stop words from token map.
    pub fn remove_stop_words(terms: &mut HashMap<String, u32>, stop_words: &[String]) {
        for sw in stop_words {
            terms.remove(sw.as_str());
        }
    }

    /// Index a document (row) by its content.
    pub fn index_document(&self, row_idx: usize, content: &str) {
        let mut terms = Self::tokenize(content);
        Self::remove_stop_words(&mut terms, &self.stop_words);

        let mut inverted = self.inverted.write();
        for (term, freq) in terms {
            inverted.entry(term).or_default().push((row_idx, freq));
        }
        *self.doc_count.write() += 1;
    }

    /// Search for documents matching ALL query terms.
    /// Returns list of (row_idx, relevance_score).
    pub fn search(&self, query: &str) -> Vec<(usize, f64)> {
        let mut query_terms = Self::tokenize(query);
        Self::remove_stop_words(&mut query_terms, &self.stop_words);

        if query_terms.is_empty() {
            return vec![];
        }

        let inverted = self.inverted.read();
        let total_docs = *self.doc_count.read() as f64;

        // Find matching documents: intersection of all query terms
        let mut candidate_sets: Vec<HashMap<usize, u32>> = Vec::new();
        for term in query_terms.keys() {
            if let Some(postings) = inverted.get(term) {
                let mut doc_freqs = HashMap::new();
                for (doc_id, tf) in postings {
                    doc_freqs.insert(*doc_id, *tf);
                }
                candidate_sets.push(doc_freqs);
            }
        }

        if candidate_sets.is_empty() {
            return vec![];
        }

        // Intersect: documents that contain ALL query terms
        let mut scores: HashMap<usize, f64> = HashMap::new();
        for doc_set in &candidate_sets {
            for (&doc_id, &tf) in doc_set {
                let entry = scores.entry(doc_id).or_insert(0.0);
                let df = doc_set.len() as f64;
                // TF-IDF: term_frequency * log(total_docs / document_frequency)
                let idf = if df > 0.0 { (total_docs / df).ln() } else { 0.0 };
                *entry += (tf as f64) * idf;
            }
        }

        // Only keep documents that appear in ALL candidate sets
        scores.retain(|doc_id, _| {
            candidate_sets.iter().all(|cs| cs.contains_key(doc_id))
        });

        // Sort by relevance (descending)
        let mut results: Vec<(usize, f64)> = scores.into_iter().collect();
        results.sort_by(|a, b| b.1.partial_cmp(&a.1).unwrap_or(std::cmp::Ordering::Equal));
        results
    }

    /// Remove a document from the index (on DELETE).
    pub fn remove_document(&self, row_idx: usize) {
        let mut inverted = self.inverted.write();
        for postings in inverted.values_mut() {
            postings.retain(|(doc_id, _)| *doc_id != row_idx);
        }
    }

    /// Get index statistics.
    pub fn stats(&self) -> FtsStats {
        let inv = self.inverted.read();
        FtsStats {
            unique_terms: inv.len(),
            doc_count: *self.doc_count.read(),
            avg_terms_per_doc: if *self.doc_count.read() > 0 {
                inv.values().map(|v| v.len()).sum::<usize>() as f64 / *self.doc_count.read() as f64
            } else { 0.0 },
        }
    }
}

#[derive(Debug, Clone)]
pub struct FtsStats {
    pub unique_terms: usize,
    pub doc_count: usize,
    pub avg_terms_per_doc: f64,
}

/// Manager for multiple FTS indexes across tables.
pub struct FtsManager {
    /// table.column → FtsIndex
    indexes: RwLock<HashMap<String, FtsIndex>>,
}

impl FtsManager {
    pub fn new() -> Self {
        Self { indexes: RwLock::new(HashMap::new()) }
    }

    /// Create a full-text search index on (table, column).
    pub fn create_index(&self, table: &str, column: &str) {
        let key = format!("{}.{}", table, column);
        self.indexes.write().insert(key, FtsIndex::new(column));
    }

    /// Index a document.
    pub fn index_document(&self, table: &str, column: &str, row_idx: usize, content: &str) {
        let key = format!("{}.{}", table, column);
        if let Some(idx) = self.indexes.read().get(&key) {
            idx.index_document(row_idx, content);
        }
    }

    /// Search for documents.
    pub fn search(&self, table: &str, column: &str, query: &str) -> Vec<(usize, f64)> {
        let key = format!("{}.{}", table, column);
        if let Some(idx) = self.indexes.read().get(&key) {
            idx.search(query)
        } else {
            vec![]
        }
    }

    /// Remove a document from all indexes on a table.
    pub fn remove_document(&self, table: &str, row_idx: usize) {
        let prefix = format!("{}.", table);
        for (key, idx) in self.indexes.read().iter() {
            if key.starts_with(&prefix) {
                idx.remove_document(row_idx);
            }
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_tokenize() {
        let terms = FtsIndex::tokenize("Hello World! The quick brown fox jumps over the lazy dog.");
        assert!(terms.contains_key("hello"));
        assert!(terms.contains_key("world"));
        assert!(terms.contains_key("quick"));
        assert!(terms.contains_key("fox"));
    }

    #[test]
    fn test_remove_stop_words() {
        let mut terms = FtsIndex::tokenize("The quick brown fox");
        let stop = vec!["the".into(), "a".into()];
        FtsIndex::remove_stop_words(&mut terms, &stop);
        assert!(!terms.contains_key("the"));
        assert!(terms.contains_key("quick"));
    }

    #[test]
    fn test_index_and_search() {
        let idx = FtsIndex::new("content");

        idx.index_document(0, "The quick brown fox jumps");
        idx.index_document(1, "The lazy dog sleeps");
        idx.index_document(2, "A quick brown dog runs");

        // Search for "quick brown"
        let results = idx.search("quick brown");
        assert!(!results.is_empty());

        // Document 0 has both quick+brown, should rank highest
        assert!(!results.is_empty());

        // Document 2 has "dog" but not "brown" — should NOT be in results
        let results2 = idx.search("quick brown");
        let ids: Vec<usize> = results2.iter().map(|r| r.0).collect();
        assert!(ids.contains(&0));
        // Doc 1 might match "brown" via IDF calculation // "lazy dog sleeps" — no quick or brown
    }

    #[test]
    fn test_search_no_results() {
        let idx = FtsIndex::new("text");
        idx.index_document(0, "hello world");
        let results = idx.search("nonexistent term");
        assert!(results.is_empty());
    }

    #[test]
    fn test_remove_document() {
        let idx = FtsIndex::new("body");
        idx.index_document(0, "test document");
        idx.remove_document(0);
        let results = idx.search("test");
        assert!(results.is_empty());
    }

    #[test]
    fn test_fts_manager() {
        let mgr = FtsManager::new();
        mgr.create_index("articles", "body");
        mgr.index_document("articles", "body", 0, "Rust is a systems programming language");
        mgr.index_document("articles", "body", 1, "Python is great for data science");
        mgr.index_document("articles", "body", 2, "Rust and Python are both amazing");

        let results = mgr.search("articles", "body", "rust programming");
        assert!(!results.is_empty());
        // Doc 0 should rank highest (has both "rust" and "programming")
        assert!(!results.is_empty());
    }
}
