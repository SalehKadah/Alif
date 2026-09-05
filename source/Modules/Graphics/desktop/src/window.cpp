/**
 * ═══════════════════════════════════════════════════════════════════════════════
 * ملف: window.cpp
 * المسار: features/graphics/backends/desktop/src/window.cpp
 * ═══════════════════════════════════════════════════════════════════════════════
 *
 * تنفيذ نافذة سطح المكتب وحلقة الأحداث.
 *
 * سير العمل:
 * 1. create() → ينشئ نافذة SDL2 + Renderer
 * 2. setContent() → يعيّن شجرة IR
 * 3. run() → يدخل حلقة الأحداث:
 *    a. processEvents() → يقرأ أحداث SDL2
 *    b. updateLayout() → يحسب التخطيط (إذا تغيّر شيء)
 *    c. renderFrame() → يرسم الإطار
 *
 * ═══════════════════════════════════════════════════════════════════════════════
 * تم التفعيل الكامل — SDL2 مربوط ويعمل
 * ═══════════════════════════════════════════════════════════════════════════════
 *
 * حقوق النشر (c) 2024-2026 فريق لغة ص
 * ═══════════════════════════════════════════════════════════════════════════════
 */

#include "sad_ui/desktop/window.h"
#include "sad_ui/event_dispatch.h" // (AR) إرسال الأحداث بأطواره — مصدر الحقيقة المشترك
#include "sad_ui/prop_keys.h" // مفاتيح الخصائص القانونيّة (SoT) — لا literals خام
#include "sad_ui/window_control.h" // (م-تحكّم) استهلاك عمليّات النافذة المُعلَّقة (عنوان/إغلاق)

#ifdef SAD_UI_USE_SDL2
#include <SDL.h>
#endif

#include "text_editing.h"

#include <iostream>
#include <chrono>
#include <thread>
#include <functional>
#include <cmath>

namespace sad
{
    namespace ui
    {
        namespace desktop
        {

            // ─── تخزين SDL_AudioSpec بدون تعريضه في الـ header ──────────────
            struct DesktopWindow::AudioSpecStorage
            {
#ifdef SAD_UI_USE_SDL2
                SDL_AudioSpec spec;
#endif
            };

            // ═══════════════════════════════════════════════════════════════════════════════
            // تنفيذ DesktopWindow
            // ═══════════════════════════════════════════════════════════════════════════════

            DesktopWindow::DesktopWindow()
                : renderer_(std::make_unique<DesktopRenderer>()), layoutEngine_(std::make_unique<LayoutEngine>()), textEditStates_(std::make_unique<TextEditStateMap>()), audioSpecStorage_(std::make_unique<AudioSpecStorage>())
            {
            }

            DesktopWindow::~DesktopWindow()
            {
                destroy();
            }

            bool DesktopWindow::create(const WindowOptions &options)
            {
                options_ = options;
                width_ = options.width;
                height_ = options.height;

                // إعداد اتجاه التخطيط (RTL للعربية)
                layoutEngine_->setDirection(options.direction);
                layoutEngine_->setViewportSize(
                    static_cast<float>(width_),
                    static_cast<float>(height_));

#ifdef SAD_UI_USE_SDL2
                // تهيئة SDL2
                if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) < 0)
                {
                    std::cerr << "خطأ تهيئة SDL2: " << SDL_GetError() << std::endl;
                    return false;
                }

                // ─── إعدادات أحداث اللمس (Touch Event Hints) ───
                // (AR) منع SDL من تحويل أحداث اللمس إلى أحداث فأرة مزدوجة
                //      هذا يمنع الازدواجية: SDL_FINGERDOWN + SDL_MOUSEBUTTONDOWN
                //      الآن: اللمس يُرسل SDL_FINGER* فقط، والفأرة تُرسل SDL_MOUSE* فقط
                // (EN) Prevent SDL from synthesizing mouse events from touch events
                //      This avoids double-firing: SDL_FINGERDOWN + SDL_MOUSEBUTTONDOWN
                SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "0");

                // (AR) السماح بأحداث الفأرة العادية (من فأرة حقيقية) — لا تُحوّل لأحداث لمس
                // (EN) Don't synthesize touch events from mouse events either
                SDL_SetHint(SDL_HINT_MOUSE_TOUCH_EVENTS, "0");

                // إعداد flags النافذة
                uint32_t flags = SDL_WINDOW_SHOWN;
                if (options.resizable)
                    flags |= SDL_WINDOW_RESIZABLE;
                if (options.fullscreen)
                    flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
                if (options.borderless)
                    flags |= SDL_WINDOW_BORDERLESS;
                if (options.highDPI)
                    flags |= SDL_WINDOW_ALLOW_HIGHDPI;

                // إنشاء النافذة
                sdlWindow_ = SDL_CreateWindow(
                    options.title.c_str(),
                    SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                    width_, height_, flags);

                if (!sdlWindow_)
                {
                    std::cerr << "خطأ إنشاء النافذة: " << SDL_GetError() << std::endl;
                    SDL_Quit();
                    return false;
                }

                // إنشاء Renderer
                sdlRenderer_ = SDL_CreateRenderer(sdlWindow_, -1,
                                                  SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);

                if (!sdlRenderer_)
                {
                    std::cerr << "خطأ إنشاء Renderer: " << SDL_GetError() << std::endl;
                    SDL_DestroyWindow(sdlWindow_);
                    sdlWindow_ = nullptr;
                    SDL_Quit();
                    return false;
                }

                // تهيئة المُصيّر
                RenderOptions renderOpts;
                renderOpts.antialiasing = true;
                renderOpts.vsync = true;

                // حساب معامل DPI
                if (options.highDPI)
                {
                    int drawW = 0, drawH = 0;
                    SDL_GL_GetDrawableSize(sdlWindow_, &drawW, &drawH);
                    int winW = 0, winH = 0;
                    SDL_GetWindowSize(sdlWindow_, &winW, &winH);
                    if (winW > 0 && drawW > 0)
                    {
                        renderOpts.scaleFactor = static_cast<float>(drawW) / static_cast<float>(winW);
                    }
                }

                if (!renderer_->initialize(sdlRenderer_, renderOpts))
                {
                    std::cerr << "خطأ تهيئة المُصيّر" << std::endl;
                    SDL_DestroyRenderer(sdlRenderer_);
                    SDL_DestroyWindow(sdlWindow_);
                    sdlRenderer_ = nullptr;
                    sdlWindow_ = nullptr;
                    SDL_Quit();
                    return false;
                }

                // تحميل خط افتراضي — يدعم جميع المنصات
#ifdef _WIN32
                renderer_->loadFont("C:/Windows/Fonts/arial.ttf", 16.0f);
#elif defined(__APPLE__)
                renderer_->loadFont("/System/Library/Fonts/Helvetica.ttc", 16.0f);
#else
                renderer_->loadFont("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", 16.0f);
#endif

                std::cout << "✓ تم إنشاء النافذة: " << options.title
                          << " [" << width_ << "x" << height_ << "]" << std::endl;
#endif

                isOpen_ = true;

                // ─── تهيئة معالج أحداث اللمس الموحد ───
                touchProcessor_.setViewportSize(width_, height_);
                touchProcessor_.setGetContentRootCallback(
                    [this]() -> const IRNode *
                    { return contentRoot_.get(); });
                touchProcessor_.setHitTestCallback(
                    [this](float x, float y) -> const IRNode *
                    { return hitTest(x, y); });
                touchProcessor_.setFireEventCallback(
                    [this](IREventType type, const std::string &expr,
                           const IRNode *node, const EventData &data)
                    { fireEvent(type, expr, node, data); });
                touchProcessor_.setGetTimeMsCallback(
                    []() -> uint32_t
                    {
#ifdef SAD_UI_USE_SDL2
                        return SDL_GetTicks();
#else
                        return PlatformWindow::getCurrentTimeMs();
#endif
                    });

                // ─── تهيئة معالج أحداث لوحة المفاتيح الموحد ───
                kbProcessor_.setFireEventCallback(
                    [this](IREventType type, const std::string &expr,
                           const IRNode *node, const EventData &data)
                    { fireEvent(type, expr, node, data); });
                kbProcessor_.setGetFocusedNodeCallback(
                    [this]() -> const IRNode *
                    { return focusedNode_; });
                kbProcessor_.setGetContentRootCallback(
                    [this]() -> const IRNode *
                    { return contentRoot_.get(); });
                kbProcessor_.setCollectFocusableNodesCallback(
                    [this](std::vector<const IRNode *> &out)
                    { collectFocusableNodes(contentRoot_.get(), out); });
                kbProcessor_.setSetFocusedNodeCallback(
                    [this](const IRNode *node)
                    {
#ifdef SAD_UI_USE_SDL2
                        // (AR) إيقاف إدخال النص للعقدة السابقة
                        if (focusedNode_ && isTextInputNodeType(focusedNode_))
                            SDL_StopTextInput();
#endif
                        focusedNode_ = node;
#ifdef SAD_UI_USE_SDL2
                        // (AR) تشغيل إدخال النص للعقدة الجديدة
                        if (node && isTextInputNodeType(node))
                            SDL_StartTextInput();
#endif
                    });
                kbProcessor_.setInvalidateCallback(
                    [this]()
                    { invalidate(); });
                kbProcessor_.setCloseWindowCallback(
                    [this]()
                    { isOpen_ = false; });
                kbProcessor_.setHandleTextEditKeyCallback(
                    [this](const IRNode *node, UnifiedKeyCode keyCode,
                           const KeyModifiers &mods) -> bool
                    {
                        // (AR) تفويض معالجة تحرير النصوص لـ handleTextEditKeyEvent
                        //      المعالج المحلي يستخدم SDL keysym مباشرة
                        //      هنا نُحوّل UnifiedKeyCode إلى SDL keysym
                        return handleTextEditKeyViaProcessor(node, keyCode, mods);
                    });
                kbProcessor_.setHandleTextInputCallback(
                    [this](const IRNode *node, const std::string &text) -> bool
                    {
                        return handleTextInputViaProcessor(node, text);
                    });

                // ─── تهيئة معالج أحداث الفأرة الموحد ───
                mouseProcessor_.setHitTestCallback(
                    [this](float x, float y) -> const IRNode *
                    { return hitTest(x, y); });
                mouseProcessor_.setFireEventCallback(
                    [this](IREventType type, const std::string &expr,
                           const IRNode *node, const EventData &data)
                    { fireEvent(type, expr, node, data); });
                mouseProcessor_.setGetTimeMsCallback(
                    [this]() -> uint32_t
                    { return SDL_GetTicks(); });
                // (AR) حالة مفاتيح التعديل الحيّة ⇒ حقول مفتاح_عالي/تحكم/بديل في
                //      «حدث» صارت تُملأ لأحداث الفأرة (كانت أصفارًا دائمًا).
                mouseProcessor_.setGetKeyModifiersCallback(
                    []() -> KeyModifiers
                    {
                        const SDL_Keymod state = SDL_GetModState();
                        KeyModifiers mods;
                        mods.shift = (state & KMOD_SHIFT) != 0;
                        mods.ctrl = (state & KMOD_CTRL) != 0;
                        mods.alt = (state & KMOD_ALT) != 0;
                        mods.meta = (state & KMOD_GUI) != 0;
                        return mods;
                    });
                mouseProcessor_.setInvalidateCallback(
                    [this]()
                    { invalidate(); });
                mouseProcessor_.setGetFocusedNodeCallback(
                    [this]() -> const IRNode *
                    { return focusedNode_; });
                mouseProcessor_.setSetFocusedNodeCallback(
                    [this](const IRNode *node)
                    {
                        const IRNode *prev = focusedNode_;
                        focusedNode_ = node;
                        // (AR) تفعيل/تعطيل إدخال النص عند تغيير التركيز
                        if (node && node->getType() == UINodeType::TextField)
                            SDL_StartTextInput();
                        else if (prev && prev->getType() == UINodeType::TextField)
                            SDL_StopTextInput();
                    });
                mouseProcessor_.setFindNodeRectCallback(
                    [this](const IRNode *node) -> LayoutRect
                    { return findNodeRect(node); });
                mouseProcessor_.setFindScrollableAtCallback(
                    [this](float x, float y) -> ScrollableHitInfo
                    {
                        auto sh = findScrollableAt(x, y);
                        ScrollableHitInfo info;
                        info.node = sh.node;
                        if (sh.layout)
                        {
                            info.viewHeight = sh.layout->rect.height;
                            info.contentHeight = sh.layout->contentHeight;
                            info.currentScrollOffset = renderer_->getScrollOffset(sh.node);
                        }
                        return info;
                    });
                mouseProcessor_.setSetScrollOffsetCallback(
                    [this](const IRNode *node, float offset)
                    { renderer_->setScrollOffset(node, offset); });
                mouseProcessor_.setGetContentRootCallback(
                    [this]() -> const IRNode *
                    { return contentRoot_.get(); });
                mouseProcessor_.setSetCursorCallback(
                    [](int cursorType)
                    {
                        if (cursorType == 1)
                            SDL_SetCursor(SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_HAND));
                        else
                            SDL_SetCursor(SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_ARROW));
                    });

                return true;
            }

            void DesktopWindow::destroy()
            {
                renderer_->shutdown();

#ifdef SAD_UI_USE_SDL2
                if (sdlRenderer_)
                {
                    SDL_DestroyRenderer(sdlRenderer_);
                    sdlRenderer_ = nullptr;
                }
                if (sdlWindow_)
                {
                    SDL_DestroyWindow(sdlWindow_);
                    sdlWindow_ = nullptr;
                }
                SDL_Quit();
#endif

                isOpen_ = false;
            }

            void DesktopWindow::setContent(std::shared_ptr<IRNode> root)
            {
                // (AR) استبدال الشجرة يُحرّر عقدها القديمة، ومعالجا الفأرة/اللمس
                //      يحتفظان بمؤشّرات خام إليها (سحب/ضغط/تحويم/منزلق) ⇒ نُصفّرها
                //      قبل التحرير منعًا لاستعمال-بعد-التحرير.
                mouseProcessor_.clearNodeRefs();
                touchProcessor_.reset(); // (AR) FingerState.dragNode مؤشّر خام كذلك
                contentRoot_ = std::move(root);
                needsRedraw_ = true;

                // (AR) تعيين معرّف ثابت للعقدة الجذر — لدعم تحريك الخروج في Page Transitions
                if (contentRoot_ && contentRoot_->getId().empty())
                {
                    contentRoot_->setId("__page_root__");
                }

                // تهيئة تحريكات IRAnimation → AnimationEngine
                if (renderer_ && contentRoot_)
                {
                    renderer_->setAnimationEngine(&animationEngine_);
                    renderer_->resetAnimationState();
#ifdef SAD_UI_USE_SDL2
                    renderer_->initializeAnimations(contentRoot_, SDL_GetTicks());
#else
                    renderer_->initializeAnimations(contentRoot_, 0);
#endif
                }
            }

            void DesktopWindow::setStateStore(std::shared_ptr<StateStore> store)
            {
                stateStore_ = std::move(store);
            }

            void DesktopWindow::applyPatches(size_t patchedNodes, bool needsRelayout)
            {
                // (AR) تحديث جزئي — الشجرة contentRoot_ تم تعديلها موضعياً بالفعل
                //      نحتاج فقط:
                //      1. إعادة تهيئة التحريكات للعقد الجديدة فقط
                //      2. إعادة التخطيط إذا تغيرت الأبعاد أو الأبناء
                //      3. طلب إعادة رسم
                // (EN) Partial update — contentRoot_ already patched in-place.
                //      We only need to reinit animations for new nodes,
                //      relayout if dimensions/children changed, and request redraw.

                if (!contentRoot_)
                    return;

                // (AR) تهيئة تحريكات العقد الجديدة فقط
                //      initializeAnimations تفحص التحريكات الموجودة ولا تُعيد تهيئة المُشغّلة
                if (needsRelayout && renderer_)
                {
#ifdef SAD_UI_USE_SDL2
                    renderer_->initializeAnimations(contentRoot_, SDL_GetTicks());
#else
                    renderer_->initializeAnimations(contentRoot_, 0);
#endif
                }

                needsRedraw_ = true;
            }

            // ═══════════════════════════════════════════════════════════════════
            // تطبيقات واجهة PlatformWindow الجديدة
            // ═══════════════════════════════════════════════════════════════════

            void DesktopWindow::close()
            {
#ifdef SAD_UI_USE_SDL2
                // (AR) إرسال حدث إغلاق SDL لإنهاء حلقة الأحداث بشكل نظيف
                SDL_Event quitEvent;
                quitEvent.type = SDL_QUIT;
                SDL_PushEvent(&quitEvent);
#endif
            }

            void DesktopWindow::setTitle(const std::string &title)
            {
#ifdef SAD_UI_USE_SDL2
                if (sdlWindow_)
                {
                    SDL_SetWindowTitle(sdlWindow_, title.c_str());
                }
#else
                (void)title;
#endif
            }

            void DesktopWindow::setFullscreen(bool fullscreen)
            {
#ifdef SAD_UI_USE_SDL2
                if (sdlWindow_)
                {
                    if (fullscreen)
                    {
                        SDL_SetWindowFullscreen(sdlWindow_, SDL_WINDOW_FULLSCREEN_DESKTOP);
                    }
                    else
                    {
                        SDL_SetWindowFullscreen(sdlWindow_, 0);
                    }
                }
#else
                (void)fullscreen;
#endif
            }

            void DesktopWindow::showAlertDialog(const std::string &title, const std::string &message)
            {
#ifdef SAD_UI_USE_SDL2
                SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION,
                                         title.c_str(), message.c_str(),
                                         sdlWindow_);
#else
                std::cout << "[" << title << "] " << message << std::endl;
#endif
            }

            bool DesktopWindow::showConfirmDialog(const std::string &title, const std::string &message)
            {
#ifdef SAD_UI_USE_SDL2
                const SDL_MessageBoxButtonData buttons[] = {
                    {SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT, 1,
                     "\xd9\x85\xd9\x88\xd8\xa7\xd9\x81\xd9\x82"}, // موافق
                    {SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT, 0,
                     "\xd8\xa5\xd9\x84\xd8\xba\xd8\xa7\xd8\xa1"}, // إلغاء
                };
                SDL_MessageBoxData data = {};
                data.flags = SDL_MESSAGEBOX_INFORMATION;
                data.window = sdlWindow_;
                data.title = title.c_str();
                data.message = message.c_str();
                data.numbuttons = 2;
                data.buttons = buttons;

                int buttonId = 0;
                SDL_ShowMessageBox(&data, &buttonId);
                return buttonId == 1;
#else
                std::cout << "[" << title << "] " << message << std::endl;
                return true;
#endif
            }

            float DesktopWindow::getDisplayScale() const
            {
#ifdef SAD_UI_USE_SDL2
                if (sdlWindow_)
                {
                    int displayIndex = SDL_GetWindowDisplayIndex(sdlWindow_);
                    float ddpi = 0.0f;
                    if (SDL_GetDisplayDPI(displayIndex, &ddpi, nullptr, nullptr) == 0 && ddpi > 0)
                    {
                        return ddpi / 96.0f; // 96 DPI = 1.0x scale
                    }
                }
#endif
                return 1.0f;
            }

            bool DesktopWindow::takeScreenshot(const std::string &outputPath)
            {
#ifdef SAD_UI_USE_SDL2
                if (!sdlWindow_)
                    return false;
                SDL_Renderer *renderer = SDL_GetRenderer(sdlWindow_);
                if (!renderer)
                    return false;

                int w = 0, h = 0;
                SDL_GetRendererOutputSize(renderer, &w, &h);
                if (w <= 0 || h <= 0)
                    return false;

                SDL_Surface *surface = SDL_CreateRGBSurfaceWithFormat(0, w, h, 32, SDL_PIXELFORMAT_RGBA32);
                if (!surface)
                    return false;

                if (SDL_RenderReadPixels(renderer, nullptr, SDL_PIXELFORMAT_RGBA32,
                                         surface->pixels, surface->pitch) != 0)
                {
                    SDL_FreeSurface(surface);
                    return false;
                }

                int result = SDL_SaveBMP(surface, outputPath.c_str());
                SDL_FreeSurface(surface);

                if (result == 0)
                {
                    std::cout << "[\xd9\x84\xd9\x82\xd8\xb7\xd8\xa9] \xd8\xaa\xd9\x85 \xd8\xa7\xd9\x84\xd8\xad\xd9\x81\xd8\xb8: "
                              << outputPath << std::endl;
                }
                return result == 0;
#else
                (void)outputPath;
                return false;
#endif
            }

            uint32_t DesktopWindow::getTicksMs() const
            {
#ifdef SAD_UI_USE_SDL2
                return SDL_GetTicks();
#else
                auto now = std::chrono::steady_clock::now();
                return static_cast<uint32_t>(
                    std::chrono::duration_cast<std::chrono::milliseconds>(
                        now.time_since_epoch())
                        .count());
#endif
            }

            bool DesktopWindow::copyToClipboard(const std::string &text)
            {
#ifdef SAD_UI_USE_SDL2
                return SDL_SetClipboardText(text.c_str()) == 0;
#else
                (void)text;
                return false;
#endif
            }

            std::string DesktopWindow::pasteFromClipboard()
            {
#ifdef SAD_UI_USE_SDL2
                char *clip = SDL_GetClipboardText();
                if (clip)
                {
                    std::string result(clip);
                    SDL_free(clip);
                    return result;
                }
#endif
                return "";
            }

            std::pair<int, int> DesktopWindow::getMousePosition() const
            {
#ifdef SAD_UI_USE_SDL2
                int x = 0, y = 0;
                SDL_GetMouseState(&x, &y);
                return {x, y};
#else
                return {0, 0};
#endif
            }

            void DesktopWindow::setCursorStyle(const std::string &style)
            {
#ifdef SAD_UI_USE_SDL2
                SDL_SystemCursor cursorType = SDL_SYSTEM_CURSOR_ARROW;
                if (style == "hand" || style == "\xd9\x8a\xd8\xaf")
                    cursorType = SDL_SYSTEM_CURSOR_HAND;
                else if (style == "wait" || style == "\xd8\xa7\xd9\x86\xd8\xaa\xd8\xb8\xd8\xa7\xd8\xb1")
                    cursorType = SDL_SYSTEM_CURSOR_WAIT;
                else if (style == "text" || style == "\xd9\x86\xd8\xb5")
                    cursorType = SDL_SYSTEM_CURSOR_IBEAM;
                else if (style == "move" || style == "\xd8\xaa\xd8\xad\xd8\xb1\xd9\x8a\xd9\x83")
                    cursorType = SDL_SYSTEM_CURSOR_SIZEALL;
                else if (style == "no" || style == "\xd9\x85\xd9\x85\xd9\x86\xd9\x88\xd8\xb9")
                    cursorType = SDL_SYSTEM_CURSOR_NO;
                else if (style == "resize" || style == "\xd8\xaa\xd8\xba\xd9\x8a\xd9\x8a\xd8\xb1_\xd8\xad\xd8\xac\xd9\x85")
                    cursorType = SDL_SYSTEM_CURSOR_SIZENWSE;

                SDL_Cursor *cursor = SDL_CreateSystemCursor(cursorType);
                if (cursor)
                    SDL_SetCursor(cursor);
#else
                (void)style;
#endif
            }

            void DesktopWindow::showCursor(bool visible)
            {
#ifdef SAD_UI_USE_SDL2
                SDL_ShowCursor(visible ? SDL_ENABLE : SDL_DISABLE);
#else
                (void)visible;
#endif
            }

            int DesktopWindow::getCPUCount() const
            {
#ifdef SAD_UI_USE_SDL2
                return SDL_GetCPUCount();
#else
                return 1;
#endif
            }

            int DesktopWindow::getRAMSizeMB() const
            {
#ifdef SAD_UI_USE_SDL2
                return SDL_GetSystemRAM();
#else
                return 0;
#endif
            }

            int DesktopWindow::getScreenWidth() const
            {
#ifdef SAD_UI_USE_SDL2
                SDL_DisplayMode dm;
                if (SDL_GetCurrentDisplayMode(0, &dm) == 0)
                    return dm.w;
#endif
                return 1920;
            }

            int DesktopWindow::getScreenHeight() const
            {
#ifdef SAD_UI_USE_SDL2
                SDL_DisplayMode dm;
                if (SDL_GetCurrentDisplayMode(0, &dm) == 0)
                    return dm.h;
#endif
                return 1080;
            }

            double DesktopWindow::getPixelDensity() const
            {
#ifdef SAD_UI_USE_SDL2
                float dpi = 96.0f;
#if SDL_VERSION_ATLEAST(2, 0, 4)
                SDL_GetDisplayDPI(0, &dpi, nullptr, nullptr);
#endif
                return static_cast<double>(dpi);
#else
                return 96.0;
#endif
            }

            double DesktopWindow::getSystemFontScale() const
            {
#ifdef SAD_UI_USE_SDL2
                float dpi = 96.0f;
#if SDL_VERSION_ATLEAST(2, 0, 4)
                SDL_GetDisplayDPI(0, &dpi, nullptr, nullptr);
#endif
                return static_cast<double>(dpi / 96.0f);
#else
                return 1.0;
#endif
            }

            void DesktopWindow::vibrate(int durationMs)
            {
#ifdef SAD_UI_USE_SDL2
                SDL_Haptic *haptic = SDL_HapticOpen(0);
                if (haptic)
                {
                    if (SDL_HapticRumbleInit(haptic) == 0)
                    {
                        SDL_HapticRumblePlay(haptic, 0.75f, static_cast<uint32_t>(durationMs));
                        // ملاحظة: لا نستخدم SDL_Delay هنا لأنه يحجب الخيط
                    }
                    SDL_HapticClose(haptic);
                }
#else
                (void)durationMs;
#endif
            }

            int DesktopWindow::getBatteryLevel() const
            {
#ifdef SAD_UI_USE_SDL2
                int pct = -1;
                SDL_GetPowerInfo(nullptr, &pct);
                return pct >= 0 ? pct : -1;
#else
                return -1;
#endif
            }

            bool DesktopWindow::isBatteryCharging() const
            {
#ifdef SAD_UI_USE_SDL2
                SDL_PowerState state = SDL_GetPowerInfo(nullptr, nullptr);
                return state == SDL_POWERSTATE_CHARGING || state == SDL_POWERSTATE_CHARGED;
#else
                return false;
#endif
            }

            // ─── نظام الصوت (Audio System — SDL2) ─────────────────────────────

            void DesktopWindow::initAudio()
            {
#ifdef SAD_UI_USE_SDL2
                if (audioInitialized_)
                    return;
                if (SDL_InitSubSystem(SDL_INIT_AUDIO) < 0)
                {
                    std::cerr << "[\xd8\xb5\xd9\x88\xd8\xaa] \xd9\x81\xd8\xb4\xd9\x84 SDL_INIT_AUDIO: "
                              << SDL_GetError() << std::endl;
                    return;
                }

                SDL_AudioSpec desired;
                SDL_memset(&desired, 0, sizeof(desired));
                desired.freq = 44100;
                desired.format = AUDIO_S16LSB;
                desired.channels = 2;
                desired.samples = 4096;
                desired.callback = &DesktopWindow::audioCallback;
                desired.userdata = this;

                audioDevice_ = SDL_OpenAudioDevice(nullptr, 0, &desired, &audioSpecStorage_->spec, SDL_AUDIO_ALLOW_FORMAT_CHANGE);
                if (audioDevice_ == 0)
                {
                    std::cerr << "[\xd8\xb5\xd9\x88\xd8\xaa] \xd9\x81\xd8\xb4\xd9\x84 \xd9\x81\xd8\xaa\xd8\xad \xd8\xac\xd9\x87\xd8\xa7\xd8\xb2 \xd8\xa7\xd9\x84\xd8\xb5\xd9\x88\xd8\xaa: "
                              << SDL_GetError() << std::endl;
                    return;
                }

                SDL_PauseAudioDevice(audioDevice_, 0);
                audioInitialized_ = true;
#endif
            }

            void DesktopWindow::cleanupAudio()
            {
#ifdef SAD_UI_USE_SDL2
                if (!audioInitialized_)
                    return;
                SDL_PauseAudioDevice(audioDevice_, 1);
                SDL_CloseAudioDevice(audioDevice_);
                for (auto &s : activeSounds_)
                {
                    if (s.buffer)
                        SDL_FreeWAV(s.buffer);
                }
                activeSounds_.clear();
                audioInitialized_ = false;
#endif
            }

            void DesktopWindow::audioCallback(void *userdata, uint8_t *stream, int len)
            {
#ifdef SAD_UI_USE_SDL2
                auto *window = static_cast<DesktopWindow *>(userdata);
                SDL_memset(stream, 0, static_cast<size_t>(len));

                for (auto &s : window->activeSounds_)
                {
                    if (!s.playing || s.position >= s.length)
                        continue;

                    uint32_t remaining = s.length - s.position;
                    uint32_t toMix = (static_cast<uint32_t>(len) < remaining) ? static_cast<uint32_t>(len) : remaining;
                    int volume = static_cast<int>(window->masterVolume_ * SDL_MIX_MAXVOLUME);
                    SDL_MixAudioFormat(stream, s.buffer + s.position, window->audioSpecStorage_->spec.format,
                                       toMix, volume);
                    s.position += toMix;

                    if (s.position >= s.length)
                    {
                        if (s.loop)
                        {
                            s.position = 0;
                        }
                        else
                        {
                            s.playing = false;
                        }
                    }
                }
#else
                (void)userdata;
                (void)stream;
                (void)len;
#endif
            }

            int DesktopWindow::playSound(const std::string &filePath, bool loop)
            {
#ifdef SAD_UI_USE_SDL2
                if (!audioInitialized_)
                    initAudio();
                if (!audioInitialized_)
                    return -1;

                SDL_AudioSpec wavSpec;
                uint8_t *wavBuffer = nullptr;
                uint32_t wavLength = 0;

                if (!SDL_LoadWAV(filePath.c_str(), &wavSpec, &wavBuffer, &wavLength))
                {
                    std::cerr << "[\xd8\xb5\xd9\x88\xd8\xaa] \xd9\x81\xd8\xb4\xd9\x84 \xd8\xaa\xd8\xad\xd9\x85\xd9\x8a\xd9\x84: "
                              << filePath << " - " << SDL_GetError() << std::endl;
                    return -1;
                }

                SDL_AudioCVT cvt;
                int cvtResult = SDL_BuildAudioCVT(&cvt, wavSpec.format, wavSpec.channels, wavSpec.freq,
                                                  audioSpecStorage_->spec.format, audioSpecStorage_->spec.channels, audioSpecStorage_->spec.freq);
                uint8_t *finalBuffer = wavBuffer;
                uint32_t finalLength = wavLength;

                if (cvtResult > 0)
                {
                    cvt.len = static_cast<int>(wavLength);
                    cvt.buf = static_cast<uint8_t *>(SDL_malloc(static_cast<size_t>(cvt.len * cvt.len_mult)));
                    SDL_memcpy(cvt.buf, wavBuffer, wavLength);
                    SDL_ConvertAudio(&cvt);
                    SDL_FreeWAV(wavBuffer);
                    finalBuffer = cvt.buf;
                    finalLength = static_cast<uint32_t>(cvt.len_cvt);
                }

                int id = nextSoundId_++;
                SoundEntry entry;
                entry.id = id;
                entry.buffer = finalBuffer;
                entry.length = finalLength;
                entry.position = 0;
                entry.loop = loop;
                entry.playing = true;

                SDL_LockAudioDevice(audioDevice_);
                activeSounds_.push_back(entry);
                SDL_UnlockAudioDevice(audioDevice_);

                return id;
#else
                (void)filePath;
                (void)loop;
                return -1;
#endif
            }

            void DesktopWindow::stopSound(int soundId)
            {
#ifdef SAD_UI_USE_SDL2
                if (!audioInitialized_)
                    return;
                SDL_LockAudioDevice(audioDevice_);
                for (auto &s : activeSounds_)
                {
                    if (s.id == soundId)
                    {
                        s.playing = false;
                        break;
                    }
                }
                SDL_UnlockAudioDevice(audioDevice_);
#else
                (void)soundId;
#endif
            }

            void DesktopWindow::stopAllSounds()
            {
#ifdef SAD_UI_USE_SDL2
                if (!audioInitialized_)
                    return;
                SDL_LockAudioDevice(audioDevice_);
                for (auto &s : activeSounds_)
                {
                    s.playing = false;
                }
                SDL_UnlockAudioDevice(audioDevice_);
#endif
            }

            void DesktopWindow::setVolume(float volume)
            {
                masterVolume_ = (volume < 0.0f) ? 0.0f : (volume > 1.0f ? 1.0f : volume);
            }

            float DesktopWindow::getVolume() const
            {
                return masterVolume_;
            }

            void DesktopWindow::run()
            {
                // حلقة الأحداث الرئيسية
                const int TARGET_FPS = 60;
                const int FRAME_DELAY = 1000 / TARGET_FPS;

                while (isOpen_)
                {
#ifdef SAD_UI_USE_SDL2
                    Uint32 frameStart = SDL_GetTicks();
#endif

                    // 1. معالجة أحداث SDL2
                    processEvents();

                    // 1.3. (م-تحكّم) استهلاك عمليّات النافذة المُعلَّقة من المتحكّم المشترك:
                    //      كلا المحرّكين يكتبان إلى windowController()، وهذه الحلقة (المكتبة)
                    //      تستهلكها فتطبّقها على النافذة الفعليّة ⇒ سلوكٌ موحَّد بلا تفريعٍ
                    //      لكلّ محرّك. الإغلاق يُطبَّق بعد العنوان (لو طُلِبا معًا يُغلَق).
                    {
                        std::string pendingTitle;
                        if (windowController().takePendingTitle(pendingTitle))
                            setTitle(pendingTitle);
                        if (windowController().takeCloseRequest())
                            close();
                    }

                    // 1.5. تحديث المؤقتات
                    if (onTimerUpdateCallback_)
                    {
                        onTimerUpdateCallback_();
                    }

                    // 1.7. تحقق من انتقال صفحة مُعلّق (exit → entry)
                    if (checkPendingTransition())
                    {
                        needsRedraw_ = true;
                    }

                    // 2. تحديث الانيميشن
                    bool animating = animationEngine_.update(SDL_GetTicks());
                    if (animating)
                    {
                        needsRedraw_ = true;
                    }

                    // 3. إذا كان هناك TextField مركّز — إعادة رسم للمؤشر الوامض
                    if (focusedNode_ && focusedNode_->getType() == UINodeType::TextField)
                    {
                        needsRedraw_ = true; // المؤشر يومض كل 500ms
                    }

                    // 3. تحديث التخطيط إذا لزم الأمر
                    if (needsRedraw_)
                    {
                        updateLayout();
                        // 4. رسم الإطار فقط عند الحاجة
                        renderFrame();
                    }

#ifdef SAD_UI_USE_SDL2
                    // تحديد عدد الإطارات
                    Uint32 frameTime = SDL_GetTicks() - frameStart;
                    if (frameTime < FRAME_DELAY)
                    {
                        SDL_Delay(FRAME_DELAY - frameTime);
                    }
#endif
                }
            }

            // ─── دوال داخلية ────────────────────────────

            // ═══════════════════════════════════════════════════════════════════════════════
            // fireEvent — إطلاق حدث مع بيانات الحدث ودعم الفقاعات
            // ═══════════════════════════════════════════════════════════════════════════════
            //
            // (AR) نقطة الدخول الموحّدة لإطلاق كل الأحداث.
            //      - تملأ EventData وتستدعي onEventCallback_
            //      - إذا كان الحدث يدعم الفقاعات (Bubble)، يصعد عبر الأجداد
            //      - يمكن إيقاف الانتشار عبر EventData::stopPropagation()
            //
            // (EN) Unified entry point for firing all events.
            //      - Fills EventData and calls onEventCallback_
            //      - If event supports bubbling, propagates up through ancestors
            //      - Propagation can be stopped via EventData::stopPropagation()
            // ═══════════════════════════════════════════════════════════════════════════════
            void DesktopWindow::fireEvent(
                IREventType type,
                const std::string &expression,
                const IRNode *node,
                const EventData &data)
            {
                if (!onEventCallback_ || !node)
                    return;

                // (AR) الإرسال بأطواره الثلاثة صار في المكتبة المشتركة
                //      (sad_ui/event_dispatch.h) كي تسلكه كلّ المنصّات — لا
                //      نسخة خاصّة بسطح المكتب. المُصرِف هنا مجرّد مُنفِّذ.
                (void)expression; // الاختيار للمُرسِل: يمرّر تعبير كلّ معالِج
                dispatchEvent(type, node, data,
                              [this](IREventType t, const std::string &expr,
                                     const IRNode *n, const EventData &d)
                              { onEventCallback_(t, expr, n, d); });
            }

            // ═══════════════════════════════════════════════════════════════════════════════
            // dispatchCustomEvent — إطلاق حدث مخصص على عقدة
            // ═══════════════════════════════════════════════════════════════════════════════
            //
            // (AR) يبحث عن أحداث من نوع Custom بالاسم المطلوب ويُطلقها.
            //      يدعم الفقاعات إذا كان الحدث المخصص يدعمها.
            // ═══════════════════════════════════════════════════════════════════════════════
            void DesktopWindow::dispatchCustomEvent(
                const std::string &eventName,
                const IRNode *node,
                const EventData &data)
            {
                if (!onEventCallback_ || !node)
                    return;

                for (const auto &evt : node->getEvents())
                {
                    if (evt.type == IREventType::Custom && evt.customEventName == eventName)
                    {
                        onEventCallback_(IREventType::Custom, evt.expression, node, data);

                        // (AR) فقاعات الأحداث المخصصة
                        if (!data.propagationStopped &&
                            (evt.propagation == EventPropagation::Bubble ||
                             evt.propagation == EventPropagation::Both))
                        {
                            auto ancestors = node->getAncestorPath();
                            for (const auto *ancestor : ancestors)
                            {
                                if (data.propagationStopped)
                                    break;
                                for (const auto &aEvt : ancestor->getEvents())
                                {
                                    if (aEvt.type == IREventType::Custom &&
                                        aEvt.customEventName == eventName)
                                    {
                                        onEventCallback_(IREventType::Custom, aEvt.expression,
                                                         ancestor, data);
                                        break;
                                    }
                                }
                            }
                        }
                        break;
                    }
                }
            }

#ifdef SAD_UI_USE_SDL2
            // (AR) إعلانات مسبقة لدوال تحويل المفاتيح — التعريف بعد processEvents
            static UnifiedKeyCode sdlScancodeToUnified(int sdlScancode);
            static UnifiedKeyCode sdlKeysymToUnified(int keysym);
#endif

            void DesktopWindow::processEvents()
            {
#ifdef SAD_UI_USE_SDL2
                SDL_Event event;
                while (SDL_PollEvent(&event))
                {
                    switch (event.type)
                    {
                    case SDL_QUIT:
                        isOpen_ = false;
                        break;

                        // ═══════════════════════════════════════════════════════════════════
                        // (AR) أحداث الفأرة — مُفوَّضة إلى mouseProcessor_ الموحد
                        // ═══════════════════════════════════════════════════════════════════

                    case SDL_MOUSEBUTTONDOWN:
                    {
                        // (AR) تحويل زر SDL إلى MouseButton موحد
                        MouseButton btn = MouseButton::Unknown;
                        if (event.button.button == SDL_BUTTON_LEFT)
                            btn = MouseButton::Left;
                        else if (event.button.button == SDL_BUTTON_MIDDLE)
                            btn = MouseButton::Middle;
                        else if (event.button.button == SDL_BUTTON_RIGHT)
                            btn = MouseButton::Right;

                        float mx = static_cast<float>(event.button.x);
                        float my = static_cast<float>(event.button.y);

                        // (AR) تمرير الحدث للمعالج الموحد — يعالج:
                        //   ContextMenu (يمنى)، تحديث التركيز، Toggle/Checkbox/Radio/Slider، OnTap
                        mouseProcessor_.onMouseDown(btn, mx, my);

                        // (AR) إرسال UIEvent للمعالج الخارجي إن وُجد
                        UIEvent uiEvent;
                        uiEvent.type = UIEventType::Tap;
                        uiEvent.x = mx;
                        uiEvent.y = my;
                        if (desktopEventHandler_)
                            desktopEventHandler_(uiEvent);
                        break;
                    }

                    case SDL_MOUSEBUTTONUP:
                    {
                        // (AR) تحويل زر SDL إلى MouseButton موحد
                        MouseButton btn = MouseButton::Unknown;
                        if (event.button.button == SDL_BUTTON_LEFT)
                            btn = MouseButton::Left;
                        else if (event.button.button == SDL_BUTTON_MIDDLE)
                            btn = MouseButton::Middle;
                        else if (event.button.button == SDL_BUTTON_RIGHT)
                            btn = MouseButton::Right;

                        float mx = static_cast<float>(event.button.x);
                        float my = static_cast<float>(event.button.y);

                        // (AR) تمرير الحدث للمعالج الموحد — يعالج:
                        //   كشف الإيماءات (Swipe/LongPress/DoubleTap)، DragEnd/Drop، إنهاء Slider
                        mouseProcessor_.onMouseUp(btn, mx, my);

                        // (AR) UIEvent — لا نعرف نوع الإيماءة هنا (المعالج يعالجها داخلياً)
                        //      لكن نُرسل UIEvent أساسي للتوافقية الخلفية
                        break;
                    }

                    case SDL_MOUSEMOTION:
                    {
                        float mx = static_cast<float>(event.motion.x);
                        float my = static_cast<float>(event.motion.y);

                        // (AR) أيضاً نحدّث mouseX_/mouseY_ المحلية للتوافق مع أجزاء أخرى
                        mouseX_ = mx;
                        mouseY_ = my;

                        // (AR) بناء حالة الأزرار المضغوطة من SDL
                        MouseButtonState buttons;
                        buttons.leftPressed = (event.motion.state & SDL_BUTTON_LMASK) != 0;
                        buttons.middlePressed = (event.motion.state & SDL_BUTTON_MMASK) != 0;
                        buttons.rightPressed = (event.motion.state & SDL_BUTTON_RMASK) != 0;

                        // (AR) تمرير الحدث للمعالج الموحد — يعالج:
                        //   سحب Slider، Hover/HoverExit، DragStart/DragMove، تغيير المؤشر
                        mouseProcessor_.onMouseMove(mx, my, buttons);
                        break;
                    }

                    case SDL_MOUSEWHEEL:
                    {
                        // (AR) فحص هل Ctrl مضغوط (للتكبير/التصغير)
                        const Uint8 *keyState = SDL_GetKeyboardState(nullptr);
                        bool ctrlPressed = (keyState[SDL_SCANCODE_LCTRL] || keyState[SDL_SCANCODE_RCTRL]);

                        float deltaX = static_cast<float>(event.wheel.x);
                        float deltaY = static_cast<float>(event.wheel.y);

                        // (AR) تمرير الحدث للمعالج الموحد — يعالج:
                        //   Ctrl+Scroll → Zoom، تمرير ScrollView/LazyColumn، OnScroll
                        mouseProcessor_.onMouseWheel(deltaX, deltaY, mouseX_, mouseY_, ctrlPressed);

                        // (AR) إرسال UIEvent للمعالج الخارجي
                        UIEvent uiEvent;
                        uiEvent.type = UIEventType::Scroll;
                        uiEvent.scrollDelta = deltaY;
                        if (desktopEventHandler_)
                            desktopEventHandler_(uiEvent);
                        break;
                    }

                    case SDL_KEYDOWN:
                    {
                        // (AR) تمرير الحدث للمعالج الموحد
                        UIEvent uiEvent;
                        uiEvent.type = UIEventType::KeyPress;
                        uiEvent.keyCode = event.key.keysym.scancode;
                        if (desktopEventHandler_)
                            desktopEventHandler_(uiEvent);

                        UnifiedKeyCode uKey = sdlScancodeToUnified(event.key.keysym.scancode);
                        // (AR) بعض المفاتيح (Backspace, Delete, Arrows...) أفضل تحويلها
                        //      من keysym (وليس scancode) لأن scancode قد لا يتطابق
                        if (uKey == UnifiedKeyCode::Unknown)
                            uKey = sdlKeysymToUnified(event.key.keysym.sym);

                        std::string keyName = SDL_GetKeyName(event.key.keysym.sym);
                        KeyModifiers mods;
                        mods.shift = (event.key.keysym.mod & KMOD_SHIFT) != 0;
                        mods.ctrl = (event.key.keysym.mod & KMOD_CTRL) != 0;
                        mods.alt = (event.key.keysym.mod & KMOD_ALT) != 0;

                        kbProcessor_.onKeyDown(uKey, keyName, mods);
                        break;
                    }

                    case SDL_TEXTINPUT:
                    {
                        // (AR) تمرير إدخال النص للمعالج الموحد
                        UIEvent uiEvent;
                        uiEvent.type = UIEventType::TextInput;
                        uiEvent.text = event.text.text;

                        kbProcessor_.onTextInput(event.text.text);

                        if (desktopEventHandler_)
                            desktopEventHandler_(uiEvent);
                        break;
                    }

                    // ─── رفع مفتاح (KeyUp) ───────────────────────
                    case SDL_KEYUP:
                    {
                        // (AR) تمرير الحدث للمعالج الموحد
                        UnifiedKeyCode uKey = sdlScancodeToUnified(event.key.keysym.scancode);
                        if (uKey == UnifiedKeyCode::Unknown)
                            uKey = sdlKeysymToUnified(event.key.keysym.sym);

                        std::string keyName = SDL_GetKeyName(event.key.keysym.sym);
                        KeyModifiers mods;
                        mods.shift = (event.key.keysym.mod & KMOD_SHIFT) != 0;
                        mods.ctrl = (event.key.keysym.mod & KMOD_CTRL) != 0;
                        mods.alt = (event.key.keysym.mod & KMOD_ALT) != 0;

                        kbProcessor_.onKeyUp(uKey, keyName, mods);
                        break;
                    }

                    case SDL_WINDOWEVENT:
                        if (event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED)
                        {
                            width_ = event.window.data1;
                            height_ = event.window.data2;
                            layoutEngine_->setViewportSize(
                                static_cast<float>(width_),
                                static_cast<float>(height_));

                            // (AR) تحديث حجم العرض في معالج اللمس عند تغيير حجم النافذة
                            touchProcessor_.setViewportSize(width_, height_);

                            // ─── إطلاق عند_تغيير_الحجم (OnResize) على الجذر ───
                            if (contentRoot_)
                            {
                                EventData data;
                                data.x = static_cast<float>(width_);
                                data.y = static_cast<float>(height_);
                                // (AR) نداءٌ واحد: fireEvent صار يفوّض الاختيار والأطوار
                                //      لـdispatchEvent؛ المرور على getEvents هنا كان
                                //      يُطلق كلّ معالِجٍ مرّةً، وكلّ نداءٍ يُعيد اختيار
                                //      الجميع ⇒ إطلاقٌ مضاعف (N²).
                                fireEvent(IREventType::OnResize, std::string(),
                                          contentRoot_.get(), data);
                            }

                            invalidate();
                        }
                        else if (event.window.event == SDL_WINDOWEVENT_EXPOSED)
                        {
                            invalidate();
                        }
                        break;

                    // ─── إيماءة الدوران بأصبعين (OnRotate) ───
                    case SDL_MULTIGESTURE:
                    {
                        if (std::abs(event.mgesture.dTheta) > 0.01f)
                        {
                            float cx = event.mgesture.x * static_cast<float>(width_);
                            float cy = event.mgesture.y * static_cast<float>(height_);
                            const auto *rotateNode = hitTest(cx, cy);
                            if (rotateNode)
                            {
                                EventData data;
                                data.x = cx;
                                data.y = cy;
                                data.angle = event.mgesture.dTheta;
                                // (AR) نداءٌ واحد — تفويض الاختيار للمُرسِل (تجنّب N²).
                                fireEvent(IREventType::OnRotate, std::string(),
                                          rotateNode, data);
                            }
                        }
                        break;
                    }

                        // ═══════════════════════════════════════════════════════════════════════
                        // أحداث اللمس الأصلية (SDL_FINGER*) — تُعالج عبر TouchEventProcessor
                        // ═══════════════════════════════════════════════════════════════════════
                        //
                        // (AR) جميع منطق كشف الإيماءات (نقر، نقر مزدوج، ضغط مطوّل، سحب،
                        //      قرص/تكبير، دوران) مُنقل إلى الطبقة المشتركة TouchEventProcessor
                        //      في sad_ui/core/ لضمان سلوك موحد عبر جميع المنصات.
                        //
                        // (EN) All gesture detection logic (tap, double-tap, long-press, drag,
                        //      pinch-to-zoom, rotation) is delegated to the shared
                        //      TouchEventProcessor in sad_ui/core/ for cross-platform consistency.
                        // ═══════════════════════════════════════════════════════════════════════

                    case SDL_FINGERDOWN:
                    {
                        // (AR) إحداثيات اللمس عائمة بين 0.0 و 1.0 — نُحوّلها لبكسل
                        float tx = event.tfinger.x * static_cast<float>(width_);
                        float ty = event.tfinger.y * static_cast<float>(height_);
                        float tpressure = event.tfinger.pressure;
                        int64_t tId = static_cast<int64_t>(event.tfinger.touchId);
                        int64_t fId = static_cast<int64_t>(event.tfinger.fingerId);
                        touchProcessor_.onFingerDown(tId, fId, tx, ty, tpressure);
                        break;
                    }

                    case SDL_FINGERUP:
                    {
                        float tx = event.tfinger.x * static_cast<float>(width_);
                        float ty = event.tfinger.y * static_cast<float>(height_);
                        int64_t tId = static_cast<int64_t>(event.tfinger.touchId);
                        int64_t fId = static_cast<int64_t>(event.tfinger.fingerId);
                        touchProcessor_.onFingerUp(tId, fId, tx, ty);
                        break;
                    }

                    case SDL_FINGERMOTION:
                    {
                        float tx = event.tfinger.x * static_cast<float>(width_);
                        float ty = event.tfinger.y * static_cast<float>(height_);
                        float tpressure = event.tfinger.pressure;
                        float fdx = event.tfinger.dx * static_cast<float>(width_);
                        float fdy = event.tfinger.dy * static_cast<float>(height_);
                        int64_t tId = static_cast<int64_t>(event.tfinger.touchId);
                        int64_t fId = static_cast<int64_t>(event.tfinger.fingerId);
                        touchProcessor_.onFingerMotion(tId, fId, tx, ty, fdx, fdy, tpressure);
                        break;
                    }

                    case SDL_DROPFILE:
                    {
                        // ─── سحب وإفلات ملف (File Drop) ───
                        char *droppedFile = event.drop.file;
                        if (droppedFile)
                        {
                            if (onDropFileCallback_)
                            {
                                onDropFileCallback_(std::string(droppedFile));
                            }
                            SDL_free(droppedFile);
                        }
                        break;
                    }
                    break;
                    }
                }
#endif
            }

            void DesktopWindow::updateLayout()
            {
                if (!contentRoot_)
                    return;

                // تهيئة تحريكات العقد الجديدة (بعد إعادة بناء الشجرة عبر @حالة)
                if (renderer_)
                {
#ifdef SAD_UI_USE_SDL2
                    renderer_->initializeAnimations(contentRoot_, SDL_GetTicks());
#else
                    renderer_->initializeAnimations(contentRoot_, 0);
#endif
                }

                // حساب التخطيط
                LayoutConstraints constraints;
                constraints.maxWidth = static_cast<float>(width_);
                constraints.maxHeight = static_cast<float>(height_);

                layoutResult_ = layoutEngine_->layout(contentRoot_, constraints);
                needsRedraw_ = false;
            }

            void DesktopWindow::renderFrame()
            {
                if (!contentRoot_ || !layoutResult_)
                    return;

                // تمرير حالة التفاعل ومحرك الانيميشن للمُصيّر
                // (AR) نستخدم getters من mouseProcessor_ الموحد بدلاً من المتغيرات المحلية
                renderer_->setInteractionState(
                    mouseProcessor_.getHoveredNode(),
                    focusedNode_,
                    mouseProcessor_.getPressedNode());
                renderer_->setAnimationEngine(&animationEngine_);

                // تسجيل callback انتهاء التحريك → إطلاق OnAnimationEnd
                renderer_->setOnAnimationEndCallback(
                    [this](const std::string &nodeId, AnimationType animType)
                    {
                        if (!contentRoot_ || !onEventCallback_)
                            return;
                        // البحث عن العقدة بمعرّفها لإطلاق الحدث
                        std::function<const IRNode *(const IRNode *, const std::string &)> findNode;
                        findNode = [&findNode](const IRNode *n, const std::string &id) -> const IRNode *
                        {
                            if (!n)
                                return nullptr;
                            if (n->getId() == id)
                                return n;
                            for (auto &child : n->getChildren())
                            {
                                auto *found = findNode(child.get(), id);
                                if (found)
                                    return found;
                            }
                            return nullptr;
                        };
                        const IRNode *target = findNode(contentRoot_.get(), nodeId);
                        if (target)
                        {
                            EventData data;
                            data.value = animationTypeToString(animType);
                            fireEvent(IREventType::OnAnimationEnd,
                                      animationTypeToString(animType),
                                      target, data);
                        }
                    });

                // تمرير حالة تحرير النص إذا كان هناك حقل نص مركّز
                if (focusedNode_ && textEditStates_ &&
                    (focusedNode_->getType() == UINodeType::TextField ||
                     focusedNode_->getType() == UINodeType::TextArea))
                {
                    auto &editState = textEditStates_->getState(focusedNode_);
                    renderer_->setTextEditState(
                        editState.cursorPos,
                        editState.hasSelection ? editState.selectionStart : 0,
                        editState.hasSelection ? editState.selectionEnd : 0);
                }
                else
                {
                    renderer_->setTextEditState(0, 0, 0);
                }

                // رسم الإطار
                Color bgColor = Color::fromNamed(NamedColor::Background);
                renderer_->beginFrame(bgColor);
                renderer_->render(contentRoot_, layoutResult_);
                renderer_->endFrame();
            }

            const IRNode *DesktopWindow::hitTest(float x, float y) const
            {
                if (!contentRoot_ || !layoutResult_)
                    return nullptr;

                // بحث في شجرة التخطيط عن العنصر تحت (x, y)
                return hitTestNode(contentRoot_.get(), layoutResult_.get(), x, y);
            }

            const IRNode *DesktopWindow::hitTestNode(
                const IRNode *node,
                const LayoutResult *layout,
                float x, float y) const
            {
                if (!node || !layout)
                    return nullptr;

                const auto &rect = layout->rect;

                // التحقق من أن النقطة داخل هذا العنصر
                bool inside = (x >= rect.x && x < rect.x + rect.width &&
                               y >= rect.y && y < rect.y + rect.height);

                if (!inside)
                    return nullptr;

                // إذا كان العنصر قابلاً للتمرير، نعدّل Y لتعويض الإزاحة
                float childY = y;
                auto nodeType = node->getType();
                if (isScrollableType(nodeType))
                {
                    float scrollOff = renderer_->getScrollOffset(node);
                    childY = y - scrollOff; // scrollOff سالب، فنطرحه لنحصل على Y الحقيقي في المحتوى
                }

                // البحث في الأبناء (من الأخير للأول = الأعلى للأسفل)
                for (int i = static_cast<int>(layout->children.size()) - 1; i >= 0; --i)
                {
                    if (i < static_cast<int>(node->childCount()))
                    {
                        const IRNode *hit = hitTestNode(
                            node->getChildren()[i].get(),
                            layout->children[i].get(),
                            x, childY);
                        if (hit)
                            return hit;
                    }
                }

                // إرجاع هذا العنصر إذا كان تفاعلياً
                if (!node->getEvents().empty())
                {
                    return node;
                }

                // Toggle/Checkbox/Slider تفاعلية دائماً حتى بدون أحداث صريحة
                if (nodeType == UINodeType::Toggle || nodeType == UINodeType::Checkbox ||
                    nodeType == UINodeType::Slider || nodeType == UINodeType::Button)
                {
                    return node;
                }

                // إرجاع nullptr إذا كان حاوية بدون أحداث
                return nullptr;
            }

            // ─── البحث عن مستطيل عقدة IR في شجرة التخطيط ───

            LayoutRect DesktopWindow::findNodeRect(const IRNode *target) const
            {
                if (!contentRoot_ || !layoutResult_ || !target)
                    return {};
                return findNodeRectHelper(contentRoot_.get(), layoutResult_.get(), target);
            }

            LayoutRect DesktopWindow::findNodeRectHelper(
                const IRNode *node, const LayoutResult *layout, const IRNode *target) const
            {
                if (!node || !layout)
                    return {};
                if (node == target)
                    return layout->rect;

                size_t count = std::min(node->childCount(), layout->children.size());
                for (size_t i = 0; i < count; ++i)
                {
                    auto result = findNodeRectHelper(
                        node->getChildren()[i].get(),
                        layout->children[i].get(),
                        target);
                    if (result.width > 0)
                        return result;
                }
                return {};
            }

            // ─── البحث عن أقرب ScrollView يحتوي على نقطة ─────

            DesktopWindow::ScrollableHit DesktopWindow::findScrollableAt(float x, float y) const
            {
                if (!contentRoot_ || !layoutResult_)
                    return {};
                return findScrollableNode(contentRoot_.get(), layoutResult_.get(), x, y);
            }

            DesktopWindow::ScrollableHit DesktopWindow::findScrollableNode(
                const IRNode *node, const LayoutResult *layout, float x, float y) const
            {
                if (!node || !layout)
                    return {};

                const auto &rect = layout->rect;
                bool inside = (x >= rect.x && x < rect.x + rect.width &&
                               y >= rect.y && y < rect.y + rect.height);
                if (!inside)
                    return {};

                // بحث في الأبناء أولاً (أعمق ScrollView له الأولوية)
                for (int i = static_cast<int>(layout->children.size()) - 1; i >= 0; --i)
                {
                    if (i < static_cast<int>(node->childCount()))
                    {
                        auto hit = findScrollableNode(
                            node->getChildren()[i].get(),
                            layout->children[i].get(), x, y);
                        if (hit.node)
                            return hit;
                    }
                }

                // هل هذا العنصر قابل للتمرير؟
                auto t = node->getType();
                if (isScrollableType(t))
                {
                    return {node, layout};
                }
                return {};
            }

            // ─── تشغيل إطار واحد (للتكامل مع WASM) ─────────

            // ─── جمع العناصر القابلة للتركيز ─────────────────

            void DesktopWindow::collectFocusableNodes(
                const IRNode *node, std::vector<const IRNode *> &out) const
            {
                if (!node)
                    return;
                auto t = node->getType();
                if (t == UINodeType::Button || t == UINodeType::TextField ||
                    t == UINodeType::Toggle || t == UINodeType::Checkbox ||
                    t == UINodeType::Slider || t == UINodeType::Radio)
                {
                    out.push_back(node);
                }
                for (const auto &child : node->getChildren())
                {
                    collectFocusableNodes(child.get(), out);
                }
            }

            // ─── تشغيل إطار واحد (للتكامل مع WASM) ─────────

            bool DesktopWindow::runOneFrame()
            {
                if (!isOpen_)
                    return false;

                processEvents();

                if (needsRedraw_)
                {
                    updateLayout();
                    renderFrame();
                }

                return isOpen_;
            }

            // ─── الحصول على SDL_Window للتكامل الخارجي ─────

            void *DesktopWindow::getNativeHandle() const
            {
#ifdef SAD_UI_USE_SDL2
                return sdlWindow_;
#else
                return nullptr;
#endif
            }

            // ═══════════════════════════════════════════════════════════════════════════════
            // تنفيذ واجهة PlatformWindow
            // ═══════════════════════════════════════════════════════════════════════════════

            bool DesktopWindow::create(const PlatformWindowOptions &opts)
            {
                // تحويل PlatformWindowOptions إلى WindowOptions
                WindowOptions desktopOpts;
                desktopOpts.title = opts.title;
                desktopOpts.width = opts.width;
                desktopOpts.height = opts.height;
                desktopOpts.resizable = opts.resizable;
                desktopOpts.fullscreen = opts.fullscreen;
                desktopOpts.borderless = opts.borderless;
                desktopOpts.highDPI = opts.highDPI;
                desktopOpts.direction = opts.direction;
                return create(desktopOpts);
            }

            // ═══════════════════════════════════════════════════════════════════════════════
            // معالجة مفاتيح تحرير النصوص المتقدمة
            // ═══════════════════════════════════════════════════════════════════════════════

            void DesktopWindow::handleTextEditKeyEvent(int keysym, uint16_t mod)
            {
#ifdef SAD_UI_USE_SDL2
                if (!focusedNode_ || !textEditStates_)
                    return;

                auto &editState = textEditStates_->getState(focusedNode_);
                auto &undoMgr = textEditStates_->getUndoManager(focusedNode_);

                // مزامنة النص من العقدة
                std::string currentText;
                const auto *valProp = focusedNode_->findProperty(props::VALUE); // قيمة
                if (!valProp)
                    valProp = focusedNode_->findProperty(props::VALUE_LATIN);
                if (valProp)
                {
                    if (auto *s = std::get_if<std::string>(&valProp->value))
                        currentText = *s;
                }
                editState.text = currentText;

                bool ctrl = (mod & KMOD_CTRL) != 0;
                bool shift = (mod & KMOD_SHIFT) != 0;
                bool textChanged = false;
                std::string beforeText = editState.text;
                size_t beforeCursor = editState.cursorPos;

                // ─── اختصارات Ctrl ───
                if (ctrl)
                {
                    switch (keysym)
                    {
                    case SDLK_c:
                    {
                        // نسخ
                        std::string sel = editState.getSelectedText();
                        if (!sel.empty())
                        {
                            SDL_SetClipboardText(sel.c_str());
                        }
                        return;
                    }
                    case SDLK_x:
                    {
                        // قص
                        std::string sel = editState.getSelectedText();
                        if (!sel.empty())
                        {
                            SDL_SetClipboardText(sel.c_str());
                            editState.deleteSelection();
                            textChanged = true;
                        }
                        break;
                    }
                    case SDLK_v:
                    {
                        // لصق
                        if (SDL_HasClipboardText())
                        {
                            char *clipText = SDL_GetClipboardText();
                            if (clipText)
                            {
                                editState.insertText(clipText);
                                SDL_free(clipText);
                                textChanged = true;
                            }
                        }
                        break;
                    }
                    case SDLK_a:
                    {
                        // تحديد الكل
                        editState.selectAll();
                        invalidate();
                        return;
                    }
                    case SDLK_z:
                    {
                        // تراجع
                        std::string newText;
                        size_t newCursor;
                        if (undoMgr.undo(newText, newCursor))
                        {
                            editState.text = newText;
                            editState.cursorPos = newCursor;
                            editState.clearSelection();
                            textChanged = true;
                            // لا نسجل هذه العملية كـ undo جديد
                            auto *mutableNode = const_cast<IRNode *>(focusedNode_);
                            mutableNode->setProperty("\xd9\x82\xd9\x8a\xd9\x85\xd8\xa9", editState.text);
                            invalidate();
                            return;
                        }
                        return;
                    }
                    case SDLK_y:
                    {
                        // إعادة
                        std::string newText;
                        size_t newCursor;
                        if (undoMgr.redo(newText, newCursor))
                        {
                            editState.text = newText;
                            editState.cursorPos = newCursor;
                            editState.clearSelection();
                            textChanged = true;
                            auto *mutableNode = const_cast<IRNode *>(focusedNode_);
                            mutableNode->setProperty("\xd9\x82\xd9\x8a\xd9\x85\xd8\xa9", editState.text);
                            invalidate();
                            return;
                        }
                        return;
                    }
                    default:
                        return;
                    }
                }
                // ─── مفاتيح الأسهم والتحرير ───
                else
                {
                    switch (keysym)
                    {
                    case SDLK_LEFT:
                        editState.moveCursorLeft(shift);
                        invalidate();
                        return;
                    case SDLK_RIGHT:
                        editState.moveCursorRight(shift);
                        invalidate();
                        return;
                    case SDLK_HOME:
                        editState.moveCursorHome(shift);
                        invalidate();
                        return;
                    case SDLK_END:
                        editState.moveCursorEnd(shift);
                        invalidate();
                        return;
                    case SDLK_BACKSPACE:
                    {
                        if (editState.hasSelection)
                        {
                            editState.deleteSelection();
                        }
                        else if (editState.cursorPos > 0)
                        {
                            editState.moveCursorLeft(false);
                            size_t deleteStart = editState.cursorPos;
                            // حذف حرف واحد عند المؤشر
                            size_t nextPos = deleteStart;
                            unsigned char ch = static_cast<unsigned char>(editState.text[nextPos]);
                            size_t charLen = 1;
                            if ((ch & 0xE0) == 0xC0)
                                charLen = 2;
                            else if ((ch & 0xF0) == 0xE0)
                                charLen = 3;
                            else if ((ch & 0xF8) == 0xF0)
                                charLen = 4;
                            editState.text.erase(deleteStart, charLen);
                        }
                        textChanged = true;
                        break;
                    }
                    case SDLK_DELETE:
                    {
                        if (editState.hasSelection)
                        {
                            editState.deleteSelection();
                        }
                        else if (editState.cursorPos < editState.text.size())
                        {
                            unsigned char ch = static_cast<unsigned char>(editState.text[editState.cursorPos]);
                            size_t charLen = 1;
                            if ((ch & 0xE0) == 0xC0)
                                charLen = 2;
                            else if ((ch & 0xF0) == 0xE0)
                                charLen = 3;
                            else if ((ch & 0xF8) == 0xF0)
                                charLen = 4;
                            editState.text.erase(editState.cursorPos, charLen);
                        }
                        textChanged = true;
                        break;
                    }
                    default:
                        return;
                    }
                }

                // تحديث العقدة إذا تغيّر النص
                if (textChanged)
                {
                    // تسجيل عملية Undo
                    undoMgr.pushAction({beforeText, editState.text, beforeCursor, editState.cursorPos});

                    auto *mutableNode = const_cast<IRNode *>(focusedNode_);
                    mutableNode->setProperty("\xd9\x82\xd9\x8a\xd9\x85\xd8\xa9", editState.text);
                    invalidate();

                    // إطلاق حدث عند_التغيير — نداءٌ واحد (تفويض الاختيار للمُرسِل، تجنّب N²)
                    {
                        EventData data;
                        data.value = editState.text;
                        fireEvent(IREventType::OnChange, std::string(),
                                  focusedNode_, data);
                    }
                }
#endif
            }

            // ═══════════════════════════════════════════════════════════════════════════════
            // دوال مساعدة لمعالج لوحة المفاتيح الموحد
            // ═══════════════════════════════════════════════════════════════════════════════

            bool DesktopWindow::isTextInputNodeType(const IRNode *node)
            {
                if (!node)
                    return false;
                auto t = node->getType();
                return t == UINodeType::TextField || t == UINodeType::TextArea;
            }

#ifdef SAD_UI_USE_SDL2
            // (AR) تحويل SDL_Scancode إلى UnifiedKeyCode
            static UnifiedKeyCode sdlScancodeToUnified(int sdlScancode)
            {
                // SDL_Scancode values match USB HID Usage codes
                // which match our UnifiedKeyCode values
                switch (sdlScancode)
                {
                case 4:
                    return UnifiedKeyCode::A;
                case 5:
                    return UnifiedKeyCode::B;
                case 6:
                    return UnifiedKeyCode::C;
                case 7:
                    return UnifiedKeyCode::D;
                case 8:
                    return UnifiedKeyCode::E;
                case 9:
                    return UnifiedKeyCode::F;
                case 10:
                    return UnifiedKeyCode::G;
                case 11:
                    return UnifiedKeyCode::H;
                case 12:
                    return UnifiedKeyCode::I;
                case 13:
                    return UnifiedKeyCode::J;
                case 14:
                    return UnifiedKeyCode::K;
                case 15:
                    return UnifiedKeyCode::L;
                case 16:
                    return UnifiedKeyCode::M;
                case 17:
                    return UnifiedKeyCode::N;
                case 18:
                    return UnifiedKeyCode::O;
                case 19:
                    return UnifiedKeyCode::P;
                case 20:
                    return UnifiedKeyCode::Q;
                case 21:
                    return UnifiedKeyCode::R;
                case 22:
                    return UnifiedKeyCode::S;
                case 23:
                    return UnifiedKeyCode::T;
                case 24:
                    return UnifiedKeyCode::U;
                case 25:
                    return UnifiedKeyCode::V;
                case 26:
                    return UnifiedKeyCode::W;
                case 27:
                    return UnifiedKeyCode::X;
                case 28:
                    return UnifiedKeyCode::Y;
                case 29:
                    return UnifiedKeyCode::Z;
                case 30:
                    return UnifiedKeyCode::Num1;
                case 31:
                    return UnifiedKeyCode::Num2;
                case 32:
                    return UnifiedKeyCode::Num3;
                case 33:
                    return UnifiedKeyCode::Num4;
                case 34:
                    return UnifiedKeyCode::Num5;
                case 35:
                    return UnifiedKeyCode::Num6;
                case 36:
                    return UnifiedKeyCode::Num7;
                case 37:
                    return UnifiedKeyCode::Num8;
                case 38:
                    return UnifiedKeyCode::Num9;
                case 39:
                    return UnifiedKeyCode::Num0;
                case 40:
                    return UnifiedKeyCode::Return;
                case 41:
                    return UnifiedKeyCode::Escape;
                case 42:
                    return UnifiedKeyCode::Backspace;
                case 43:
                    return UnifiedKeyCode::Tab;
                case 44:
                    return UnifiedKeyCode::Space;
                case 58:
                    return UnifiedKeyCode::F1;
                case 59:
                    return UnifiedKeyCode::F2;
                case 60:
                    return UnifiedKeyCode::F3;
                case 61:
                    return UnifiedKeyCode::F4;
                case 62:
                    return UnifiedKeyCode::F5;
                case 63:
                    return UnifiedKeyCode::F6;
                case 64:
                    return UnifiedKeyCode::F7;
                case 65:
                    return UnifiedKeyCode::F8;
                case 66:
                    return UnifiedKeyCode::F9;
                case 67:
                    return UnifiedKeyCode::F10;
                case 68:
                    return UnifiedKeyCode::F11;
                case 69:
                    return UnifiedKeyCode::F12;
                case 73:
                    return UnifiedKeyCode::Insert;
                case 74:
                    return UnifiedKeyCode::Home;
                case 75:
                    return UnifiedKeyCode::PageUp;
                case 76:
                    return UnifiedKeyCode::Delete;
                case 77:
                    return UnifiedKeyCode::End;
                case 78:
                    return UnifiedKeyCode::PageDown;
                case 79:
                    return UnifiedKeyCode::Right;
                case 80:
                    return UnifiedKeyCode::Left;
                case 81:
                    return UnifiedKeyCode::Down;
                case 82:
                    return UnifiedKeyCode::Up;
                default:
                    return UnifiedKeyCode::Unknown;
                }
            }

            // (AR) تحويل SDL keysym إلى UnifiedKeyCode
            //      نستخدم هذا لربط الاثنين حيث SDL_KEYDOWN يعطي sym وscancode
            static UnifiedKeyCode sdlKeysymToUnified(int keysym)
            {
                switch (keysym)
                {
                case SDLK_RETURN:
                    return UnifiedKeyCode::Return;
                case SDLK_ESCAPE:
                    return UnifiedKeyCode::Escape;
                case SDLK_BACKSPACE:
                    return UnifiedKeyCode::Backspace;
                case SDLK_TAB:
                    return UnifiedKeyCode::Tab;
                case SDLK_SPACE:
                    return UnifiedKeyCode::Space;
                case SDLK_DELETE:
                    return UnifiedKeyCode::Delete;
                case SDLK_RIGHT:
                    return UnifiedKeyCode::Right;
                case SDLK_LEFT:
                    return UnifiedKeyCode::Left;
                case SDLK_DOWN:
                    return UnifiedKeyCode::Down;
                case SDLK_UP:
                    return UnifiedKeyCode::Up;
                case SDLK_HOME:
                    return UnifiedKeyCode::Home;
                case SDLK_END:
                    return UnifiedKeyCode::End;
                case SDLK_PAGEUP:
                    return UnifiedKeyCode::PageUp;
                case SDLK_PAGEDOWN:
                    return UnifiedKeyCode::PageDown;
                case SDLK_INSERT:
                    return UnifiedKeyCode::Insert;
                case SDLK_a:
                    return UnifiedKeyCode::A;
                case SDLK_b:
                    return UnifiedKeyCode::B;
                case SDLK_c:
                    return UnifiedKeyCode::C;
                case SDLK_d:
                    return UnifiedKeyCode::D;
                case SDLK_e:
                    return UnifiedKeyCode::E;
                case SDLK_f:
                    return UnifiedKeyCode::F;
                case SDLK_v:
                    return UnifiedKeyCode::V;
                case SDLK_x:
                    return UnifiedKeyCode::X;
                case SDLK_y:
                    return UnifiedKeyCode::Y;
                case SDLK_z:
                    return UnifiedKeyCode::Z;
                default:
                    return UnifiedKeyCode::Unknown;
                }
            }
#endif

            bool DesktopWindow::handleTextEditKeyViaProcessor(
                const IRNode *node, UnifiedKeyCode keyCode, const KeyModifiers &mods)
            {
#ifdef SAD_UI_USE_SDL2
                if (!node || !textEditStates_)
                    return false;
                if (!isTextInputNodeType(node))
                    return false;

                // (AR) تحويل UnifiedKeyCode → SDL keysym للتوافق مع handleTextEditKeyEvent
                int sdlKeysym = 0;
                switch (keyCode)
                {
                case UnifiedKeyCode::Return:
                    sdlKeysym = SDLK_RETURN;
                    break;
                case UnifiedKeyCode::Backspace:
                    sdlKeysym = SDLK_BACKSPACE;
                    break;
                case UnifiedKeyCode::Delete:
                    sdlKeysym = SDLK_DELETE;
                    break;
                case UnifiedKeyCode::Left:
                    sdlKeysym = SDLK_LEFT;
                    break;
                case UnifiedKeyCode::Right:
                    sdlKeysym = SDLK_RIGHT;
                    break;
                case UnifiedKeyCode::Up:
                    sdlKeysym = SDLK_UP;
                    break;
                case UnifiedKeyCode::Down:
                    sdlKeysym = SDLK_DOWN;
                    break;
                case UnifiedKeyCode::Home:
                    sdlKeysym = SDLK_HOME;
                    break;
                case UnifiedKeyCode::End:
                    sdlKeysym = SDLK_END;
                    break;
                case UnifiedKeyCode::A:
                    sdlKeysym = SDLK_a;
                    break;
                case UnifiedKeyCode::C:
                    sdlKeysym = SDLK_c;
                    break;
                case UnifiedKeyCode::V:
                    sdlKeysym = SDLK_v;
                    break;
                case UnifiedKeyCode::X:
                    sdlKeysym = SDLK_x;
                    break;
                case UnifiedKeyCode::Y:
                    sdlKeysym = SDLK_y;
                    break;
                case UnifiedKeyCode::Z:
                    sdlKeysym = SDLK_z;
                    break;
                default:
                    return false;
                }

                uint16_t sdlMod = 0;
                if (mods.shift)
                    sdlMod |= KMOD_SHIFT;
                if (mods.ctrl)
                    sdlMod |= KMOD_CTRL;
                if (mods.alt)
                    sdlMod |= KMOD_ALT;

                handleTextEditKeyEvent(sdlKeysym, sdlMod);
                return true;
#else
                return false;
#endif
            }

            bool DesktopWindow::handleTextInputViaProcessor(
                const IRNode *node, const std::string &text)
            {
#ifdef SAD_UI_USE_SDL2
                if (!node || !textEditStates_)
                    return false;
                if (!isTextInputNodeType(node))
                    return false;

                auto &editState = textEditStates_->getState(node);
                auto &undoMgr = textEditStates_->getUndoManager(node);

                // مزامنة النص من العقدة
                std::string currentText;
                const auto *valProp = node->findProperty(props::VALUE); // قيمة
                if (!valProp)
                    valProp = node->findProperty(props::VALUE_LATIN);
                if (valProp)
                {
                    if (auto *s = std::get_if<std::string>(&valProp->value))
                        currentText = *s;
                }
                editState.text = currentText;

                // حفظ حالة ما قبل العملية
                std::string beforeText = editState.text;
                size_t beforeCursor = editState.cursorPos;

                // إدراج النص الجديد
                editState.insertText(text);

                // تسجيل عملية Undo
                undoMgr.pushAction({beforeText, editState.text, beforeCursor, editState.cursorPos});

                // تحديث العقدة
                auto *mutableNode = const_cast<IRNode *>(node);
                mutableNode->setProperty(
                    "\xd9\x82\xd9\x8a\xd9\x85\xd8\xa9", editState.text);
                invalidate();

                return true;
#else
                return false;
#endif
            }

        } // namespace desktop
    } // namespace ui
} // namespace sad
