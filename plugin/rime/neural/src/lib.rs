// SPDX-License-Identifier: MIT
//! The only unsafe code is the C ABI: the Rime adapter owns these NUL-terminated
//! strings and keeps them alive until the synchronous call returns.
use chinese_ime_lm::{CandidateFacts, Reranker, SentenceModel};
use std::ffi::{c_char, CStr};
use std::sync::Arc;

#[no_mangle]
pub unsafe extern "C" fn cime_load(path: *const c_char) -> *mut Reranker {
    if path.is_null() {
        return std::ptr::null_mut();
    }
    let Ok(path) = CStr::from_ptr(path).to_str() else {
        return std::ptr::null_mut();
    };
    let Ok(bytes) = std::fs::read(path) else {
        return std::ptr::null_mut();
    };
    match SentenceModel::load(&bytes) {
        Ok(model) => Box::into_raw(Box::new(Reranker::new(Arc::new(model)))),
        Err(_) => std::ptr::null_mut(),
    }
}

#[no_mangle]
pub unsafe extern "C" fn cime_free(handle: *mut Reranker) {
    if !handle.is_null() {
        drop(Box::from_raw(handle));
    }
}

/// Score real decoder paths for beam search. The caller owns `scores[count]`;
/// values are mean log probabilities, so different word splits remain comparable.
#[no_mangle]
pub unsafe extern "C" fn cime_scores(
    handle: *mut Reranker,
    context: *const c_char,
    texts: *const *const c_char,
    count: usize,
    scores: *mut f32,
) -> i32 {
    if handle.is_null() || context.is_null() || texts.is_null() || scores.is_null() {
        return -1;
    }
    let Ok(context) = CStr::from_ptr(context).to_str() else { return -1 };
    let mut candidates = Vec::with_capacity(count);
    for &pointer in std::slice::from_raw_parts(texts, count) {
        if pointer.is_null() { return -1; }
        let Ok(text) = CStr::from_ptr(pointer).to_str() else { return -1 };
        candidates.push(text);
    }
    let values = (*handle).log_probabilities(context, &candidates);
    std::ptr::copy_nonoverlapping(values.as_ptr(), scores, count);
    0
}

/// Return the original index to promote, zero to keep order, or -1 for invalid
/// FFI input. Bit 0 means full-key coverage; bit 1 means a trusted dictionary hit.
#[no_mangle]
pub unsafe extern "C" fn cime_best(
    handle: *mut Reranker,
    context: *const c_char,
    texts: *const *const c_char,
    facts: *const u8,
    count: usize,
) -> i32 {
    if handle.is_null() || context.is_null() || texts.is_null() || facts.is_null() {
        return -1;
    }
    let Ok(context) = CStr::from_ptr(context).to_str() else {
        return -1;
    };
    let pointers = std::slice::from_raw_parts(texts, count);
    let facts = std::slice::from_raw_parts(facts, count);
    let mut candidates = Vec::with_capacity(count);
    for &pointer in pointers {
        if pointer.is_null() {
            return -1;
        }
        let Ok(text) = CStr::from_ptr(pointer).to_str() else {
            return -1;
        };
        candidates.push(text);
    }
    (*handle)
        .best_where(context, &candidates, |index| CandidateFacts {
            answers_key: facts[index] & 1 != 0,
            trusted_dictionary_hit: facts[index] & 2 != 0,
        })
        .unwrap_or(0) as i32
}
