use rustboth::RustBoth;

pub struct RustStatic {
    rustboth: RustBoth,
}

impl RustStatic {
    pub fn new() -> Self {
        Self { rustboth: RustBoth::new() }
    }
}

mod ffi {
    use super::*;

    #[no_mangle]
    pub fn ruststatic_new() -> *mut RustStatic {
        Box::into_raw(Box::new(RustStatic::new()))
    }

    #[no_mangle]
    pub fn ruststatic_delete(m: *mut RustStatic) {
        drop(unsafe { Box::from_raw(m) })
    }
}
