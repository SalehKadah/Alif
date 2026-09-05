/**
 * ═══════════════════════════════════════════════════════════════════════════════
 * ملف: image_cache.h
 * المسار: features/graphics/core/include/sad_ui/image_cache.h
 * ═══════════════════════════════════════════════════════════════════════════════
 *
 * نظام ذاكرة الصور المؤقتة (Image Cache System).
 *
 * يدعم:
 * - ذاكرة LRU (الأقل استخداماً يُزال أولاً)
 * - مسار ذاكرة على القرص (اختياري)
 * - تحميل غير متزامن مع placeholder
 * - إعدادات حجم الذاكرة والقرص
 * - تحويل ملف → بيانات خام (pixels)
 * - تغيير حجم عند التحميل (لتوفير الذاكرة)
 *
 * حقوق النشر (c) 2024-2026 فريق لغة ص
 * ═══════════════════════════════════════════════════════════════════════════════
 */

#ifndef SAD_UI_IMAGE_CACHE_H
#define SAD_UI_IMAGE_CACHE_H

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <unordered_map>
#include <list>
#include <cstdint>
#include <mutex>

namespace sad {
namespace ui {

// ═══════════════════════════════════════════════════════════════════════════════
// بيانات صورة
// ═══════════════════════════════════════════════════════════════════════════════

struct ImageData {
    int width = 0;
    int height = 0;
    int channels = 4;  ///< RGBA
    std::vector<uint8_t> pixels;

    bool isValid() const { return width > 0 && height > 0 && !pixels.empty(); }
    size_t sizeInBytes() const { return pixels.size(); }
};

// ═══════════════════════════════════════════════════════════════════════════════
// حالة التحميل
// ═══════════════════════════════════════════════════════════════════════════════

enum class ImageLoadState : uint8_t {
    NotLoaded,      ///< لم يُحمّل
    Loading,        ///< جاري التحميل
    Loaded,         ///< تم التحميل
    Error           ///< خطأ
};

struct ImageEntry {
    std::string key;
    ImageData data;
    ImageLoadState state = ImageLoadState::NotLoaded;
    std::string errorMessage;
    uint64_t lastAccess = 0;
    uint32_t accessCount = 0;
};

// ═══════════════════════════════════════════════════════════════════════════════
// إعدادات الذاكرة المؤقتة
// ═══════════════════════════════════════════════════════════════════════════════

struct ImageCacheConfig {
    size_t maxMemoryBytes = 100 * 1024 * 1024;  ///< 100 MB
    size_t maxDiskBytes = 500 * 1024 * 1024;     ///< 500 MB
    std::string diskCachePath;                    ///< مسار القرص (فارغ = معطل)
    int maxConcurrentLoads = 4;
    bool enableDiskCache = false;
    int maxWidth = 0;   ///< أقصى عرض (0 = بدون تحديد)
    int maxHeight = 0;  ///< أقصى ارتفاع
};

// ═══════════════════════════════════════════════════════════════════════════════
// ذاكرة LRU
// ═══════════════════════════════════════════════════════════════════════════════

class LRUCache {
public:
    explicit LRUCache(size_t maxSize);

    /// إضافة/تحديث
    void put(const std::string& key, std::shared_ptr<ImageEntry> entry);

    /// جلب (يحدّث الوصول)
    std::shared_ptr<ImageEntry> get(const std::string& key);

    /// حذف
    void remove(const std::string& key);

    /// تنظيف
    void clear();

    /// الحجم الحالي والحد الأقصى
    size_t currentSize() const { return currentSize_; }
    size_t maxSize() const { return maxSize_; }
    size_t entryCount() const { return cache_.size(); }

    /// هل يوجد؟
    bool contains(const std::string& key) const;

private:
    size_t maxSize_;
    size_t currentSize_ = 0;
    // القائمة المرتبطة: الأحدث في المقدمة
    std::list<std::string> lruOrder_;
    std::unordered_map<std::string, std::pair<std::shared_ptr<ImageEntry>,
                       std::list<std::string>::iterator>> cache_;

    void evict();
    void touchKey(const std::string& key);
};

// ═══════════════════════════════════════════════════════════════════════════════
// محمّل صور (Image Loader) — واجهة
// ═══════════════════════════════════════════════════════════════════════════════

class ImageLoader {
public:
    virtual ~ImageLoader() = default;

    /// تحميل متزامن من ملف
    virtual ImageData loadFromFile(const std::string& path) = 0;

    /// تحميل متزامن من ذاكرة
    virtual ImageData loadFromMemory(const uint8_t* data, size_t size) = 0;

    /// تغيير الحجم
    virtual ImageData resize(const ImageData& image, int newWidth, int newHeight) = 0;
};

/// محمّل افتراضي (placeholder بسيط)
class DefaultImageLoader : public ImageLoader {
public:
    ImageData loadFromFile(const std::string& path) override;
    ImageData loadFromMemory(const uint8_t* data, size_t size) override;
    ImageData resize(const ImageData& image, int newWidth, int newHeight) override;
};

// ═══════════════════════════════════════════════════════════════════════════════
// مدير ذاكرة الصور (Image Cache Manager)
// ═══════════════════════════════════════════════════════════════════════════════

class ImageCacheManager {
public:
    static ImageCacheManager& instance();

    /// الإعدادات
    void configure(const ImageCacheConfig& config);
    const ImageCacheConfig& config() const { return config_; }

    /// تعيين المُحمّل
    void setLoader(std::shared_ptr<ImageLoader> loader) { loader_ = loader; }

    /// تحميل صورة (يرجع من الذاكرة أو يحمّل)
    std::shared_ptr<ImageEntry> getImage(const std::string& key);

    /// تحميل من ملف وتخزين
    std::shared_ptr<ImageEntry> loadFromFile(const std::string& path,
                                              const std::string& key = "");

    /// تحميل من ذاكرة وتخزين
    std::shared_ptr<ImageEntry> loadFromMemory(const std::string& key,
                                                const uint8_t* data, size_t size);

    /// تخزين صورة جاهزة
    void putImage(const std::string& key, const ImageData& data);

    /// حذف
    void removeImage(const std::string& key);

    /// تنظيف كل الذاكرة
    void clearMemoryCache();
    void clearAll();

    /// إحصائيات
    size_t memoryCacheSize() const;
    size_t memoryCacheCount() const;
    size_t cacheHits() const { return cacheHits_; }
    size_t cacheMisses() const { return cacheMisses_; }

    /// مستمع اكتمال التحميل
    using LoadCallback = std::function<void(const std::string& key, std::shared_ptr<ImageEntry>)>;
    void onImageLoaded(LoadCallback cb) { loadCb_ = cb; }

private:
    ImageCacheManager();
    ImageCacheConfig config_;
    std::unique_ptr<LRUCache> memoryCache_;
    std::shared_ptr<ImageLoader> loader_;
    std::mutex mutex_;

    size_t cacheHits_ = 0;
    size_t cacheMisses_ = 0;
    LoadCallback loadCb_;
};

} // namespace ui
} // namespace sad

#endif // SAD_UI_IMAGE_CACHE_H
