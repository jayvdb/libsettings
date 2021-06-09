extern crate bindgen;

use cmake;

use std::env;

use std::path::PathBuf;

use cmake::Config;

fn main() {
    // Builds the project in the root directory, installing it
    // into $OUT_DIR
    let mut dst = Config::new("..")
                        .define("SKIP_UNIT_TESTS", "ON")
                        .define("BUILD_SHARED_LIBS", "OFF")
                        .define("PYTHON", "python3")
                        .build();
    println!("cargo:rustc-link-search=native={}/lib", dst.display());

    dst = cmake::build(".");
    println!("cargo:rustc-link-search=native={}/lib", dst.display());

    println!("cargo:rustc-link-lib=static=sbp");
    println!("cargo:rustc-link-lib=static=settings");
    println!("cargo:rustc-link-lib=static=swiftnav");
    println!("cargo:rustc-link-lib=static=rustbindsettings");

    let bindings = bindgen::Builder::default()
        .header("./libsettings_wrapper.h")
        .clang_arg("-I../include")
        .clang_arg("-I../third_party/libswiftnav/include")
        .clang_arg("-I../third_party/libsbp/c/include")
        .generate()
        .expect("Unable to generate bindings");

    // Write out the generated bindings...
    let out_dir = env::var("OUT_DIR").unwrap();
    let out_dir = PathBuf::from(out_dir);

    bindings
        .write_to_file(out_dir.join("libsettings.rs"))
        .expect("Couldn't write bindings!");
}
