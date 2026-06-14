use std::collections::HashMap;
use std::sync::Mutex;
use wasm_bindgen::prelude::*;
use pqcrypto_traits::kem::{Ciphertext as _, PublicKey as KemPk, SecretKey as KemSk, SharedSecret as _};
use pqcrypto_traits::sign::{PublicKey as SignPk, SecretKey as SignSk, DetachedSignature};

// ── Core identity store (pure Rust, testable) ─────────────────────

pub struct Identity {
    pub did: String,
    kem_public: Vec<u8>,
    kem_secret: Vec<u8>,
    signing_public: Vec<u8>,
    signing_secret: Vec<u8>,
}

pub struct IdentityStore {
    store: Mutex<HashMap<u64, Identity>>,
    next: std::sync::atomic::AtomicU64,
}

impl IdentityStore {
    pub fn new() -> Self {
        Self { store: Mutex::new(HashMap::new()), next: std::sync::atomic::AtomicU64::new(1) }
    }

    pub fn generate(&self, name: &str) -> u64 {
        use pqcrypto_kyber::kyber1024;
        use pqcrypto_dilithium::dilithium5;

        let (kem_pk, kem_sk) = kyber1024::keypair();
        let (sig_pk, sig_sk) = dilithium5::keypair();
        let did = format!("did:phantom:{}", &hex::encode(blake3::hash(name.as_bytes()).as_bytes())[..16]);

        let id = Identity {
            did,
            kem_public: kem_pk.as_bytes().to_vec(),
            kem_secret: kem_sk.as_bytes().to_vec(),
            signing_public: sig_pk.as_bytes().to_vec(),
            signing_secret: sig_sk.as_bytes().to_vec(),
        };
        let h = self.next.fetch_add(1, std::sync::atomic::Ordering::SeqCst);
        self.store.lock().unwrap().insert(h, id);
        h
    }

    pub fn get_public_info(&self, handle: u64) -> Option<(String, Vec<u8>, Vec<u8>)> {
        self.store.lock().unwrap().get(&handle).map(|id| {
            (id.did.clone(), id.kem_public.clone(), id.signing_public.clone())
        })
    }

    pub fn sign(&self, handle: u64, message: &[u8]) -> Option<Vec<u8>> {
        use pqcrypto_dilithium::dilithium5;
        let store = self.store.lock().unwrap();
        let id = store.get(&handle)?;
        let sk = dilithium5::SecretKey::from_bytes(&id.signing_secret).ok()?;
        let sig = dilithium5::detached_sign(message, &sk);
        Some(sig.as_bytes().to_vec())
    }

    pub fn verify(&self, handle: u64, message: &[u8], signature: &[u8]) -> Option<bool> {
        use pqcrypto_dilithium::dilithium5;
        let store = self.store.lock().unwrap();
        let id = store.get(&handle)?;
        let pk = dilithium5::PublicKey::from_bytes(&id.signing_public).ok()?;
        let sig = DetachedSignature::from_bytes(signature).ok()?;
        Some(dilithium5::verify_detached_signature(&sig, message, &pk).is_ok())
    }

    pub fn encapsulate(&self, handle: u64) -> Option<(Vec<u8>, Vec<u8>)> {
        use pqcrypto_kyber::kyber1024;
        let store = self.store.lock().unwrap();
        let id = store.get(&handle)?;
        let pk = kyber1024::PublicKey::from_bytes(&id.kem_public).ok()?;
        let (ss, ct) = kyber1024::encapsulate(&pk);
        Some((ct.as_bytes().to_vec(), ss.as_bytes().to_vec()))
    }

    pub fn decapsulate(&self, handle: u64, ciphertext: &[u8]) -> Option<Vec<u8>> {
        use pqcrypto_kyber::kyber1024;
        use pqcrypto_traits::kem::Ciphertext;
        let store = self.store.lock().unwrap();
        let id = store.get(&handle)?;
        let ct = Ciphertext::from_bytes(ciphertext).ok()?;
        let sk = kyber1024::SecretKey::from_bytes(&id.kem_secret).ok()?;
        let ss = kyber1024::decapsulate(&ct, &sk);
        Some(ss.as_bytes().to_vec())
    }

    pub fn release(&self, handle: u64) {
        self.store.lock().unwrap().remove(&handle);
    }
}

// ── WASM bindings (thin wrapper) ───────────────────────────────────

static STORE: std::sync::LazyLock<IdentityStore> = std::sync::LazyLock::new(|| IdentityStore::new());

fn wasm_json(val: serde_json::Value) -> Result<JsValue, JsValue> {
    #[cfg(target_arch = "wasm32")]
    { serde_wasm_bindgen::to_value(&val).map_err(|e| JsValue::from_str(&e.to_string())) }
    #[cfg(not(target_arch = "wasm32"))]
    { Ok(JsValue::from_str(&val.to_string())) }
}

#[wasm_bindgen]
pub fn generate_identity(name: &str) -> Result<JsValue, JsValue> {
    let handle = STORE.generate(name);
    let (did, pk, sig_pk) = STORE.get_public_info(handle).unwrap();
    wasm_json(serde_json::json!({
        "handle": handle, "did": did,
        "publicKey": hex::encode(pk), "signingPublicKey": hex::encode(sig_pk),
    }))
}

#[wasm_bindgen]
pub fn sign(handle: u64, message_hex: &str) -> Result<JsValue, JsValue> {
    let msg = hex::decode(message_hex).map_err(|e| JsValue::from_str(&e.to_string()))?;
    let sig = STORE.sign(handle, &msg).ok_or_else(|| JsValue::from_str("signing failed"))?;
    wasm_json(serde_json::json!({ "signature": hex::encode(sig) }))
}

#[wasm_bindgen]
pub fn verify(handle: u64, message_hex: &str, signature_hex: &str) -> Result<JsValue, JsValue> {
    let msg = hex::decode(message_hex).map_err(|e| JsValue::from_str(&e.to_string()))?;
    let sig_bytes = hex::decode(signature_hex).map_err(|e| JsValue::from_str(&e.to_string()))?;
    let valid = STORE.verify(handle, &msg, &sig_bytes).unwrap_or(false);
    wasm_json(serde_json::json!({ "valid": valid }))
}

#[wasm_bindgen]
pub fn encapsulate(handle: u64) -> Result<JsValue, JsValue> {
    let (ct, ss) = STORE.encapsulate(handle).ok_or_else(|| JsValue::from_str("encapsulate failed"))?;
    wasm_json(serde_json::json!({ "ciphertext": hex::encode(ct), "sharedSecret": hex::encode(ss) }))
}

#[wasm_bindgen]
pub fn decapsulate(handle: u64, ciphertext_hex: &str) -> Result<JsValue, JsValue> {
    let ct = hex::decode(ciphertext_hex).map_err(|e| JsValue::from_str(&e.to_string()))?;
    let ss = STORE.decapsulate(handle, &ct).ok_or_else(|| JsValue::from_str("decapsulate failed"))?;
    wasm_json(serde_json::json!({ "sharedSecret": hex::encode(ss) }))
}

#[wasm_bindgen]
pub fn get_did(handle: u64) -> Result<JsValue, JsValue> {
    let (did, _, _) = STORE.get_public_info(handle).ok_or_else(|| JsValue::from_str("not found"))?;
    wasm_json(serde_json::json!({ "did": did }))
}

#[wasm_bindgen]
pub fn release(handle: u64) { STORE.release(handle); }

#[wasm_bindgen]
pub fn phantom_hash(data_hex: &str) -> Result<JsValue, JsValue> {
    let data = hex::decode(data_hex).map_err(|e| JsValue::from_str(&e.to_string()))?;
    let h = blake3::hash(&data);
    wasm_json(serde_json::json!({ "hash": hex::encode(h.as_bytes()) }))
}

#[wasm_bindgen]
pub fn version() -> String { "phantom-sdk-wasm/0.1.0".to_string() }

// ── Native tests ───────────────────────────────────────────────────

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_generate_and_sign() {
        let store = IdentityStore::new();
        let handle = store.generate("alice");
        let (did, _, _) = store.get_public_info(handle).unwrap();
        assert!(did.starts_with("did:phantom:"));

        let sig = store.sign(handle, b"hello phantom").unwrap();
        assert!(store.verify(handle, b"hello phantom", &sig).unwrap());
        assert!(!store.verify(handle, b"wrong message", &sig).unwrap());
        store.release(handle);
    }

    #[test]
    fn test_kem() {
        let store = IdentityStore::new();
        let h = store.generate("bob");
        let (ct, ss1) = store.encapsulate(h).unwrap();
        let ss2 = store.decapsulate(h, &ct).unwrap();
        assert_eq!(ss1, ss2);
        store.release(h);
    }

    #[test]
    fn test_multiple_identities() {
        let store = IdentityStore::new();
        let a = store.generate("alice");
        let b = store.generate("bob");
        assert_ne!(a, b);
        let sig_a = store.sign(a, b"msg").unwrap();
        assert!(store.verify(a, b"msg", &sig_a).unwrap());
        assert!(!store.verify(b, b"msg", &sig_a).unwrap());
        store.release(a);
        store.release(b);
    }
}
