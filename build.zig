const std = @import("std");

const example_sources = [_][]const u8{
    "examples/aurora.cpp",
    "examples/bubbles.cpp",
    "examples/deepsea.cpp",
    "examples/fractal.cpp",
    "examples/funky.cpp",
    "examples/lunar.cpp",
    "examples/mario.cpp",
    "examples/neon.cpp",
    "examples/polar.cpp",
    "examples/rainforest.cpp",
    "examples/raymarch.cpp",
    "examples/ripple.cpp",
    "examples/sdf_primitives.cpp",
    "examples/seascape.cpp",
    "examples/starfield.cpp",
    "examples/tunnelwisp.cpp",
    "examples/tzozen.cpp",
    "examples/voronoi.cpp",
    "examples/warp.cpp",
};

pub fn build(b: *std.Build) void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.option(
        std.builtin.OptimizeMode,
        "optimize",
        "Optimization mode (default: ReleaseFast)",
    ) orelse .ReleaseFast;

    const is_macos = target.result.os.tag == .macos;
    const use_metal = b.option(
        bool,
        "metal",
        "Build the Metal backend (enabled by default on macOS)",
    ) orelse is_macos;
    const use_openmp = b.option(
        bool,
        "openmp",
        "Enable OpenMP CPU rendering (default: true)",
    ) orelse true;
    const use_lto = b.option(
        bool,
        "lto",
        "Enable LTO (default: false on macOS, true elsewhere)",
    ) orelse !is_macos;

    if (use_metal and !is_macos) {
        std.debug.panic("-Dmetal=true is only supported for macOS targets", .{});
    }
    if (use_lto and is_macos) {
        std.debug.panic(
            "Zig LTO is unavailable on macOS because Zig's LLD cannot link Mach-O; use -Dlto=false",
            .{},
        );
    }

    const sumi_path = b.option(
        []const u8,
        "sumi-path",
        "Path to libsumi (default: ./libsumi)",
    ) orelse "libsumi";
    const sumi_include = b.pathJoin(&.{ sumi_path, "include" });

    const default_homebrew_prefix = if (target.result.cpu.arch == .aarch64)
        "/opt/homebrew"
    else
        "/usr/local";
    const homebrew_prefix = b.graph.environ_map.get("HOMEBREW_PREFIX") orelse default_homebrew_prefix;
    const libomp_prefix = b.option(
        []const u8,
        "libomp-prefix",
        "Path to the libomp prefix on macOS",
    ) orelse b.pathJoin(&.{ homebrew_prefix, "opt", "libomp" });

    const eshi_step = b.step("eshi", "Build and install only the main eshi executable");
    const examples_step = b.step("examples", "Build and install all example executables");

    const main_install = addEshiExecutable(b, .{
        .name = "eshi",
        .shader_source = "shader.cpp",
        .target = target,
        .optimize = optimize,
        .sumi_include = sumi_include,
        .libomp_prefix = libomp_prefix,
        .use_metal = use_metal,
        .use_openmp = use_openmp,
        .use_lto = use_lto,
    });
    eshi_step.dependOn(&main_install.step);
    b.getInstallStep().dependOn(&main_install.step);

    for (example_sources) |source| {
        const name = std.fs.path.stem(source);
        const install = addEshiExecutable(b, .{
            .name = name,
            .shader_source = source,
            .target = target,
            .optimize = optimize,
            .sumi_include = sumi_include,
            .libomp_prefix = libomp_prefix,
            .use_metal = use_metal,
            .use_openmp = use_openmp,
            .use_lto = use_lto,
        });
        examples_step.dependOn(&install.step);
        b.getInstallStep().dependOn(&install.step);

        const example_step = b.step(name, b.fmt("Build and install only the {s} example", .{name}));
        example_step.dependOn(&install.step);
    }
}

const ExecutableOptions = struct {
    name: []const u8,
    shader_source: []const u8,
    target: std.Build.ResolvedTarget,
    optimize: std.builtin.OptimizeMode,
    sumi_include: []const u8,
    libomp_prefix: []const u8,
    use_metal: bool,
    use_openmp: bool,
    use_lto: bool,
};

fn addEshiExecutable(b: *std.Build, options: ExecutableOptions) *std.Build.Step.InstallArtifact {
    const module = b.createModule(.{
        .target = options.target,
        .optimize = options.optimize,
        .link_libc = true,
        .link_libcpp = true,
    });

    module.addIncludePath(b.path("."));
    module.addIncludePath(pathFromOption(b, options.sumi_include));
    module.addCMacro("LINK_SHADER", "1");

    const cpp_flags: []const []const u8 = if (options.use_openmp)
        if (options.target.result.os.tag == .macos)
            &.{ "-std=c++11", "-Wall", "-Wextra", "-Wno-nullability-completeness", "-Xpreprocessor", "-fopenmp" }
        else
            &.{ "-std=c++11", "-Wall", "-Wextra", "-fopenmp" }
    else
        &.{ "-std=c++11", "-Wall", "-Wextra" };

    module.addCSourceFiles(.{
        .files = &.{ "main.cpp", options.shader_source },
        .flags = cpp_flags,
        .language = .cpp,
    });

    if (options.use_metal) {
        module.addCMacro("USE_METAL", "1");
        module.addCSourceFile(.{
            .file = b.path("renderer_metal.mm"),
            .flags = &.{ "-std=c++11", "-Wall", "-Wextra", "-Wno-nullability-completeness", "-fobjc-arc" },
            .language = .objective_cpp,
        });
        module.linkFramework("Metal", .{});
        module.linkFramework("Foundation", .{});
        module.linkFramework("QuartzCore", .{});
    }

    if (options.use_openmp) {
        if (options.target.result.os.tag == .macos) {
            const include_path = b.pathJoin(&.{ options.libomp_prefix, "include" });
            const library_path = b.pathJoin(&.{ options.libomp_prefix, "lib" });
            module.addSystemIncludePath(pathFromOption(b, include_path));
            module.addLibraryPath(pathFromOption(b, library_path));
            module.linkSystemLibrary("omp", .{ .use_pkg_config = .no });
        } else {
            module.linkSystemLibrary("gomp", .{ .use_pkg_config = .no });
        }
    }

    const packages = [_][]const u8{
        // SDL2_ttf's pkg-config metadata already includes SDL2. Listing both
        // makes Zig emit the SDL dylib twice, which modern macOS dyld rejects.
        "SDL2_ttf",
        "libavcodec",
        "libavformat",
        "libavutil",
        "libswscale",
    };
    for (packages) |package| {
        module.linkSystemLibrary(package, .{ .use_pkg_config = .force });
    }

    const exe = b.addExecutable(.{
        .name = options.name,
        .root_module = module,
        .use_llvm = true,
        .use_lld = if (options.use_lto) true else null,
    });
    exe.lto = if (options.use_lto) .full else .none;

    return b.addInstallArtifact(exe, .{});
}

fn pathFromOption(b: *std.Build, path: []const u8) std.Build.LazyPath {
    return if (std.fs.path.isAbsolute(path))
        .{ .cwd_relative = path }
    else
        b.path(path);
}
