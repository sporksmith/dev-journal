fn main() {
    println!("My CLOCK_REALTIME value: {}", linux_raw_sys::general::CLOCK_REALTIME);
    println!("crates.io CLOCK_REALTIME value: {}", linux_raw_sys_crates_io::general::CLOCK_REALTIME);
    println!("rustix clock_gettime: {:?}", rustix::time::clock_gettime(rustix::thread::ClockId::Realtime));
}
