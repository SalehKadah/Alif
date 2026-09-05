/**
 * ═══════════════════════════════════════════════════════════════════════════════
 * ملف: image_cache.cpp
 * المسار: features/graphics/core/src/image_cache.cpp
 * ═══════════════════════════════════════════════════════════════════════════════
 *
 * تنفيذ نظام ذاكرة الصور المؤقتة.
 *
 * حقوق النشر (c) 2024-2026 فريق لغة ص
 * ═══════════════════════════════════════════════════════════════════════════════
 */

#include "sad_ui/image_cache.h"
#include <algorithm>
#include <fstream>
#include <chrono>

namespace sad {
namespace ui {

// ═══════════════════════════════════════════════════════════════════════════════
// LRUCache
// ═══════════════════════════════════════════════════════════════════════════════

LRUCache::LRUCache(size_t maxSize) : maxSize_(maxSize) {}

void LRUCache::put(const std::string& key, std::shared_ptr<ImageEntry> entry) {
    // إذا موجود، حدّث
    auto it = cache_.find(key);
    if (it != cache_.end()) {
        currentSize_ -= it->second.first->data.sizeInBytes();
        lruOrder_.erase(it->second.second);
        cache_.erase(it);
    }

    size_t entrySize = entry->data.sizeInBytes();

    // طرد حتى يتوفر مكان
    while (currentSize_ + entrySize > maxSize_ && !lruOrder_.empty()) {
        evict();
    }

    lruOrder_.push_front(key);
    cache_[key] = {entry, lruOrder_.begin()};
    currentSize_ += entrySize;
}

std::shared_ptr<ImageEntry> LRUCache::get(const std::string& key) {
    auto it = cache_.find(key);
    if (it == cache_.end()) return nullptr;

    touchKey(key);
    it->second.first->accessCount++;

    auto now = std::chrono::steady_clock::now().time_since_epoch();
    it->second.first->lastAccess =
        std::chrono::duration_cast<std::chrono::milliseconds>(now).count();

    return it->second.first;
}

void LRUCache::remove(const std::string& key) {
    auto it = cache_.find(key);
    if (it == cache_.end()) return;

    currentSize_ -= it->second.first->data.sizeInBytes();
    lruOrder_.erase(it->second.second);
    cache_.erase(it);
}

void LRUCache::clear() {
    cache_.clear();
    lruOrder_.clear();
    currentSize_ = 0;
}

bool LRUCache::contains(const std::string& key) const {
    return cache_.find(key) != cache_.end();
}

void LRUCache::evict() {
    if (lruOrder_.empty()) return;
    const std::string& lruKey = lruOrder_.back();
    auto it = cache_.find(lruKey);
    if (it != cache_.end()) {
        currentSize_ -= it->second.first->data.sizeInBytes();
        cache_.erase(it);
    }
    lruOrder_.pop_back();
}

void LRUCache::touchKey(const std::string& key) {
    auto it = cache_.find(key);
    if (it == cache_.end()) return;
    lruOrder_.erase(it->second.second);
    lruOrder_.push_front(key);
    it->second.second = lruOrder_.begin();
}

// ═══════════════════════════════════════════════════════════════════════════════
// DefaultImageLoader
// ═══════════════════════════════════════════════════════════════════════════════

ImageData DefaultImageLoader::loadFromFile(const std::string& path) {
    ImageData data;
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) return data;

    // قراءة كبيانات خام — التكامل الحقيقي يعتمد على stb_image أو backend
    size_t fileSize = static_cast<size_t>(file.tellg());
    file.seekg(0);
    std::vector<uint8_t> raw(fileSize);
    file.read(reinterpret_cast<char*>(raw.data()), static_cast<std::streamsize>(fileSize));

    // Placeholder: نفترض 1x1 بيكسل
    data.width = 1;
    data.height = 1;
    data.channels = 4;
    data.pixels = {255, 255, 255, 255}; // أبيض
    return data;
}

ImageData DefaultImageLoader::loadFromMemory(const uint8_t* rawData, size_t size) {
    ImageData data;
    if (!rawData || size == 0) return data;
    // Placeholder
    data.width = 1;
    data.height = 1;
    data.channels = 4;
    data.pixels = {255, 255, 255, 255};
    return data;
}

ImageData DefaultImageLoader::resize(const ImageData& image, int newWidth, int newHeight) {
    if (!image.isValid() || newWidth <= 0 || newHeight <= 0) return image;

    ImageData result;
    result.width = newWidth;
    result.height = newHeight;
    result.channels = image.channels;
    result.pixels.resize(static_cast<size_t>(newWidth * newHeight * image.channels));

    // تحجيم بسيط (nearest neighbor)
    for (int y = 0; y < newHeight; ++y) {
        for (int x = 0; x < newWidth; ++x) {
            int srcX = x * image.width / newWidth;
            int srcY = y * image.height / newHeight;
            int srcIdx = (srcY * image.width + srcX) * image.channels;
            int dstIdx = (y * newWidth + x) * image.channels;
            for (int c = 0; c < image.channels; ++c) {
                if (srcIdx + c < static_cast<int>(image.pixels.size()))
                    result.pixels[dstIdx + c] = image.pixels[srcIdx + c];
            }
        }
    }
    return result;
}

// ═══════════════════════════════════════════════════════════════════════════════
// ImageCacheManager
// ═══════════════════════════════════════════════════════════════════════════════

ImageCacheManager::ImageCacheManager() {
    memoryCache_ = std::make_unique<LRUCache>(config_.maxMemoryBytes);
    loader_ = std::make_shared<DefaultImageLoader>();
}

ImageCacheManager& ImageCacheManager::instance() {
    static ImageCacheManager inst;
    return inst;
}

void ImageCacheManager::configure(const ImageCacheConfig& config) {
    std::lock_guard<std::mutex> lock(mutex_);
    config_ = config;
    memoryCache_ = std::make_unique<LRUCache>(config.maxMemoryBytes);
}

std::shared_ptr<ImageEntry> ImageCacheManager::getImage(const std::string& key) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto entry = memoryCache_->get(key);
    if (entry) {
        ++cacheHits_;
        return entry;
    }
    ++cacheMisses_;
    return nullptr;
}

std::shared_ptr<ImageEntry> ImageCacheManager::loadFromFile(const std::string& path,
                                                              const std::string& key) {
    std::string cacheKey = key.empty() ? path : key;

    // تحقق من الذاكرة أولاً
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto existing = memoryCache_->get(cacheKey);
        if (existing && existing->state == ImageLoadState::Loaded) {
            ++cacheHits_;
            return existing;
        }
    }

    ++cacheMisses_;

    // تحميل
    auto entry = std::make_shared<ImageEntry>();
    entry->key = cacheKey;
    entry->state = ImageLoadState::Loading;

    ImageData data = loader_->loadFromFile(path);
    if (!data.isValid()) {
        entry->state = ImageLoadState::Error;
        entry->errorMessage = "فشل تحميل الصورة: " + path;
        return entry;
    }

    // تحجيم إذا لزم
    if (config_.maxWidth > 0 && data.width > config_.maxWidth) {
        float ratio = static_cast<float>(config_.maxWidth) / data.width;
        int newH = static_cast<int>(data.height * ratio);
        data = loader_->resize(data, config_.maxWidth, newH);
    }
    if (config_.maxHeight > 0 && data.height > config_.maxHeight) {
        float ratio = static_cast<float>(config_.maxHeight) / data.height;
        int newW = static_cast<int>(data.width * ratio);
        data = loader_->resize(data, newW, config_.maxHeight);
    }

    entry->data = std::move(data);
    entry->state = ImageLoadState::Loaded;

    {
        std::lock_guard<std::mutex> lock(mutex_);
        memoryCache_->put(cacheKey, entry);
    }

    if (loadCb_) loadCb_(cacheKey, entry);
    return entry;
}

std::shared_ptr<ImageEntry> ImageCacheManager::loadFromMemory(const std::string& key,
                                                                const uint8_t* data, size_t size) {
    auto entry = std::make_shared<ImageEntry>();
    entry->key = key;
    entry->state = ImageLoadState::Loading;

    ImageData imgData = loader_->loadFromMemory(data, size);
    if (!imgData.isValid()) {
        entry->state = ImageLoadState::Error;
        entry->errorMessage = "فشل فك ترميز الصورة";
        return entry;
    }

    entry->data = std::move(imgData);
    entry->state = ImageLoadState::Loaded;

    {
        std::lock_guard<std::mutex> lock(mutex_);
        memoryCache_->put(key, entry);
    }

    if (loadCb_) loadCb_(key, entry);
    return entry;
}

void ImageCacheManager::putImage(const std::string& key, const ImageData& data) {
    auto entry = std::make_shared<ImageEntry>();
    entry->key = key;
    entry->data = data;
    entry->state = ImageLoadState::Loaded;

    std::lock_guard<std::mutex> lock(mutex_);
    memoryCache_->put(key, entry);
}

void ImageCacheManager::removeImage(const std::string& key) {
    std::lock_guard<std::mutex> lock(mutex_);
    memoryCache_->remove(key);
}

void ImageCacheManager::clearMemoryCache() {
    std::lock_guard<std::mutex> lock(mutex_);
    memoryCache_->clear();
    cacheHits_ = 0;
    cacheMisses_ = 0;
}

void ImageCacheManager::clearAll() {
    clearMemoryCache();
}

size_t ImageCacheManager::memoryCacheSize() const {
    return memoryCache_->currentSize();
}

size_t ImageCacheManager::memoryCacheCount() const {
    return memoryCache_->entryCount();
}

} // namespace ui
} // namespace sad
