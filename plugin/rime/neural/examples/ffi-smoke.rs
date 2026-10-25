// SPDX-License-Identifier: MIT
use std::ffi::CString;
use rime_chinese_ime_lm::{cime_best, cime_free, cime_load};

fn main() {
    let path = CString::new(std::env::args().nth(1).expect("model path")).unwrap();
    let context = CString::new("").unwrap();
    unsafe {
        let handle = cime_load(path.as_ptr());
        assert!(!handle.is_null(), "the released model must load through the C ABI");
        for (wrong, correct) in [
            ("今天天气真号", "今天天气真好"),
            ("我想去北京旅又", "我想去北京旅游"),
        ] {
            let texts = [CString::new(wrong).unwrap(), CString::new(correct).unwrap()];
            let pointers = [texts[0].as_ptr(), texts[1].as_ptr()];
            let selected = cime_best(handle, context.as_ptr(), pointers.as_ptr(), [1, 1].as_ptr(), 2);
            println!("{wrong} / {correct}: selected={selected}");
            assert_eq!(selected, 1, "a scored candidate must retain its original index");
            assert_eq!(cime_best(handle, context.as_ptr(), pointers.as_ptr(), [3, 1].as_ptr(), 2), 0,
                       "a full-key dictionary leader must remain untouched");
            assert_eq!(cime_best(handle, context.as_ptr(), pointers.as_ptr(), [0, 1].as_ptr(), 2), 0,
                       "a prefix leader must remain untouched");
        }
        cime_free(handle);
    }
}
