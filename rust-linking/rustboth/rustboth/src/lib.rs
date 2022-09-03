pub struct RustBoth {}

impl RustBoth {
    pub fn new() -> Self {
        Self {}
    }
}

mod ffi {
    use super::*;

    #[no_mangle]
    pub fn rustboth_new() -> *mut RustBoth {
        Box::into_raw(Box::new(RustBoth::new()))
    }

    #[no_mangle]
    pub fn rustboth_delete(m: *mut RustBoth) {
        drop(unsafe { Box::from_raw(m) })
    }
}
