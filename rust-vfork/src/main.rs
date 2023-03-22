//! Example program safely using vfork to fork and exec a child process.
//!
//! vfork cannot be safely called from Rust code, since the compiler currently doesn't support
//! functions that can return twice. See https://github.com/rust-lang/libc/issues/1596.
//!
//! We work around it by using inline assembly to encapsulate the
//! second return.

use std::arch::asm;
use std::ffi::{CStr, CString};

fn main() {
    // We're going to exec "/bin/echo" in a child process.
    let pathname: *mut i8 = CString::new("/bin/echo").unwrap().into_raw();

    // argv is a null-terminated array of string pointers.
    // The first string should be the binary name again.
    let argv: [*mut i8; 3] = [
        CString::new("/bin/echo").unwrap().into_raw(),
        CString::new("hello from forked child").unwrap().into_raw(),
        std::ptr::null_mut(),
    ];
    // envp is also a null-terminated array of string pointers.
    // We're not passing along any environment variables.
    let envp: [*mut i8; 1] = [std::ptr::null_mut()];

    // We'll store the parent's return value of vfork here.
    let mut parent_vfork_rv: i32;

    // If the child's call to execve fails, we'll store its return value here.
    let mut child_execve_rv: i64 = 0;

    // We use inline assembly to vfork + exec.
    //
    // The child process execs without ever reaching the end of the asm block, so from the Rust
    // compiler's perspective there is no second return.
    unsafe {
        asm!(
        // Call libc `vfork`, which returns twice.
        "call vfork",

        // Parent process gets non-zero return value. Jump to the end of the asm block to "return"
        // it.
        "cmp rax, 0",
        "jne 1f",

        // We're in the child process. Here we're just directly calling execve, though
        // if there were some extra setup we needed to do in between (such as closing or dup'ing
        // file descriptors), we could do that here first or call out to a normal function to do
        // so.
        "mov rdi, r12",
        "mov rsi, r13",
        "mov rdx, r14",
        "call execve",

        // execve doesn't return if it succeeds, but could return
        // if e.g. an executable file wasn't found at the the specified path.
        //
        // We save the execve result to memory, which is still shared with the parent,
        // and then execute an undefined instruction to crash.
        "mov [r15], rax", // *r15 is child_execve_rv
        "ud2",

        // "out" label
        "1:",

        // We explicitly assign input parameters to callee-saved registers, so that the call to
        // vfork won't clobber them.
        in("r12") pathname,
        in("r13") argv.as_slice().as_ptr(),
        in("r14") envp.as_slice().as_ptr(),
        in("r15") &mut child_execve_rv as *mut _,

        // In the parent process, the return value of "vfork" will be in eax. Save it to
        // `parent_vfork_rv`.
        out("eax") parent_vfork_rv,

        // The call to the vfork libc function clobbers caller-saved registers.
        clobber_abi("C"),
        );
    };

    // save errno, which may have been set by vfork or execve.
    let errno_after_asm_block = unsafe { *libc::__errno_location() };

    // `vfork` guarantees that we'll only get here after the child has exec'd or exited, so we can
    // safely "reconstitute" and our string arguments to `execve`.
    let pathname = unsafe { CString::from_raw(pathname) };
    drop(pathname);
    let argv: Vec<_> = argv
        .into_iter()
        .filter(|p| !p.is_null())
        .map(|s| unsafe { CString::from_raw(s) })
        .collect();
    drop(argv);
    let envp: Vec<_> = envp
        .into_iter()
        .filter(|p| !p.is_null())
        .map(|s| unsafe { CString::from_raw(s) })
        .collect();
    drop(envp);

    if parent_vfork_rv < 0 {
        panic!(
            "vfork failed: {}",
            unsafe { CStr::from_ptr(libc::strerror(errno_after_asm_block)) }
                .to_str()
                .unwrap()
        );
    }

    if child_execve_rv != 0 {
        panic!(
            "execve failed: {}",
            unsafe { CStr::from_ptr(libc::strerror(errno_after_asm_block)) }
                .to_str()
                .unwrap()
        );
    }

    // Wait for child to exit
    let mut status = 0;
    let rv = unsafe { libc::waitpid(parent_vfork_rv, &mut status, 0) };

    // Validate that waitpid itself succeeded.
    assert_eq!(rv, parent_vfork_rv);

    // Validate that child exited normally.
    assert!(libc::WIFEXITED(status));

    // Validate child exit code.
    assert_eq!(libc::WEXITSTATUS(status), 0);
}
