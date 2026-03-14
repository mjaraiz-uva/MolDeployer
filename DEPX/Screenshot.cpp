#include "Screenshot.h"
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <vector>
#include <fstream>
#include <mutex>
#include "../Logger/Logger.h"

namespace Screenshot {

    static std::mutex g_mutex;
    static std::string g_pendingFilename;

    void Request(const std::string& filename) {
        std::lock_guard<std::mutex> lock(g_mutex);
        g_pendingFilename = filename;
    }

    // Write a BMP file from raw RGBA pixel data (bottom-up)
    static bool WriteBMP(const std::string& filename, int width, int height, const std::vector<unsigned char>& pixels) {
        // BMP expects BGR bottom-up, and rows padded to 4-byte boundaries
        int rowStride = width * 3;
        int rowPadding = (4 - (rowStride % 4)) % 4;
        int paddedRowSize = rowStride + rowPadding;
        int imageSize = paddedRowSize * height;
        int fileSize = 54 + imageSize;

        std::ofstream file(filename, std::ios::binary);
        if (!file.is_open()) return false;

        // BMP file header (14 bytes)
        unsigned char bmpFileHeader[14] = {};
        bmpFileHeader[0] = 'B';
        bmpFileHeader[1] = 'M';
        bmpFileHeader[2] = fileSize & 0xFF;
        bmpFileHeader[3] = (fileSize >> 8) & 0xFF;
        bmpFileHeader[4] = (fileSize >> 16) & 0xFF;
        bmpFileHeader[5] = (fileSize >> 24) & 0xFF;
        bmpFileHeader[10] = 54; // pixel data offset
        file.write(reinterpret_cast<char*>(bmpFileHeader), 14);

        // DIB header (40 bytes - BITMAPINFOHEADER)
        unsigned char bmpInfoHeader[40] = {};
        bmpInfoHeader[0] = 40; // header size
        bmpInfoHeader[4] = width & 0xFF;
        bmpInfoHeader[5] = (width >> 8) & 0xFF;
        bmpInfoHeader[6] = (width >> 16) & 0xFF;
        bmpInfoHeader[7] = (width >> 24) & 0xFF;
        bmpInfoHeader[8] = height & 0xFF;
        bmpInfoHeader[9] = (height >> 8) & 0xFF;
        bmpInfoHeader[10] = (height >> 16) & 0xFF;
        bmpInfoHeader[11] = (height >> 24) & 0xFF;
        bmpInfoHeader[12] = 1; // color planes
        bmpInfoHeader[14] = 24; // bits per pixel
        bmpInfoHeader[20] = imageSize & 0xFF;
        bmpInfoHeader[21] = (imageSize >> 8) & 0xFF;
        bmpInfoHeader[22] = (imageSize >> 16) & 0xFF;
        bmpInfoHeader[23] = (imageSize >> 24) & 0xFF;
        file.write(reinterpret_cast<char*>(bmpInfoHeader), 40);

        // Pixel data: convert RGBA top-down to BGR bottom-up
        std::vector<unsigned char> row(paddedRowSize, 0);
        for (int y = 0; y < height; ++y) {
            // glReadPixels gives bottom-up, BMP is also bottom-up, so row order is correct
            const unsigned char* src = &pixels[y * width * 4];
            for (int x = 0; x < width; ++x) {
                row[x * 3 + 0] = src[x * 4 + 2]; // B
                row[x * 3 + 1] = src[x * 4 + 1]; // G
                row[x * 3 + 2] = src[x * 4 + 0]; // R
            }
            file.write(reinterpret_cast<char*>(row.data()), paddedRowSize);
        }

        return file.good();
    }

    void CaptureIfPending() {
        std::string filename;
        {
            std::lock_guard<std::mutex> lock(g_mutex);
            if (g_pendingFilename.empty()) return;
            filename = g_pendingFilename;
            g_pendingFilename.clear();
        }

        // Get full window framebuffer size (not the current GL viewport which may be a sub-region)
        int width = 0, height = 0;
        GLFWwindow* window = glfwGetCurrentContext();
        if (window) {
            glfwGetFramebufferSize(window, &width, &height);
        }
        // Set viewport to full framebuffer before reading pixels
        glViewport(0, 0, width, height);

        if (width <= 0 || height <= 0) {
            Logger::Error("Screenshot: Invalid viewport size.");
            return;
        }

        // Read pixels from framebuffer
        std::vector<unsigned char> pixels(width * height * 4);
        glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());

        // Save as BMP
        if (WriteBMP(filename, width, height, pixels)) {
            Logger::Info("Screenshot saved: " + filename);
        } else {
            Logger::Error("Screenshot: Failed to save " + filename);
        }
    }

} // namespace Screenshot
