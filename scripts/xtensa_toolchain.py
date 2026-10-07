Import("env", "projenv")

from pathlib import Path


board_mcu = env.BoardConfig().get("build.mcu", "")
if board_mcu in ("esp32", "esp32s2", "esp32s3"):
    platform = env.PioPlatform()
    package_dir = platform.get_package_dir("toolchain-xtensa-esp-elf")
    if package_dir:
        package_path = Path(package_dir)
        compiler_dirs = (
            package_path / "bin",
            package_path / "xtensa-esp-elf" / "bin",
        )
        compiler_dir = next(
            (directory for directory in compiler_dirs
             if (directory / "xtensa-esp32-elf-g++").is_file() or
             (directory / "xtensa-esp32-elf-g++.exe").is_file()),
            None,
        )
        if compiler_dir is not None:
            # The bundled compiler names live in this nested directory, which
            # the platform's build PATH does not include.
            env.PrependENVPath("PATH", str(compiler_dir))
            projenv.PrependENVPath("PATH", str(compiler_dir))
