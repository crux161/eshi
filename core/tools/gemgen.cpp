/**
 * @file gemgen.cpp
 * @brief Emit the reference greybox asset as a GLB.
 *
 * PLAN Step 8 calls for a faceted gem modelled in Blender. This is its
 * greybox: a procedurally generated icosahedron with flat-shaded facets and a
 * PBR material, written straight out as a GLB. It is here rather than checked
 * in as a binary for the reason every other generated artifact in this
 * repository is generated — the input is reviewable, the output is
 * reproducible, and nobody has to trust a blob. A Blender-authored replacement
 * drops into the same path without touching a line of engine code.
 *
 * The file it writes is a plain glTF 2.0 binary container: a JSON chunk
 * describing one mesh with one material, then a binary chunk holding
 * interleaved positions and normals plus the index buffer.
 *
 * usage: eshi-gemgen [-o larimar_logo.glb]
 */
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

namespace {

struct Vec3 {
    float x, y, z;
};

Vec3 subtract(const Vec3& a, const Vec3& b) { return Vec3{a.x - b.x, a.y - b.y, a.z - b.z}; }

Vec3 cross(const Vec3& a, const Vec3& b) {
    return Vec3{a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}

Vec3 normalize(const Vec3& v) {
    const float length = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
    if (length <= 0.0f) return Vec3{0.0f, 1.0f, 0.0f};
    return Vec3{v.x / length, v.y / length, v.z / length};
}

/**
 * An icosahedron, stretched into something gem-shaped.
 *
 * Flat facets are the point: every triangle gets its own three vertices and one
 * face normal, so the silhouette reads as cut stone under a single light rather
 * than as a smooth ball. That also makes the asset a fair test of the loader —
 * 60 vertices, 20 primitives, no shared indices to hide a stride bug.
 */
void build_gem(std::vector<Vec3>* positions, std::vector<Vec3>* normals,
               std::vector<uint16_t>* indices) {
    const float t = (1.0f + std::sqrt(5.0f)) * 0.5f;
    Vec3 base[12] = {
            {-1, t, 0}, {1, t, 0},  {-1, -t, 0}, {1, -t, 0},
            {0, -1, t}, {0, 1, t},  {0, -1, -t}, {0, 1, -t},
            {t, 0, -1}, {t, 0, 1},  {-t, 0, -1}, {-t, 0, 1},
    };
    static const int faces[20][3] = {
            {0, 11, 5}, {0, 5, 1},   {0, 1, 7},   {0, 7, 10}, {0, 10, 11},
            {1, 5, 9},  {5, 11, 4},  {11, 10, 2}, {10, 7, 6}, {7, 1, 8},
            {3, 9, 4},  {3, 4, 2},   {3, 2, 6},   {3, 6, 8},  {3, 8, 9},
            {4, 9, 5},  {2, 4, 11},  {6, 2, 10},  {8, 6, 7},  {9, 8, 1},
    };

    /* Unit radius, then taller than wide: a stone, not a die. */
    for (int i = 0; i < 12; ++i) {
        base[i] = normalize(base[i]);
        base[i].x *= 0.62f;
        base[i].y *= 0.80f;
        base[i].z *= 0.62f;
    }

    for (int face = 0; face < 20; ++face) {
        const Vec3 a = base[faces[face][0]];
        const Vec3 b = base[faces[face][1]];
        const Vec3 c = base[faces[face][2]];
        const Vec3 normal = normalize(cross(subtract(b, a), subtract(c, a)));
        const uint16_t first = (uint16_t)positions->size();
        positions->push_back(a);
        positions->push_back(b);
        positions->push_back(c);
        normals->push_back(normal);
        normals->push_back(normal);
        normals->push_back(normal);
        indices->push_back(first);
        indices->push_back((uint16_t)(first + 1));
        indices->push_back((uint16_t)(first + 2));
    }
}

void append(std::vector<uint8_t>* buffer, const void* data, size_t bytes) {
    const uint8_t* start = static_cast<const uint8_t*>(data);
    buffer->insert(buffer->end(), start, start + bytes);
}

void pad_to_four(std::vector<uint8_t>* buffer, uint8_t filler) {
    while ((buffer->size() % 4) != 0) buffer->push_back(filler);
}

void write_u32(std::vector<uint8_t>* buffer, uint32_t value) {
    /* glTF is little-endian on the wire regardless of the host. */
    const uint8_t bytes[4] = {(uint8_t)(value & 0xFF), (uint8_t)((value >> 8) & 0xFF),
                              (uint8_t)((value >> 16) & 0xFF), (uint8_t)((value >> 24) & 0xFF)};
    append(buffer, bytes, sizeof(bytes));
}

std::string format_float(float value) {
    char text[32];
    std::snprintf(text, sizeof(text), "%.6g", (double)value);
    return text;
}

} /* namespace */

int main(int argc, char** argv) {
    std::string output = "larimar_logo.glb";
    for (int i = 1; i < argc; ++i) {
        const std::string argument = argv[i];
        if ((argument == "-o" || argument == "--output") && i + 1 < argc) {
            output = argv[++i];
        } else {
            std::fprintf(stderr, "usage: %s [-o out.glb]\n", argv[0]);
            return 2;
        }
    }

    std::vector<Vec3> positions;
    std::vector<Vec3> normals;
    std::vector<uint16_t> indices;
    build_gem(&positions, &normals, &indices);

    /* Bounds are required on the POSITION accessor, and gltfio checks them. */
    Vec3 minimum = positions[0];
    Vec3 maximum = positions[0];
    for (size_t i = 1; i < positions.size(); ++i) {
        minimum.x = positions[i].x < minimum.x ? positions[i].x : minimum.x;
        minimum.y = positions[i].y < minimum.y ? positions[i].y : minimum.y;
        minimum.z = positions[i].z < minimum.z ? positions[i].z : minimum.z;
        maximum.x = positions[i].x > maximum.x ? positions[i].x : maximum.x;
        maximum.y = positions[i].y > maximum.y ? positions[i].y : maximum.y;
        maximum.z = positions[i].z > maximum.z ? positions[i].z : maximum.z;
    }

    std::vector<uint8_t> binary;
    const uint32_t positions_offset = 0;
    append(&binary, positions.data(), positions.size() * sizeof(Vec3));
    const uint32_t normals_offset = (uint32_t)binary.size();
    append(&binary, normals.data(), normals.size() * sizeof(Vec3));
    const uint32_t indices_offset = (uint32_t)binary.size();
    append(&binary, indices.data(), indices.size() * sizeof(uint16_t));
    pad_to_four(&binary, 0);

    std::string json;
    json += "{\"asset\":{\"version\":\"2.0\",\"generator\":\"eshi-gemgen\"},";
    json += "\"scene\":0,\"scenes\":[{\"nodes\":[0]}],";
    json += "\"nodes\":[{\"mesh\":0,\"name\":\"larimar_logo\"}],";
    json += "\"meshes\":[{\"name\":\"gem\",\"primitives\":[{\"attributes\":{\"POSITION\":0,"
            "\"NORMAL\":1},\"indices\":2,\"material\":0}]}],";
    /*
     * A pale blue dielectric with a smooth-ish surface: enough roughness to
     * catch the single directional light the backend installs, and no metallic
     * component, because metal without image-based lighting renders black and
     * that would look like a bug rather than a missing feature.
     */
    json += "\"materials\":[{\"name\":\"larimar\",\"pbrMetallicRoughness\":{"
            "\"baseColorFactor\":[0.46,0.79,0.86,1.0],"
            "\"metallicFactor\":0.0,\"roughnessFactor\":0.35}}],";

    json += "\"accessors\":[";
    json += "{\"bufferView\":0,\"componentType\":5126,\"count\":" +
            std::to_string(positions.size()) + ",\"type\":\"VEC3\",\"min\":[" +
            format_float(minimum.x) + "," + format_float(minimum.y) + "," +
            format_float(minimum.z) + "],\"max\":[" + format_float(maximum.x) + "," +
            format_float(maximum.y) + "," + format_float(maximum.z) + "]},";
    json += "{\"bufferView\":1,\"componentType\":5126,\"count\":" +
            std::to_string(normals.size()) + ",\"type\":\"VEC3\"},";
    json += "{\"bufferView\":2,\"componentType\":5123,\"count\":" +
            std::to_string(indices.size()) + ",\"type\":\"SCALAR\"}],";

    json += "\"bufferViews\":[";
    json += "{\"buffer\":0,\"byteOffset\":" + std::to_string(positions_offset) +
            ",\"byteLength\":" + std::to_string(positions.size() * sizeof(Vec3)) +
            ",\"target\":34962},";
    json += "{\"buffer\":0,\"byteOffset\":" + std::to_string(normals_offset) +
            ",\"byteLength\":" + std::to_string(normals.size() * sizeof(Vec3)) +
            ",\"target\":34962},";
    json += "{\"buffer\":0,\"byteOffset\":" + std::to_string(indices_offset) +
            ",\"byteLength\":" + std::to_string(indices.size() * sizeof(uint16_t)) +
            ",\"target\":34963}],";
    json += "\"buffers\":[{\"byteLength\":" + std::to_string(binary.size()) + "}]}";

    std::vector<uint8_t> json_chunk;
    append(&json_chunk, json.data(), json.size());
    pad_to_four(&json_chunk, ' '); /* JSON pads with spaces, BIN with zeroes. */

    std::vector<uint8_t> glb;
    write_u32(&glb, 0x46546C67u); /* "glTF" */
    write_u32(&glb, 2u);
    write_u32(&glb, (uint32_t)(12 + 8 + json_chunk.size() + 8 + binary.size()));
    write_u32(&glb, (uint32_t)json_chunk.size());
    write_u32(&glb, 0x4E4F534Au); /* "JSON" */
    append(&glb, json_chunk.data(), json_chunk.size());
    write_u32(&glb, (uint32_t)binary.size());
    write_u32(&glb, 0x004E4942u); /* "BIN\0" */
    append(&glb, binary.data(), binary.size());

    std::ofstream stream(output.c_str(), std::ios::binary | std::ios::trunc);
    if (!stream) {
        std::fprintf(stderr, "eshi-gemgen: cannot write %s\n", output.c_str());
        return 1;
    }
    stream.write(reinterpret_cast<const char*>(glb.data()), (std::streamsize)glb.size());
    if (!stream) {
        std::fprintf(stderr, "eshi-gemgen: failed while writing %s\n", output.c_str());
        return 1;
    }
    std::printf("eshi-gemgen: %s (%zu triangles, %zu bytes)\n", output.c_str(),
                indices.size() / 3, glb.size());
    return 0;
}
