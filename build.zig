const std = @import("std");

const example_sources = [_][]const u8{
    "examples/aurora.cpp",
    "examples/bubbles.cpp",
    "examples/deepsea.cpp",
    "examples/fractal.cpp",
    "examples/funky.cpp",
    "examples/harlequin.zig",
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
    const use_gl = b.option(
        bool,
        "gl",
        "Build the Larimar OpenGL backend, Kantei grade Paper (default: true)",
    ) orelse true;

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

    // Larimar: the Phase 0 engine core plus its hosts. Built independently of
    // the shadertoy gallery above so neither path can break the other.
    const larimar_install = addLarimarExecutable(b, .{
        .name = "pong",
        // ripple is linked in so the Ink tier has a compiled-in entry point for
        // the same source the GPU tiers transpile at runtime.
        .game_sources = &.{ "examples/pong/pong.cpp", "examples/ripple.cpp" },
        .target = target,
        .optimize = optimize,
        .sumi_include = sumi_include,
        .libomp_prefix = libomp_prefix,
        .use_openmp = use_openmp,
        .use_metal = use_metal,
        .use_gl = use_gl,
        .use_lto = use_lto,
    });
    const larimar_step = b.step("larimar", "Build the Larimar core and the SDL host (pong)");
    larimar_step.dependOn(&larimar_install.step);
    b.getInstallStep().dependOn(&larimar_install.step);

    // Core tests link only the core: no SDL, no FFmpeg, no libsumi. If this
    // target ever needs one of them, the OS-oblivious boundary has been broken.
    const test_module = b.createModule(.{
        .target = target,
        .optimize = optimize,
        .link_libc = true,
        .link_libcpp = true,
    });
    test_module.addIncludePath(b.path("core/include"));
    test_module.addCSourceFiles(.{
        .files = &.{
            "core/src/world.cpp",
            "core/src/render/registry.cpp",
            "core/src/render/transpile.cpp",
            "core/src/render/ink.cpp",
            "core/tests/test_core.cpp",
        },
        .flags = &.{ "-std=c++11", "-Wall", "-Wextra" },
        .language = .cpp,
    });
    const core_tests = b.addExecutable(.{
        .name = "eshi-core-tests",
        .root_module = test_module,
        .use_llvm = true,
    });
    core_tests.lto = .none;

    const run_tests = b.addRunArtifact(core_tests);
    const test_step = b.step("test", "Run the Larimar core tests");
    test_step.dependOn(&run_tests.step);

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

    const zig_shader = std.mem.eql(u8, std.fs.path.extension(options.shader_source), ".zig");
    module.addCSourceFiles(.{
        .files = if (zig_shader) &.{"main.cpp"} else &.{ "main.cpp", options.shader_source },
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
    if (zig_shader) {
        const shader_module = b.createModule(.{
            .root_source_file = b.path(options.shader_source),
            .target = options.target,
            .optimize = options.optimize,
        });
        const shader_object = b.addObject(.{
            .name = b.fmt("{s}-shader", .{options.name}),
            .root_module = shader_module,
        });
        exe.root_module.addObject(shader_object);
    }
    exe.lto = if (options.use_lto) .full else .none;

    return b.addInstallArtifact(exe, .{});
}

const LarimarOptions = struct {
    name: []const u8,
    game_sources: []const []const u8,
    target: std.Build.ResolvedTarget,
    optimize: std.builtin.OptimizeMode,
    sumi_include: []const u8,
    libomp_prefix: []const u8,
    use_openmp: bool,
    use_metal: bool,
    use_gl: bool,
    use_lto: bool,
};

/// Builds a Larimar host: the OS-oblivious core, a game, and the SDL host.
///
/// The core compiles with no platform headers and no Metal/CUDA/GL sources —
/// that separation is the architecture's central invariant, so it is enforced
/// here by what is *absent* from the source list rather than by convention.
fn addLarimarExecutable(b: *std.Build, options: LarimarOptions) *std.Build.Step.InstallArtifact {
    const module = b.createModule(.{
        .target = options.target,
        .optimize = options.optimize,
        .link_libc = true,
        .link_libcpp = true,
    });

    module.addIncludePath(b.path("."));
    module.addIncludePath(b.path("core/include"));
    module.addIncludePath(pathFromOption(b, options.sumi_include));

    const cpp_flags: []const []const u8 = if (options.use_openmp)
        if (options.target.result.os.tag == .macos)
            &.{ "-std=c++11", "-Wall", "-Wextra", "-Wno-nullability-completeness", "-Xpreprocessor", "-fopenmp" }
        else
            &.{ "-std=c++11", "-Wall", "-Wextra", "-fopenmp" }
    else
        &.{ "-std=c++11", "-Wall", "-Wextra" };

    // Kantei Grade 2 (Paper). The core links no windowing library, so the host
    // supplies GL entry points through eshi_gl_set_proc_loader().
    if (options.use_gl) module.addCMacro("ESHI_HAVE_GL", "1");

    module.addCSourceFiles(.{
        .files = if (options.use_gl)
            &.{
                "core/src/world.cpp",
                "core/src/render/registry.cpp",
                "core/src/render/transpile.cpp",
                "core/src/render/ink.cpp",
                "core/src/render/gl.cpp",
            }
        else
            &.{
                "core/src/world.cpp",
                "core/src/render/registry.cpp",
                "core/src/render/transpile.cpp",
                "core/src/render/ink.cpp",
            },
        .flags = cpp_flags,
        .language = .cpp,
    });

    // Kantei Grade 3 (Brush). Metal renders offscreen and opens no window, so
    // it stays inside the core's boundary rule.
    if (options.use_metal) {
        module.addCMacro("ESHI_HAVE_METAL", "1");
        module.addCSourceFile(.{
            .file = b.path("core/src/render/metal.mm"),
            .flags = &.{ "-std=c++11", "-Wall", "-Wextra", "-Wno-nullability-completeness", "-fobjc-arc" },
            .language = .objective_cpp,
        });
        module.linkFramework("Metal", .{});
        module.linkFramework("Foundation", .{});
    }

    module.addCSourceFiles(.{
        .files = options.game_sources,
        .flags = cpp_flags,
        .language = .cpp,
    });
    module.addCSourceFiles(.{
        .files = &.{"hosts/sdl/host_sdl.cpp"},
        .flags = cpp_flags,
        .language = .cpp,
    });

    if (options.use_openmp) {
        if (options.target.result.os.tag == .macos) {
            module.addSystemIncludePath(pathFromOption(b, b.pathJoin(&.{ options.libomp_prefix, "include" })));
            module.addLibraryPath(pathFromOption(b, b.pathJoin(&.{ options.libomp_prefix, "lib" })));
            module.linkSystemLibrary("omp", .{ .use_pkg_config = .no });
        } else {
            module.linkSystemLibrary("gomp", .{ .use_pkg_config = .no });
        }
    }

    const packages = [_][]const u8{
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
