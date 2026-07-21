const std = @import("std");

/// Memory layout matching sumi::vec2 (2 x f32)
pub const Vec2 = extern struct {
    x: f32 = 0.0,
    y: f32 = 0.0,

    pub fn init(x: f32, y: f32) Vec2 {
        return .{ .x = x, .y = y };
    }
};

/// 3D Vector helpers for raymarching
pub const Vec3 = extern struct {
    x: f32 = 0.0,
    y: f32 = 0.0,
    z: f32 = 0.0,

    pub fn init(x: f32, y: f32, z: f32) Vec3 {
        return .{ .x = x, .y = y, .z = z };
    }

    pub fn add(a: Vec3, b: Vec3) Vec3 {
        return .{ .x = a.x + b.x, .y = a.y + b.y, .z = a.z + b.z };
    }

    pub fn sub(a: Vec3, b: Vec3) Vec3 {
        return .{ .x = a.x - b.x, .y = a.y - b.y, .z = a.z - b.z };
    }

    pub fn mulScalar(a: Vec3, s: f32) Vec3 {
        return .{ .x = a.x * s, .y = a.y * s, .z = a.z * s };
    }

    pub fn dot(a: Vec3, b: Vec3) f32 {
        return a.x * b.x + a.y * b.y + a.z * b.z;
    }

    pub fn len(a: Vec3) f32 {
        return @sqrt(dot(a, a));
    }

    pub fn normalize(a: Vec3) Vec3 {
        const l = len(a);
        if (l < 0.00001) return Vec3.init(0, 0, 0);
        return mulScalar(a, 1.0 / l);
    }

    pub fn cross(a: Vec3, b: Vec3) Vec3 {
        return .{
            .x = a.y * b.z - a.z * b.y,
            .y = a.z * b.x - a.x * b.z,
            .z = a.x * b.y - a.y * b.x,
        };
    }

    pub fn lerp(a: Vec3, b: Vec3, t: f32) Vec3 {
        return a.mulScalar(1.0 - t).add(b.mulScalar(t));
    }
};

/// Memory layout matching sumi::vec4 (4 x f32)
pub const Vec4 = extern struct {
    x: f32 = 0.0,
    y: f32 = 0.0,
    z: f32 = 0.0,
    w: f32 = 1.0,

    pub fn init(x: f32, y: f32, z: f32, w: f32) Vec4 {
        return .{ .x = x, .y = y, .z = z, .w = w };
    }
};

const MapResult = struct {
    dist: f32,
    mat_id: f32,
};

fn hash21(x: f32, y: f32) f32 {
    const dot_val = x * 12.9898 + y * 78.233;
    const sin_val = @sin(dot_val) * 43758.5453;
    return sin_val - @floor(sin_val);
}

fn map(p: Vec3, iTime: f32) MapResult {
    // 1. Bouncing Ball
    const bounce_height = @abs(@sin(iTime * 3.0)) * 2.0;
    const sphere_pos = p.sub(Vec3.init(0.0, 1.0 + bounce_height, 0.0));
    const d_sphere = sphere_pos.len() - 1.0;

    // 2. Infinite Floor
    const d_floor = p.y;

    if (d_sphere < d_floor) {
        return .{ .dist = d_sphere, .mat_id = 1.0 };
    }
    return .{ .dist = d_floor, .mat_id = 0.0 };
}

fn calcNormal(p: Vec3, iTime: f32) Vec3 {
    const e: f32 = 0.001;
    const nx = map(Vec3.init(p.x + e, p.y, p.z), iTime).dist - map(Vec3.init(p.x - e, p.y, p.z), iTime).dist;
    const ny = map(Vec3.init(p.x, p.y + e, p.z), iTime).dist - map(Vec3.init(p.x, p.y - e, p.z), iTime).dist;
    const nz = map(Vec3.init(p.x, p.y, p.z + e), iTime).dist - map(Vec3.init(p.x, p.y, p.z - e), iTime).dist;
    return Vec3.normalize(Vec3.init(nx, ny, nz));
}

// Export the exact C++ mangled symbol name expected by eshi's main.o:
// void mainImage(sumi::vec4& fragColor, sumi::vec2 fragCoord, sumi::vec2 iResolution, float iTime)
export fn _Z9mainImageRN4sumi4vec4ENS_4vec2ES2_f(
    fragColor: *Vec4,
    fragCoord: Vec2,
    iResolution: Vec2,
    iTime: f32,
) void {
    const uv_x = (fragCoord.x - 0.5 * iResolution.x) / iResolution.y;
    const uv_y = (fragCoord.y - 0.5 * iResolution.y) / iResolution.y;

    // Orbiting Camera
    const cam_angle = iTime * 0.4;
    const radius: f32 = 8.0;
    const ro = Vec3.init(radius * @sin(cam_angle), 2.5, -radius * @cos(cam_angle));

    const target = Vec3.init(0.0, 1.0, 0.0);
    const cw = Vec3.normalize(target.sub(ro));
    const cu = Vec3.normalize(Vec3.cross(cw, Vec3.init(0.0, 1.0, 0.0)));
    const cv = Vec3.normalize(Vec3.cross(cu, cw));
    const rd = Vec3.normalize(cu.mulScalar(uv_x).add(cv.mulScalar(uv_y)).add(cw.mulScalar(1.0)));

    // Raymarching Loop
    var d0: f32 = 0.0;
    var hit_mat: f32 = 0.0;
    var p = ro;

    var i: u32 = 0;
    while (i < 100) : (i += 1) {
        p = ro.add(rd.mulScalar(d0));
        const res = map(p, iTime);
        hit_mat = res.mat_id;
        if (res.dist < 0.001 or d0 > 40.0) break;
        d0 += res.dist;
    }

    var col = Vec3.init(0.0, 0.0, 0.0);
    const mist_color = Vec3.init(0.05, 0.08, 0.18);

    if (d0 < 40.0) {
        const n = calcNormal(p, iTime);
        const light = Vec3.normalize(Vec3.init(1.0, 2.0, -1.0));
        const diff = @max(0.0, Vec3.dot(n, light));

        if (hit_mat == 0.0) {
            // Harlequin Floor (Checkerboard)
            const fx = @floor(p.x);
            const fz = @floor(p.z);
            const check = @mod(fx + fz, 2.0);

            var base_col = if (check < 0.5) Vec3.init(0.08, 0.08, 0.12) else Vec3.init(0.85, 0.85, 0.90);

            // Twinkling Sparkles on dark tiles
            const grid_x = @floor(p.x * 12.0);
            const grid_z = @floor(p.z * 12.0);
            const noise_val = hash21(grid_x, grid_z);
            const twinkle = std.math.pow(f32, @max(0.0, @sin(iTime * 6.0 + noise_val * 62.83) * 0.5 + 0.5), 12.0);
            const sparkle_col = Vec3.init(1.0, 0.9, 0.5).mulScalar(twinkle * 1.5 * (if (check < 0.5) @as(f32, 1.0) else @as(f32, 0.0)));

            col = base_col.mulScalar(diff + 0.1).add(sparkle_col);
        } else {
            // Bouncing Sphere
            col = Vec3.init(0.9, 0.2, 0.35).mulScalar(diff + 0.15);
        }
    }

    // Distance Mist
    const fog_amount = 1.0 - @exp(-d0 * 0.05);
    col = Vec3.lerp(col, mist_color, fog_amount);

    // Gamma correction
    const inv_gamma = 1.0 / 2.2;
    fragColor.* = Vec4.init(
        std.math.pow(f32, @max(0.0, col.x), inv_gamma),
        std.math.pow(f32, @max(0.0, col.y), inv_gamma),
        std.math.pow(f32, @max(0.0, col.z), inv_gamma),
        1.0,
    );
}
