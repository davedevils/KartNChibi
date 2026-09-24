#pragma once

#include <vector>
#include <string>
#include <cstdint>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <algorithm>

namespace KnCTest {

struct ImageData {
    std::vector<uint8_t> pixels; // RGBA
    int width  = 0;
    int height = 0;
};

struct ComparisonResult {
    double rmseR = 0, rmseG = 0, rmseB = 0, rmseA = 0;
    double rmseTotal    = 0;    // Combined RMSE across RGB
    int    maxDeltaR    = 0, maxDeltaG = 0, maxDeltaB = 0;
    int    maxDelta     = 0;
    int    mismatchPixels = 0;  // pixels with any diff above threshold
    int    totalPixels  = 0;
    double mismatchPct  = 0.0;  // mismatch pixels divided by total pixels times 100
    bool   passed       = false;
    std::string details;
};

/// loads a bmp file into an rgba pixel buffer
inline ImageData LoadBMP(const std::string& path) {
    ImageData img;
    std::ifstream f(path, std::ios::binary);
    if (!f.is_open()) return img;

    uint8_t header[54];
    f.read(reinterpret_cast<char*>(header), 54);
    if (f.gcount() < 54) return img;

    img.width  = *(int32_t*)&header[18];
    img.height = *(int32_t*)&header[22];
    int bpp    = *(int16_t*)&header[28];
    if (bpp != 24 && bpp != 32) return img;

    int channels = bpp / 8;
    int rowBytes = ((img.width * channels + 3) & ~3);
    int dataSize = rowBytes * img.height;

    std::vector<uint8_t> raw(dataSize);
    f.read(reinterpret_cast<char*>(raw.data()), dataSize);

    // converts bgr a bottom up to rgba top down
    img.pixels.resize(img.width * img.height * 4);
    for (int y = 0; y < img.height; y++) {
        int srcY = img.height - 1 - y; // BMP is bottom-up
        for (int x = 0; x < img.width; x++) {
            int srcIdx = srcY * rowBytes + x * channels;
            int dstIdx = (y * img.width + x) * 4;
            // bmp stores bgr this maps it to rgba alpha 255 when the source has none
            img.pixels[dstIdx + 0] = raw[srcIdx + 2];
            img.pixels[dstIdx + 1] = raw[srcIdx + 1];
            img.pixels[dstIdx + 2] = raw[srcIdx + 0];
            img.pixels[dstIdx + 3] = (channels == 4) ? raw[srcIdx + 3] : 255;
        }
    }
    return img;
}

/// compares two rgba images using a per channel diff threshold for mismatch
inline ComparisonResult CompareImages(const ImageData& test, const ImageData& golden, int threshold = 2) {
    ComparisonResult r;
    if (test.width != golden.width || test.height != golden.height ||
        test.pixels.empty() || golden.pixels.empty()) {
        r.details = "Size mismatch or empty image";
        return r;
    }

    r.totalPixels = test.width * test.height;
    double sumSqR = 0, sumSqG = 0, sumSqB = 0, sumSqA = 0;

    for (int i = 0; i < r.totalPixels; i++) {
        int idx = i * 4;
        int dR = std::abs((int)test.pixels[idx+0] - (int)golden.pixels[idx+0]);
        int dG = std::abs((int)test.pixels[idx+1] - (int)golden.pixels[idx+1]);
        int dB = std::abs((int)test.pixels[idx+2] - (int)golden.pixels[idx+2]);
        int dA = std::abs((int)test.pixels[idx+3] - (int)golden.pixels[idx+3]);

        sumSqR += dR * dR;
        sumSqG += dG * dG;
        sumSqB += dB * dB;
        sumSqA += dA * dA;

        if (dR > r.maxDeltaR) r.maxDeltaR = dR;
        if (dG > r.maxDeltaG) r.maxDeltaG = dG;
        if (dB > r.maxDeltaB) r.maxDeltaB = dB;

        int maxCh = std::max({dR, dG, dB});
        if (maxCh > r.maxDelta) r.maxDelta = maxCh;
        if (maxCh > threshold) r.mismatchPixels++;
    }

    r.rmseR = std::sqrt(sumSqR / r.totalPixels);
    r.rmseG = std::sqrt(sumSqG / r.totalPixels);
    r.rmseB = std::sqrt(sumSqB / r.totalPixels);
    r.rmseA = std::sqrt(sumSqA / r.totalPixels);
    r.rmseTotal = std::sqrt((sumSqR + sumSqG + sumSqB) / (r.totalPixels * 3.0));
    r.mismatchPct = (r.totalPixels > 0) ? (r.mismatchPixels * 100.0 / r.totalPixels) : 0.0;

    // default thresholds rmse under 1 over 255 and mismatch under half a percent
    r.passed = (r.rmseTotal < 1.0) && (r.mismatchPct < 0.5);

    char buf[512];
    snprintf(buf, sizeof(buf),
        "RMSE: %.3f (R:%.3f G:%.3f B:%.3f) | MaxDelta: %d | Mismatch: %d/%d (%.2f%%)",
        r.rmseTotal, r.rmseR, r.rmseG, r.rmseB,
        r.maxDelta, r.mismatchPixels, r.totalPixels, r.mismatchPct);
    r.details = buf;
    return r;
}

/// generates a diff image with amplified differences in the red channel
inline ImageData GenerateDiffImage(const ImageData& test, const ImageData& golden, int amplify = 10) {
    ImageData diff;
    if (test.width != golden.width || test.height != golden.height) return diff;
    diff.width = test.width;
    diff.height = test.height;
    diff.pixels.resize(test.pixels.size());

    for (size_t i = 0; i < test.pixels.size(); i += 4) {
        int dR = std::abs((int)test.pixels[i+0] - (int)golden.pixels[i+0]);
        int dG = std::abs((int)test.pixels[i+1] - (int)golden.pixels[i+1]);
        int dB = std::abs((int)test.pixels[i+2] - (int)golden.pixels[i+2]);
        int maxD = std::max({dR, dG, dB});

        int v = std::min(255, maxD * amplify);
        // red channel holds the diff green and blue stay 0 alpha stays opaque
        diff.pixels[i+0] = (uint8_t)v;
        diff.pixels[i+1] = 0;
        diff.pixels[i+2] = 0;
        diff.pixels[i+3] = 255;
    }
    return diff;
}

/// saves rgba pixels as a bmp for diff images
inline bool SaveRGBAToBMP(const std::string& path, const ImageData& img) {
    if (img.pixels.empty()) return false;
    int rowBytes = ((img.width * 3 + 3) & ~3);
    int dataSize = rowBytes * img.height;

    uint8_t header[54] = {};
    *(uint16_t*)&header[0] = 0x4D42; // 'BM'
    *(uint32_t*)&header[2] = 54 + dataSize;
    *(uint32_t*)&header[10] = 54;
    *(uint32_t*)&header[14] = 40;
    *(int32_t*)&header[18] = img.width;
    *(int32_t*)&header[22] = img.height;
    *(uint16_t*)&header[26] = 1;
    *(uint16_t*)&header[28] = 24;

    std::ofstream f(path, std::ios::binary);
    if (!f.is_open()) return false;
    f.write(reinterpret_cast<char*>(header), 54);

    std::vector<uint8_t> row(rowBytes, 0);
    for (int y = img.height - 1; y >= 0; y--) { // BMP bottom-up
        for (int x = 0; x < img.width; x++) {
            int srcIdx = (y * img.width + x) * 4;
            row[x * 3 + 0] = img.pixels[srcIdx + 2];
            row[x * 3 + 1] = img.pixels[srcIdx + 1];
            row[x * 3 + 2] = img.pixels[srcIdx + 0];
        }
        f.write(reinterpret_cast<char*>(row.data()), rowBytes);
    }
    return true;
}

/// full visual comparison that loads test and golden bmps compares and optionally generates a diff
inline ComparisonResult CompareScreenshotAdvanced(
    const std::string& testPath,
    const std::string& goldenPath,
    const std::string& diffOutputPath = "",
    int threshold = 2)
{
    auto test   = LoadBMP(testPath);
    auto golden = LoadBMP(goldenPath);

    if (test.pixels.empty()) {
        ComparisonResult r;
        r.details = "Failed to load test image: " + testPath;
        return r;
    }
    if (golden.pixels.empty()) {
        ComparisonResult r;
        r.details = "Failed to load golden image: " + goldenPath;
        return r;
    }

    auto result = CompareImages(test, golden, threshold);

    if (!diffOutputPath.empty() && !result.passed) {
        auto diff = GenerateDiffImage(test, golden);
        SaveRGBAToBMP(diffOutputPath, diff);
        result.details += " | Diff saved: " + diffOutputPath;
    }

    return result;
}

}
