/**
 * @file ppmdiff.cpp
 * @brief Compare two directories of raw P6 frames and report the worst channel.
 *
 * The cross-tier conformance claim — Ink, Paper, and Brush agree to within one
 * least-significant bit — was measured by hand and written into a table in
 * docs/larimar/ARCHITECTURE.md. A number nobody can re-derive is a number that
 * quietly stops being true, so this makes it a command.
 *
 * It deliberately reports the *maximum* absolute channel difference rather than
 * a mean or a perceptual score. Averages hide exactly the failure this exists to
 * catch: one wrong pixel in a frame is a broken transpile rule, and a mean over
 * 320x180 buries it below the rounding noise that is expected everywhere else.
 *
 * usage: ppmdiff <dir-a> <dir-b> [max-allowed-diff]
 */
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include <dirent.h>

namespace {

struct Image {
    int width;
    int height;
    std::vector<unsigned char> pixels; /* RGB, tightly packed. */
};

/** Reads binary P6 with the exact header the SDL host's --dump writes. */
bool read_ppm(const std::string& path, Image* out, std::string* error) {
    std::FILE* file = std::fopen(path.c_str(), "rb");
    if (!file) {
        *error = "cannot open " + path;
        return false;
    }
    int width = 0, height = 0, maximum = 0;
    if (std::fscanf(file, "P6\n%d %d\n%d\n", &width, &height, &maximum) != 3 ||
        width <= 0 || height <= 0 || maximum != 255) {
        std::fclose(file);
        *error = "not a 255-level binary PPM: " + path;
        return false;
    }
    out->width = width;
    out->height = height;
    out->pixels.assign((size_t)width * (size_t)height * 3, 0);
    const size_t read = std::fread(&out->pixels[0], 1, out->pixels.size(), file);
    std::fclose(file);
    if (read != out->pixels.size()) {
        *error = "truncated frame: " + path;
        return false;
    }
    return true;
}

std::vector<std::string> frame_names(const std::string& directory, std::string* error) {
    std::vector<std::string> names;
    DIR* dir = opendir(directory.c_str());
    if (!dir) {
        *error = "cannot open directory " + directory;
        return names;
    }
    while (struct dirent* entry = readdir(dir)) {
        const std::string name = entry->d_name;
        if (name.size() > 4 && name.compare(name.size() - 4, 4, ".ppm") == 0) {
            names.push_back(name);
        }
    }
    closedir(dir);
    /* The host names frames %06d, so lexical order is frame order. */
    for (size_t i = 1; i < names.size(); ++i) {
        const std::string key = names[i];
        size_t j = i;
        while (j > 0 && names[j - 1] > key) {
            names[j] = names[j - 1];
            --j;
        }
        names[j] = key;
    }
    return names;
}

} /* namespace */

int main(int argc, char** argv) {
    if (argc < 3) {
        std::fprintf(stderr, "usage: %s <dir-a> <dir-b> [max-allowed-diff]\n", argv[0]);
        return 2;
    }
    const std::string left_dir = argv[1];
    const std::string right_dir = argv[2];
    const int allowed = argc > 3 ? std::atoi(argv[3]) : 1;

    std::string error;
    const std::vector<std::string> left_names = frame_names(left_dir, &error);
    if (!error.empty()) {
        std::fprintf(stderr, "ppmdiff: %s\n", error.c_str());
        return 2;
    }
    const std::vector<std::string> right_names = frame_names(right_dir, &error);
    if (!error.empty()) {
        std::fprintf(stderr, "ppmdiff: %s\n", error.c_str());
        return 2;
    }
    if (left_names.empty()) {
        std::fprintf(stderr, "ppmdiff: %s holds no frames\n", left_dir.c_str());
        return 2;
    }
    if (left_names.size() != right_names.size()) {
        std::fprintf(stderr, "ppmdiff: %s has %zu frames, %s has %zu\n", left_dir.c_str(),
                     left_names.size(), right_dir.c_str(), right_names.size());
        return 2;
    }

    int worst = 0;
    std::string worst_frame;
    int worst_x = 0, worst_y = 0;
    double total = 0.0;
    double samples = 0.0;

    for (size_t i = 0; i < left_names.size(); ++i) {
        Image left, right;
        if (!read_ppm(left_dir + "/" + left_names[i], &left, &error) ||
            !read_ppm(right_dir + "/" + right_names[i], &right, &error)) {
            std::fprintf(stderr, "ppmdiff: %s\n", error.c_str());
            return 2;
        }
        if (left.width != right.width || left.height != right.height) {
            std::fprintf(stderr, "ppmdiff: %s is %dx%d, %s is %dx%d\n", left_names[i].c_str(),
                         left.width, left.height, right_names[i].c_str(), right.width,
                         right.height);
            return 2;
        }
        for (size_t p = 0; p < left.pixels.size(); ++p) {
            const int difference = std::abs((int)left.pixels[p] - (int)right.pixels[p]);
            total += difference;
            samples += 1.0;
            if (difference > worst) {
                worst = difference;
                worst_frame = left_names[i];
                worst_x = (int)((p / 3) % (size_t)left.width);
                worst_y = (int)((p / 3) / (size_t)left.width);
            }
        }
    }

    std::printf("frames=%zu mean=%.4f/255 max=%d/255", left_names.size(),
                samples > 0.0 ? total / samples : 0.0, worst);
    if (worst > 0) {
        std::printf(" worst=%s@%d,%d", worst_frame.c_str(), worst_x, worst_y);
    }
    std::printf("\n");

    if (worst > allowed) {
        std::fprintf(stderr,
                     "ppmdiff: tiers disagree by %d, above the %d allowed. One LSB is float "
                     "evaluation differing; more than that is a transpile or binding bug.\n",
                     worst, allowed);
        return 1;
    }
    return 0;
}
