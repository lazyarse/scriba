//! # harper-ffi
//!
//! A panic-safe plain-C ABI wrapper around [`harper_core`], the grammar
//! checking engine behind Harper.
//!
//! The C++ host application (Scriba) owns spelling correction via Hunspell, so
//! the `SpellCheck` rule is **disabled** in the engine created by
//! [`harper_init`]. Everything else in the curated English lint group stays
//! enabled.
//!
//! Offsets reported through the ABI are **byte offsets** into the UTF-8 input
//! buffer passed to [`harper_lint`].

use std::ptr;

use harper_core::linting::{LintGroup, Linter, Suggestion};
use harper_core::spell::FstDictionary;
use harper_core::{Dialect, Document};

/// Opaque handle to a lint engine (an English Markdown [`LintGroup`]).
///
/// Never dereference from C++; allocate and free only through the FFI
/// functions.
#[repr(C)]
pub struct HarperEngine {
    _opaque: [u8; 0],
}

/// Opaque handle to an owned list of lint issues.
///
/// The list (and the message buffers it owns) remains valid until
/// [`harper_free_issues`] is called.
#[repr(C)]
pub struct HarperIssueList {
    _opaque: [u8; 0],
}

/// A single lint issue, ready to hand across the ABI.
///
/// `start`/`len` are byte offsets into the original UTF-8 input. Each
/// suggestion is a `(kind, text)` pair: kind 0 = replace the issue span with
/// `text`, 1 = remove the issue span, 2 = insert `text` after the issue span.
/// All suggestion kinds apply to the issue's own `start`/`len` span.
struct HarperIssue {
    start: usize,
    len: usize,
    message: Vec<u8>,
    suggestions: Vec<(u8, Vec<u8>)>,
}

struct HarperIssueListInner {
    issues: Vec<HarperIssue>,
}

/// Build a `byte_offset[char_index]` lookup table for `text` so character
/// spans returned by harper-core can be converted to byte offsets.
fn byte_offsets(text: &str) -> Vec<usize> {
    let mut offsets = Vec::with_capacity(text.chars().count() + 1);
    let mut byte = 0;
    for c in text.chars() {
        offsets.push(byte);
        byte += c.len_utf8();
    }
    offsets.push(byte);
    offsets
}

/// Map a `harper_init` dialect code to a [`Dialect`]. Unknown codes fall back
/// to American, matching the historic default.
fn dialect_from_code(code: u8) -> Dialect {
    match code {
        1 => Dialect::British,
        2 => Dialect::Australian,
        3 => Dialect::Indian,
        4 => Dialect::Canadian,
        _ => Dialect::American,
    }
}

/// Create a lint engine for English Markdown in the given dialect
/// (`dialect_code`: 0 American, 1 British, 2 Australian, 3 Indian, 4
/// Canadian; anything else falls back to American).
///
/// The `SpellCheck` rule is disabled: the host application handles spelling
/// with Hunspell. Returns a null pointer on failure (e.g. an internal panic).
#[no_mangle]
pub extern "C" fn harper_init(dialect_code: u8) -> *mut HarperEngine {
    let result = std::panic::catch_unwind(std::panic::AssertUnwindSafe(|| {
        let mut group = LintGroup::new_curated(
            FstDictionary::curated(),
            dialect_from_code(dialect_code),
        );
        group.config.set_rule_enabled("SpellCheck", false);
        Box::into_raw(Box::new(group)) as *mut HarperEngine
    }));

    result.unwrap_or(ptr::null_mut())
}

/// Lint the given UTF-8 text (not necessarily NUL-terminated) with `engine`.
///
/// Returns an owned list of issues whose `start`/`len` are byte offsets into
/// the input. Returns a null pointer on any failure (null arguments, invalid
/// UTF-8, or an internal panic). The returned list must be released with
/// [`harper_free_issues`].
///
/// # Safety
///
/// `engine` must be a live pointer from [`harper_init`], and `text_ptr` must
/// point to `text_len` readable bytes. The engine may not be used
/// concurrently.
#[no_mangle]
pub unsafe extern "C" fn harper_lint(
    engine: *mut HarperEngine,
    text_ptr: *const u8,
    text_len: usize,
) -> *mut HarperIssueList {
    let result = std::panic::catch_unwind(std::panic::AssertUnwindSafe(|| {
        if engine.is_null() || text_ptr.is_null() {
            return ptr::null_mut();
        }

        let bytes = std::slice::from_raw_parts(text_ptr, text_len);
        let Ok(text) = std::str::from_utf8(bytes) else {
            return ptr::null_mut();
        };

        let group = &mut *(engine as *mut LintGroup);
        let document = Document::new_markdown_default_curated(text);
        let lints = group.lint(&document);

        let offsets = byte_offsets(text);
        let max_char = offsets.len().saturating_sub(1);

        let issues = lints
            .into_iter()
            .map(|lint| {
                let start_ch = lint.span.start.min(max_char);
                let end_ch = lint.span.end.min(max_char);
                let start = offsets[start_ch];
                let end = offsets[end_ch];
                let suggestions = lint
                    .suggestions
                    .into_iter()
                    .map(|s| match s {
                        Suggestion::ReplaceWith(chars) => {
                            (0u8, chars.into_iter().collect::<String>().into_bytes())
                        }
                        Suggestion::Remove => (1u8, Vec::new()),
                        Suggestion::InsertAfter(chars) => {
                            (2u8, chars.into_iter().collect::<String>().into_bytes())
                        }
                    })
                    .collect();
                HarperIssue {
                    start,
                    len: end.saturating_sub(start),
                    message: lint.message.into_bytes(),
                    suggestions,
                }
            })
            .collect();

        let list = Box::new(HarperIssueListInner { issues });
        Box::into_raw(list) as *mut HarperIssueList
    }));

    result.unwrap_or(ptr::null_mut())
}

/// Number of issues in `list`. Returns 0 for a null list.
#[no_mangle]
pub extern "C" fn harper_issues_len(list: *const HarperIssueList) -> usize {
    if list.is_null() {
        return 0;
    }
    // SAFETY: `list` must be a live pointer from `harper_lint`.
    let inner = unsafe { &*(list as *const HarperIssueListInner) };
    inner.issues.len()
}

/// Byte offset of issue `i` into the original UTF-8 input.
///
/// Returns 0 if `list` is null or `i` is out of range.
#[no_mangle]
pub extern "C" fn harper_issue_start(list: *const HarperIssueList, i: usize) -> usize {
    if list.is_null() {
        return 0;
    }
    let inner = unsafe { &*(list as *const HarperIssueListInner) };
    inner.issues.get(i).map_or(0, |issue| issue.start)
}

/// Length in bytes of issue `i`. Returns 0 if `list` is null or `i` is out of
/// range.
#[no_mangle]
pub extern "C" fn harper_issue_len(list: *const HarperIssueList, i: usize) -> usize {
    if list.is_null() {
        return 0;
    }
    let inner = unsafe { &*(list as *const HarperIssueListInner) };
    inner.issues.get(i).map_or(0, |issue| issue.len)
}

/// UTF-8 message of issue `i` (guaranteed valid UTF-8). `out_len` receives the
/// byte length if non-null. The returned pointer is valid until the list is
/// freed. Returns null (and `*out_len = 0`) when `list` is null or `i` is out
/// of range.
#[no_mangle]
pub extern "C" fn harper_issue_message(
    list: *const HarperIssueList,
    i: usize,
    out_len: *mut usize,
) -> *const u8 {
    if list.is_null() {
        unsafe { ptr::write(out_len, 0) };
        return ptr::null();
    }
    let inner = unsafe { &*(list as *const HarperIssueListInner) };
    match inner.issues.get(i) {
        Some(issue) => {
            unsafe { ptr::write(out_len, issue.message.len()) };
            issue.message.as_ptr()
        }
        None => {
            unsafe { ptr::write(out_len, 0) };
            ptr::null()
        }
    }
}

/// Number of suggestions attached to issue `i`. Returns 0 for a null list or
/// an out-of-range index.
#[no_mangle]
pub extern "C" fn harper_issue_suggestion_count(list: *const HarperIssueList, i: usize) -> usize {
    if list.is_null() {
        return 0;
    }
    let inner = unsafe { &*(list as *const HarperIssueListInner) };
    inner
        .issues
        .get(i)
        .map_or(0, |issue| issue.suggestions.len())
}

/// Replacement text of suggestion `j` for issue `i`, as UTF-8.
///
/// `out_kind` receives the suggestion kind (0 = replace the issue span with
/// the text, 1 = remove the issue span, 2 = insert the text after the issue
/// span) and `out_len` the byte length. The returned pointer is valid until
/// the list is freed. Returns null (and zeroed-out params) when the list is
/// null or the indices are out of range.
#[no_mangle]
pub extern "C" fn harper_issue_suggestion(
    list: *const HarperIssueList,
    i: usize,
    j: usize,
    out_kind: *mut u8,
    out_len: *mut usize,
) -> *const u8 {
    if list.is_null() {
        unsafe {
            ptr::write(out_kind, 0);
            ptr::write(out_len, 0);
        }
        return ptr::null();
    }
    let inner = unsafe { &*(list as *const HarperIssueListInner) };
    match inner.issues.get(i).and_then(|issue| issue.suggestions.get(j)) {
        Some((kind, text)) => {
            unsafe {
                ptr::write(out_kind, *kind);
                ptr::write(out_len, text.len());
            }
            text.as_ptr()
        }
        None => {
            unsafe {
                ptr::write(out_kind, 0);
                ptr::write(out_len, 0);
            }
            ptr::null()
        }
    }
}

/// Release an issue list returned by [`harper_lint`]. Null pointers are
/// ignored.
#[no_mangle]
pub extern "C" fn harper_free_issues(list: *mut HarperIssueList) {
    if !list.is_null() {
        // SAFETY: `list` must be a live pointer from `harper_lint`.
        drop(unsafe { Box::from_raw(list as *mut HarperIssueListInner) });
    }
}

/// Release an engine created by [`harper_init`]. Null pointers are ignored.
#[no_mangle]
pub extern "C" fn harper_free(engine: *mut HarperEngine) {
    if !engine.is_null() {
        // SAFETY: `engine` must be a live pointer from `harper_init`.
        drop(unsafe { Box::from_raw(engine as *mut LintGroup) });
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    /// Lint `text` through the public FFI surface and drain the results,
    /// freeing every handle along the way.
    fn lint_all(text: &str) -> Vec<(usize, usize, String, Vec<(u8, String)>)> {
        let engine = harper_init(0);
        assert!(!engine.is_null(), "harper_init failed");

        let list = unsafe { harper_lint(engine, text.as_ptr(), text.len()) };
        assert!(!list.is_null(), "harper_lint failed");

        let count = harper_issues_len(list);
        let mut issues = Vec::with_capacity(count);
        for i in 0..count {
            let mut out_len = 0usize;
            let msg_ptr = harper_issue_message(list, i, &mut out_len);
            let msg = if msg_ptr.is_null() {
                String::new()
            } else {
                let bytes = unsafe { std::slice::from_raw_parts(msg_ptr, out_len) };
                String::from_utf8_lossy(bytes).into_owned()
            };
            let mut suggestions = Vec::new();
            let suggestion_count = harper_issue_suggestion_count(list, i);
            for j in 0..suggestion_count {
                let mut kind = 0u8;
                let mut out_len = 0usize;
                let text_ptr = harper_issue_suggestion(list, i, j, &mut kind, &mut out_len);
                let text = if text_ptr.is_null() {
                    String::new()
                } else {
                    let bytes = unsafe { std::slice::from_raw_parts(text_ptr, out_len) };
                    String::from_utf8_lossy(bytes).into_owned()
                };
                suggestions.push((kind, text));
            }
            issues.push((harper_issue_start(list, i), harper_issue_len(list, i), msg, suggestions));
        }

        harper_free_issues(list);
        harper_free(engine);
        issues
    }

    #[test]
    fn grammar_violation_is_detected() {
        let issues = lint_all("I has a cat.");
        assert!(
            !issues.is_empty(),
            "expected at least one grammar issue for 'I has a cat.'"
        );
        assert!(
            issues.iter().any(|(_, _, msg, _)| !msg.is_empty()),
            "expected at least one issue with a non-empty message, got {:?}",
            issues
        );
    }

    #[test]
    fn suggestions_are_exposed() {
        let issues = lint_all("I has a cat.");
        assert!(
            issues.iter().any(|(_, _, _, suggestions)| suggestions
                .iter()
                .any(|(kind, text)| *kind == 0 && !text.is_empty())),
            "expected a replace-with suggestion for 'I has a cat.', got {:?}",
            issues
        );
    }

    #[test]
    fn dialect_codes_map_to_expected_dialects() {
        use harper_core::Dialect;
        assert_eq!(dialect_from_code(0), Dialect::American);
        assert_eq!(dialect_from_code(1), Dialect::British);
        assert_eq!(dialect_from_code(2), Dialect::Australian);
        assert_eq!(dialect_from_code(3), Dialect::Indian);
        assert_eq!(dialect_from_code(4), Dialect::Canadian);
        assert_eq!(dialect_from_code(255), Dialect::American);
    }

    #[test]
    fn spelling_is_not_flagged() {
        let issues = lint_all("This is helo wrking text.");
        assert!(
            issues.iter().all(|(_, _, msg, _)| !msg.contains("Did you mean")),
            "spell-check style messages must be absent since SpellCheck is disabled, got {:?}",
            issues
        );
    }

    #[test]
    fn offsets_are_valid_byte_offsets() {
        let text = "I has a cat.";
        let issues = lint_all(text);
        for (start, len, _, _) in &issues {
            assert!(start + len <= text.len(), "span out of bounds: {start}..{}", start + len);
        }
    }

    #[test]
    fn byte_offsets_are_correct_for_unicode() {
        let text = "Héllo wörld.";
        let offsets = byte_offsets(text);
        assert_eq!(offsets.len(), text.chars().count() + 1);
        assert_eq!(&text[offsets[0]..offsets[1]], "H");
        assert_eq!(&text[offsets[1]..offsets[2]], "é");
        assert_eq!(offsets[2], 3);
    }

    #[test]
    fn null_pointers_are_handled_gracefully() {
        assert!(!harper_init(0).is_null());
        assert!(unsafe { harper_lint(ptr::null_mut(), ptr::null(), 0) }.is_null());
        assert_eq!(harper_issues_len(ptr::null()), 0);
        assert_eq!(harper_issue_start(ptr::null(), 0), 0);
        assert_eq!(harper_issue_len(ptr::null(), 0), 0);
        let mut len = 123usize;
        assert!(harper_issue_message(ptr::null(), 0, &mut len).is_null());
        assert_eq!(len, 0);
        assert_eq!(harper_issue_suggestion_count(ptr::null(), 0), 0);
        let mut kind = 42u8;
        let mut len = 123usize;
        assert!(harper_issue_suggestion(ptr::null(), 0, 0, &mut kind, &mut len).is_null());
        assert_eq!(kind, 0);
        assert_eq!(len, 0);
        harper_free_issues(ptr::null_mut());
        harper_free(ptr::null_mut());
    }
}
