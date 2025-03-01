﻿#include "Core/Graphics/Window_SDL.hpp"
#include "Core/ApplicationModel_SDL.hpp"
#include "Core/InitializeConfigure.hpp"
#include "Core/Type.hpp"
#include "Core/i18n.hpp"
#include "glad/gl.h"
#include "uni_algo/ranges_grapheme.h"
#include "uni_algo/ranges_word.h"
#include "SDL.h"
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <iterator>
#include <spdlog/spdlog.h>
#include <string>

namespace Core::Graphics
{
#define APPMODEL ((ApplicationModel_SDL*)m_framework)
    void Window_SDL::handleEvents()
    {
        SDL_Event ev;
        while (SDL_PollEvent(&ev))
        {

            dispatchEvent(EventType::NativeWindowMessage, EventData{ .event = ev});
            switch (ev.type)
            {
            case SDL_QUIT:
                dispatchEvent(EventType::WindowClose);
                APPMODEL->requestExit();
                break;
            case SDL_WINDOWEVENT:
                switch (ev.window.event)
                {
                case SDL_WINDOWEVENT_RESIZED:
                    {
                        EventData d = {};
                        d.window_size = Vector2I(ev.window.data1, ev.window.data2);
                        sdl_window_width = ev.window.data1;
                        sdl_window_height = ev.window.data2;
                        dispatchEvent(EventType::WindowSize, d);
                    }
                    break;
                case SDL_WINDOWEVENT_FOCUS_GAINED:
                    dispatchEvent(EventType::WindowActive);
                    break;
                case SDL_WINDOWEVENT_FOCUS_LOST:
                    dispatchEvent(EventType::WindowInactive);
                    break;
                }
                break;
            case SDL_TEXTINPUT:
                if (!(SDL_GetModState() & (KMOD_CTRL | KMOD_LALT)))
                {
                    // m_text_input = m_text_input
                    //     .substr(0, m_text_cursor_pos)
                    //     .append(ev.text.text)
                    //     .append(
                    //         m_text_input.substr(m_text_cursor_pos)
                    //     );

                    // m_text_cursor_pos += std::strlen(ev.text.text);
                    // auto view = una::views::grapheme::utf8(m_text_input);
                    // auto pos = std::next(view.begin(), m_text_cursor_pos).begin() - m_text_input.begin();
                    // m_text_input.insert(pos, ev.text.text);

                    // auto view2 = una::views::grapheme::utf8(std::string(ev.text.text));
                    // m_text_cursor_pos += std::distance(view2.begin(), view2.end());

                    insertInputTextAtCursor(ev.text.text);
                    m_ime_cursor_pos = -1;
                }
                break;
            case SDL_TEXTEDITING:
                m_ime_comp = ev.edit.text;
                m_ime_cursor_pos = ev.edit.start;
                break;
            case SDL_TEXTEDITING_EXT:
                m_ime_comp = ev.editExt.text;
                m_ime_cursor_pos = ev.editExt.start;
                break;
            case SDL_KEYDOWN:
                switch (ev.key.keysym.sym) {
                    case SDLK_BACKSPACE:
                        if (SDL_IsTextInputActive() && m_text_cursor_pos > 0 && !(SDL_GetModState() & KMOD_ALT))
                        {
                            // m_text_input = m_text_input
                            //     .substr(0, m_text_cursor_pos - 1)
                            //     .append(
                            //         m_text_input.substr(m_text_cursor_pos)
                            //     );
                            if (SDL_GetModState() & KMOD_CTRL)
                            {
                                std::string text = m_text_input.substr(0, getTextCursorPosRaw());
                                auto view = una::views::grapheme::utf8(text);
                                auto view2 = una::views::grapheme::utf8(m_text_input);
                                auto words = una::views::word::utf8(text);
                                uint32_t amt_words = std::distance(words.begin(), words.end());

                                auto pos = std::next(view2.begin(), m_text_cursor_pos).begin() - m_text_input.begin();
                                auto pos2 = std::next(words.begin(), amt_words - 1).begin() - text.begin();

                                auto last_word_view = una::views::grapheme::utf8(*std::next(words.begin(), amt_words - 1));

                                m_text_cursor_pos -= std::distance(last_word_view.begin(), last_word_view.end());

                                m_text_input = m_text_input
                                    .substr(0, pos2)
                                    .append(m_text_input.substr(pos));
            
                                m_ime_cursor_pos = -1;
                            }
                            else
                            {
                                removeInputTextAtCursor(1, false);
                                m_ime_cursor_pos = -1;
                            }
                        }
                        break;
                    case SDLK_DELETE:
                        if (SDL_IsTextInputActive() && m_text_cursor_pos < getTextInputLength() && !(SDL_GetModState() & KMOD_ALT))
                        {
                            if (SDL_GetModState() & KMOD_CTRL)
                            {
                                std::string text = m_text_input.substr(getTextCursorPosRaw());
                                auto view = una::views::grapheme::utf8(text);
                                auto view2 = una::views::grapheme::utf8(m_text_input);
                                auto words = una::views::word::utf8(text);

                                auto pos = std::next(view2.begin(), m_text_cursor_pos).begin() - m_text_input.begin();
                                auto pos2 = std::next(words.begin()).begin() - text.begin();

                                m_text_input = m_text_input
                                    .substr(0, pos)
                                    .append(m_text_input.substr(pos2));

                                m_ime_cursor_pos = -1;
                            }
                            else
                            {
                                removeInputTextAtCursor(1, true);

                                m_ime_cursor_pos = -1;
                            }
                        }
                        break;
                    case SDLK_RETURN:
                        if (SDL_IsTextInputActive() && m_return_enable && !(SDL_GetModState() & (KMOD_ALT | KMOD_CTRL)))
                        {
                            auto view = una::views::grapheme::utf8(m_text_input);
                            auto pos = std::next(view.begin(), m_text_cursor_pos).begin() - m_text_input.begin();
                            m_text_input.insert(pos, "\n");

                            m_text_cursor_pos += 1;
                            m_ime_cursor_pos = -1;
                        }
                }
            }
        }
    }

    const char * gp_cSeverity[] = {"High", "Medium", "Low", "Notification"};
    const char * gp_cType[] = {"Error", "Deprecated", "Undefined", "Portability", "Performance", "Other"};
    const char * gp_cSource[] = {"OpenGL", "OS", "GLSL Compiler", "3rd Party", "Application", "Other"};

    void DebugCallback(uint32_t uiSource, uint32_t uiType, uint32_t uiID, uint32_t uiSeverity, int32_t iLength, const char * p_cMessage, void* p_UserParam)
    {
        // Get the severity
        uint32_t uiSevID = 3;
        switch (uiSeverity) {
            case GL_DEBUG_SEVERITY_HIGH:
                uiSevID = 0; break;
            case GL_DEBUG_SEVERITY_MEDIUM:
                uiSevID = 1; break;
            case GL_DEBUG_SEVERITY_LOW:
                uiSevID = 2; break;
            case GL_DEBUG_SEVERITY_NOTIFICATION:
            default:
                uiSevID = 3; break;
        }

        // Get the type
        uint32_t uiTypeID = 5;
        switch (uiType) {
            case GL_DEBUG_TYPE_ERROR:
                uiTypeID = 0; break;
            case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR:
                uiTypeID = 1; break;
            case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:
                uiTypeID = 2; break;
            case GL_DEBUG_TYPE_PORTABILITY:
                uiTypeID = 3; break;
            case GL_DEBUG_TYPE_PERFORMANCE:
                uiTypeID = 4; break;
            case GL_DEBUG_TYPE_OTHER:
            default:
                uiTypeID = 5; break;
        }

        // Get the source
        uint32_t uiSourceID = 5;
        switch (uiSource) {
            case GL_DEBUG_SOURCE_API:
                uiSourceID = 0; break;
            case GL_DEBUG_SOURCE_WINDOW_SYSTEM:
                uiSourceID = 1; break;
            case GL_DEBUG_SOURCE_SHADER_COMPILER:
                uiSourceID = 2; break;
            case GL_DEBUG_SOURCE_THIRD_PARTY:
                uiSourceID = 3; break;
            case GL_DEBUG_SOURCE_APPLICATION:
                uiSourceID = 4; break;
            case GL_DEBUG_SOURCE_OTHER:
            default:
                uiSourceID = 5; break;
        }

        // Output to the Log
        spdlog::debug("OpenGL Debug: Severity={}, Type={}, Source={} - {}",
                        gp_cSeverity[uiSevID], gp_cType[uiTypeID], gp_cSource[uiSourceID], p_cMessage);
        if (uiSeverity == GL_DEBUG_SEVERITY_HIGH) {
            //This a serious error so we need to shutdown the program
            SDL_Event event;
            event.type = SDL_QUIT;
            SDL_PushEvent(&event);
        }
    }

    bool Window_SDL::createWindow()
    {
        // Create a window

        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
        SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
        SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
        SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
        SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);
        SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
#ifndef NDEBUG
        spdlog::debug("GL DEBUGGER ENABLED");
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_DEBUG_FLAG);
#endif

        sdl_window = SDL_CreateWindow(
            sdl_window_text.c_str(),
            SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
            sdl_window_width, sdl_window_height,
            sdl_window_flags
        );
        if (sdl_window == NULL)
        {
            spdlog::error("[luastg] (GetError = {}) SDL_CreateWindow failed", SDL_GetError());
            return false;
        }
        m_monitor_idx = SDL_GetWindowDisplayIndex(sdl_window);

        SDL_GLContext context = SDL_GL_CreateContext(sdl_window);

        int version = gladLoadGL((GLADloadfunc) SDL_GL_GetProcAddress);
        spdlog::info("[core] OpenGL {}.{}", GLAD_VERSION_MAJOR(version), GLAD_VERSION_MINOR(version));
        spdlog::info("[core] {}", (const char*)glGetString(GL_VERSION));
        GLint s;
        glGetIntegerv(GL_MAX_TEXTURE_SIZE, &s);
        spdlog::info("[core] max texture size: {}", s);

#ifndef __APPLE__
#ifndef NDEBUG
        glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
    
        //Set up the debug info callback
        glDebugMessageCallback((GLDEBUGPROC)&DebugCallback, NULL);
    
        //Set up the type of debug information we want to receive
        uint32_t uiUnusedIDs = 0;
        glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, &uiUnusedIDs, GL_TRUE); //Enable all
#endif
#endif

        dispatchEvent(EventType::WindowCreate);

        return true;
    }
    void Window_SDL::destroyWindow()
    {
        if (sdl_window)
        {
            SDL_DestroyWindow(sdl_window);
        }
        sdl_window = NULL;
    }
    bool Window_SDL::recreateWindow()
    {
        dispatchEvent(EventType::WindowDestroy);
        uint32_t flags = SDL_GetWindowFlags(sdl_window);
        Vector2I pos{};
        SDL_GetWindowPosition(sdl_window, &pos.x, &pos.y);
        SDL_DestroyWindow(sdl_window);
        sdl_window = SDL_CreateWindow(sdl_window_text.c_str(), pos.x, pos.y, sdl_window_width, sdl_window_height, flags);
        
        if (sdl_window == NULL)
        {
            spdlog::error("[luastg] (GetError = {}) SDL_CreateWindow failed", SDL_GetError());
            return false;
        }

        dispatchEvent(EventType::WindowCreate);
        return true;
    }
    void Window_SDL::_toggleFullScreenMode()
    {
        if (m_fullscreen_mode != FullscreenMode::Windowed)
            _setWindowMode(Vector2U(sdl_window_width, sdl_window_height), true);
        else
            _setBorderlessFullScreenMode();
    }
    void Window_SDL::_setWindowMode(Vector2U size, bool ignore_size)
    {
        SDL_SetWindowFullscreen(sdl_window, 0);

        bool want_restore_placement = false;

        if (m_fullscreen_mode != FullscreenMode::Windowed && ignore_size)
        {
            want_restore_placement = true;
        }

        setFrameStyle(WindowFrameStyle::Normal);
        m_fullscreen_mode = FullscreenMode::Windowed;

        EventData event_data{};
        event_data.window_fullscreen_state = false;
        dispatchEvent(EventType::WindowFullscreenStateChange, event_data);

        if (want_restore_placement)
        {
            SDL_SetWindowPosition(sdl_window, m_last_window_rect.x, m_last_window_rect.y);
            SDL_SetWindowSize(sdl_window, m_last_window_rect.w, m_last_window_rect.h);
            sdl_window_width = m_last_window_rect.w;
            sdl_window_height = m_last_window_rect.h;
        }
        else
        {
            int32_t monitor = m_monitor_idx == -1 ? SDL_GetWindowDisplayIndex(sdl_window) : m_monitor_idx;
            SDL_SetWindowPosition(sdl_window, SDL_WINDOWPOS_CENTERED_DISPLAY(m_monitor_idx), SDL_WINDOWPOS_CENTERED_DISPLAY(m_monitor_idx));
            SDL_SetWindowSize(sdl_window, size.x, size.y);
            sdl_window_width = size.x;
            sdl_window_height = size.y;
        }
    }
    void Window_SDL::_setBorderlessFullScreenMode()
    {
        if (m_fullscreen_mode == FullscreenMode::Windowed)
        {
            SDL_GetWindowPosition(sdl_window, &m_last_window_rect.x, &m_last_window_rect.y);
            SDL_GetWindowSize(sdl_window, &m_last_window_rect.w, &m_last_window_rect.h);
        }

        int32_t monitor = m_monitor_idx == -1 ? SDL_GetWindowDisplayIndex(sdl_window) : m_monitor_idx;
        SDL_Rect r;
        SDL_GetDisplayBounds(monitor, &r);
        sdl_window_width = r.w;
        sdl_window_height = r.h;

        SDL_SetWindowSize(sdl_window, r.w, r.h);
        SDL_SetWindowPosition(sdl_window, SDL_WINDOWPOS_CENTERED_DISPLAY(monitor), SDL_WINDOWPOS_CENTERED_DISPLAY(monitor));
        SDL_SetWindowFullscreen(sdl_window, SDL_WINDOW_FULLSCREEN_DESKTOP);

        setFrameStyle(WindowFrameStyle::None);
        m_fullscreen_mode = FullscreenMode::Borderless;

        EventData event_data{};
        event_data.window_fullscreen_state = false;
        dispatchEvent(EventType::WindowFullscreenStateChange, event_data);
    }
    void Window_SDL::_setFullScreenMode()
    {
        if (m_fullscreen_mode == FullscreenMode::Windowed)
        {
            SDL_GetWindowPosition(sdl_window, &m_last_window_rect.x, &m_last_window_rect.y);
            SDL_GetWindowSize(sdl_window, &m_last_window_rect.w, &m_last_window_rect.h);
        }

        int32_t monitor = m_monitor_idx == -1 ? SDL_GetWindowDisplayIndex(sdl_window) : m_monitor_idx;
        SDL_Rect r;
        SDL_GetDisplayBounds(monitor, &r);
        sdl_window_width = r.w;
        sdl_window_height = r.h;

        SDL_SetWindowPosition(sdl_window, SDL_WINDOWPOS_CENTERED_DISPLAY(monitor), SDL_WINDOWPOS_CENTERED_DISPLAY(monitor));
        SDL_SetWindowFullscreen(sdl_window, SDL_WINDOW_FULLSCREEN);

        setFrameStyle(WindowFrameStyle::None);
        m_fullscreen_mode = FullscreenMode::Exclusive;

        EventData event_data{};
        event_data.window_fullscreen_state = true;
        dispatchEvent(EventType::WindowFullscreenStateChange, event_data);
    }

    RectI Window_SDL::getRect()
    {
        SDL_Rect rc;
        SDL_GetWindowPosition(sdl_window, &rc.x, &rc.y);
        SDL_GetWindowSize(sdl_window, &rc.w, &rc.h);
        return RectI(rc.x, rc.y, rc.x + rc.w, rc.y + rc.h);
    }
    bool Window_SDL::setRect(RectI v)
    {
        SDL_SetWindowPosition(sdl_window, v.a.x, v.a.y);
        SDL_SetWindowSize(sdl_window, v.b.x - v.a.x, v.b.y - v.a.y);
        return true;
    }

    void Window_SDL::dispatchEvent(EventType t, EventData d)
    {
        // callback
        m_is_dispatch_event = true;
        switch (t)
        {
        case EventType::WindowCreate:
            for (auto& v : m_eventobj)
            {
                if (v) v->onWindowCreate();
            }
            break;
        case EventType::WindowDestroy:
            for (auto& v : m_eventobj)
            {
                if (v) v->onWindowDestroy();
            }
            break;
        case EventType::WindowActive:
            for (auto& v : m_eventobj)
            {
                if (v) v->onWindowActive();
            }
            break;
        case EventType::WindowInactive:
            for (auto& v : m_eventobj)
            {
                if (v) v->onWindowInactive();
            }
            break;
        case EventType::WindowClose:
            for (auto& v : m_eventobj)
            {
                if (v) v->onWindowClose();
            }
            break;
        case EventType::WindowSize:
            for (auto& v : m_eventobj)
            {
                if (v) v->onWindowSize(d.window_size);
            }
            break;
        case EventType::NativeWindowMessage:
            for (auto& v : m_eventobj)
            {
                if (v) v->onNativeWindowMessage(&d.event);
            }
            break;
        case EventType::DeviceChange:
            for (auto& v : m_eventobj)
            {
                if (v) v->onDeviceChange();
            }
            break;
        }
        m_is_dispatch_event = false;
        // Dealing with delayed objects
        removeEventListener(nullptr);
        for (auto& v : m_eventobj_late)
        {
            m_eventobj.emplace_back(v);
        }
        m_eventobj_late.clear();
    }
    void Window_SDL::Window_SDL::addEventListener(IWindowEventListener* e)
    {
        removeEventListener(e);
        if (m_is_dispatch_event)
        {
            m_eventobj_late.emplace_back(e);
        }
        else
        {
            m_eventobj.emplace_back(e);
        }
    }
    void Window_SDL::Window_SDL::removeEventListener(IWindowEventListener* e)
    {
        if (m_is_dispatch_event)
        {
            for (auto& v : m_eventobj)
            {
                if (v == e)
                {
                    v = nullptr; // doesn't break traversal
                }
            }
        }
        else
        {
            for (auto it = m_eventobj.begin(); it != m_eventobj.end();)
            {
                if (*it == e)
                    it = m_eventobj.erase(it);
                else
                    it++;
            }
        }
    }

    void* Window_SDL::getNativeHandle() { return sdl_window; }

    void Window_SDL::setTitleText(StringView str)
    {
        sdl_window_text = str;
        SDL_SetWindowTitle(sdl_window, sdl_window_text.c_str());
    }
    StringView Window_SDL::getTitleText()
    {
        return sdl_window_text;
    }

    bool Window_SDL::setFrameStyle(WindowFrameStyle style)
    {
        m_framestyle = style;
        switch (style)
        {
        default:
            assert(false); return false;
        case WindowFrameStyle::None:
            // sdl_window_flags = SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN | SDL_WINDOW_BORDERLESS;
            SDL_SetWindowBordered(sdl_window, SDL_FALSE);
            SDL_SetWindowResizable(sdl_window, SDL_FALSE);
            break;
        case WindowFrameStyle::Fixed:
            // sdl_window_flags = SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN;
            SDL_SetWindowBordered(sdl_window, SDL_TRUE);
            SDL_SetWindowResizable(sdl_window, SDL_FALSE);
            break;
        case WindowFrameStyle::Normal:
            // sdl_window_flags = SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE;
            SDL_SetWindowBordered(sdl_window, SDL_TRUE);
            SDL_SetWindowResizable(sdl_window, SDL_TRUE);
            break;
        }
        return true;
    }
    WindowFrameStyle Window_SDL::getFrameStyle()
    {
        return m_framestyle;
    }

    Vector2U Window_SDL::getSize()
    {
        SDL_GetWindowSize(sdl_window, (int*)&sdl_window_width, (int*)&sdl_window_height);
        return { sdl_window_width, sdl_window_height };
    }
    bool Window_SDL::setSize(Vector2U v)
    {
        sdl_window_width = v.x;
        sdl_window_height = v.y;
        SDL_SetWindowSize(sdl_window, (int)v.x, (int)v.y);
        return true;
    }

    WindowLayer Window_SDL::getLayer()
    {
        if (m_hidewindow)
            return WindowLayer::Invisible;
        if (SDL_GetWindowFlags(sdl_window) & SDL_WINDOW_ALWAYS_ON_TOP)
            return WindowLayer::Top;
        return WindowLayer::Normal;
    }
    bool Window_SDL::setLayer(WindowLayer layer)
    {
        switch (layer)
        {
        default:
        case WindowLayer::Unknown:
            assert(false); return false;
        case WindowLayer::Invisible:
            SDL_HideWindow(sdl_window);
            break;
        case WindowLayer::Normal:
            SDL_ShowWindow(sdl_window);
            SDL_SetWindowAlwaysOnTop(sdl_window, SDL_FALSE);
            break;
        case WindowLayer::Top:
            SDL_ShowWindow(sdl_window);
            SDL_SetWindowAlwaysOnTop(sdl_window, SDL_TRUE);
            break;
        }
        m_hidewindow = layer == WindowLayer::Invisible;
        return true;
    }

    void Window_SDL::setWindowMode(Vector2U size)
    {
        _setWindowMode(size, false);
    }
    void Window_SDL::setExclusiveFullScreenMode()
    {
        _setFullScreenMode();
    }
    void Window_SDL::setBorderlessFullScreenMode()
    {
        _setBorderlessFullScreenMode();
    }
    
    uint32_t Window_SDL::getMonitorCount()
    {
        return SDL_GetNumVideoDisplays();
    }
    RectI Window_SDL::getMonitorRect(uint32_t index)
    {
        SDL_Rect rc;
        SDL_GetDisplayBounds(index, &rc);
        return RectI(rc.x, rc.y, rc.x + rc.w, rc.y + rc.h);
    }
    void Window_SDL::setMonitorCentered(uint32_t index)
    {
        SDL_SetWindowPosition(sdl_window, SDL_WINDOWPOS_CENTERED_DISPLAY(index), SDL_WINDOWPOS_CENTERED_DISPLAY(index));
    }
    void Window_SDL::setMonitorFullScreen(uint32_t index)
    {
        SDL_Rect rc;
        SDL_GetDisplayBounds(index, &rc);
        SDL_SetWindowPosition(sdl_window, rc.x, rc.y);
        SDL_SetWindowSize(sdl_window, rc.w, rc.h);
    }

    bool Window_SDL::setCursor(WindowCursor type)
    {
        m_cursor = type;
        switch (type)
        {
        default:
            assert(false); return false;

        case WindowCursor::None:
            SDL_ShowCursor(SDL_DISABLE);
            SDL_SetCursor(NULL);
            break;

        case WindowCursor::Arrow:
            SDL_ShowCursor(SDL_ENABLE);
            SDL_SetCursor(SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_ARROW));
            break;
        case WindowCursor::Hand:
            SDL_ShowCursor(SDL_ENABLE);
            SDL_SetCursor(SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_HAND));
            break;

        case WindowCursor::Cross:
            SDL_ShowCursor(SDL_ENABLE);
            SDL_SetCursor(SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_CROSSHAIR));
            break;
        case WindowCursor::TextInput:
            SDL_ShowCursor(SDL_ENABLE);
            SDL_SetCursor(SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_IBEAM));
            break;
        
        case WindowCursor::Resize:
            SDL_ShowCursor(SDL_ENABLE);
            SDL_SetCursor(SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_SIZEALL));
            break;
        case WindowCursor::ResizeEW:
            SDL_ShowCursor(SDL_ENABLE);
            SDL_SetCursor(SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_SIZEWE));
            break;
        case WindowCursor::ResizeNS:
            SDL_ShowCursor(SDL_ENABLE);
            SDL_SetCursor(SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_SIZENS));
            break;
        case WindowCursor::ResizeNESW:
            SDL_ShowCursor(SDL_ENABLE);
            SDL_SetCursor(SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_SIZENESW));
            break;
        case WindowCursor::ResizeNWSE:
            SDL_ShowCursor(SDL_ENABLE);
            SDL_SetCursor(SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_SIZENWSE));
            break;

        case WindowCursor::NotAllowed:
            SDL_ShowCursor(SDL_ENABLE);
            SDL_SetCursor(SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_NO));
            break;
        case WindowCursor::Wait:
            SDL_ShowCursor(SDL_ENABLE);
            SDL_SetCursor(SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_WAIT));
            break;
        }
        return true;
    }
    WindowCursor Window_SDL::getCursor()
    {
        return m_cursor;
    }

    void Window_SDL::setTextInputEnable(bool enable)
    {
        if (enable)
        {
            SDL_SetHint(SDL_HINT_IME_SHOW_UI, "1");
            SDL_StartTextInput();
        }
        else
        {
            SDL_StopTextInput();
        }
    }

    std::string Window_SDL::getTextInput()
    {
        return m_text_input;
    }

    std::string Window_SDL::getIMEComp()
    {
        return m_ime_comp;
    }

    void Window_SDL::setTextInput(StringView text)
    {
        m_text_input = text;
        auto view = una::views::grapheme::utf8(m_text_input);
        m_text_cursor_pos = std::distance(view.begin(), view.end());
    }

    void Window_SDL::clearTextInput()
    {
        m_text_input = "";
        m_ime_comp = "";
        m_text_cursor_pos = 0;
    }

    uint32_t Window_SDL::getTextInputLength()
    {
        return m_text_input.length();
    }

    uint32_t Window_SDL::getTextCursorPos()
    {
        return m_text_cursor_pos;
    }

    uint32_t Window_SDL::getTextCursorPosRaw()
    {
        auto view = una::views::grapheme::utf8(m_text_input);
        return std::next(view.begin(), m_text_cursor_pos).begin() - m_text_input.begin();
    }

    bool Window_SDL::setTextCursorPos(uint32_t pos)
    {
        if (pos > getTextInputLength())
        {
            return false;
        }

        m_text_cursor_pos = pos;

        return true;
    }

    int32_t Window_SDL::getIMECursorPos()
    {
        if (m_ime_cursor_pos < 0)
        {
            return m_ime_cursor_pos;
        }
        auto view = una::views::grapheme::utf8(m_ime_comp);
        return std::next(view.begin(), m_ime_cursor_pos).begin() - m_ime_comp.begin();
    }

    void Window_SDL::insertInputTextAtCursor(StringView text, bool move_cursor)
    {
        insertInputText(text, m_text_cursor_pos);
        if (!move_cursor)
            return;

        auto view2 = una::views::grapheme::utf8(text);
        m_text_cursor_pos += std::distance(view2.begin(), view2.end());
    }

    bool Window_SDL::insertInputText(StringView text, uint32_t pos)
    {
        auto view = una::views::grapheme::utf8(m_text_input);
        if (pos > std::distance(view.begin(), view.end()))
            return false;
        auto pos2 = std::next(view.begin(), m_text_cursor_pos).begin() - m_text_input.begin();
        m_text_input.insert(pos2, text);

        return true;
    }

    uint32_t Window_SDL::removeInputTextAtCursor(uint32_t length, bool after)
    {
        auto view = una::views::grapheme::utf8(m_text_input);
        auto pos = std::next(view.begin(), m_text_cursor_pos).begin() - m_text_input.begin();
        uint32_t actual_length;
        uint32_t pos2;
        if (after)
        {
            actual_length = std::distance(
                std::next(view.begin(), m_text_cursor_pos).begin(),
                std::next(view.begin(), m_text_cursor_pos + length).begin()
            );

            pos2 = std::next(view.begin(), m_text_cursor_pos + length).begin() - m_text_input.begin();
            m_text_input = m_text_input
                .substr(0, pos)
                .append(m_text_input.substr(pos2));
        }
        else
        {
            actual_length = std::distance(
                std::next(view.begin(), m_text_cursor_pos - length).begin(),
                std::next(view.begin(), m_text_cursor_pos).begin()
            );

            pos2 = std::next(view.begin(), m_text_cursor_pos - length).begin() - m_text_input.begin();
            m_text_input = m_text_input
                .substr(0, pos2)
                .append(m_text_input.substr(pos));

            m_text_cursor_pos -= actual_length;
        }

        return actual_length;
    }

    int32_t Window_SDL::removeInputText(uint32_t length, uint32_t pos)
    {
        auto view = una::views::grapheme::utf8(m_text_input);
        if (pos > std::distance(view.begin(), view.end()))
            return -1;

        auto actual_length = std::distance(
            std::next(view.begin(), pos).begin(),
            std::next(view.begin(), pos + length).begin()
        );

        auto pos2 = std::next(view.begin(), length).begin() - m_text_input.begin();

        m_text_input = m_text_input
            .substr(0, pos)
            .append(m_text_input.substr(pos2));

        return actual_length;
    }

    void Window_SDL::setTextInputReturnEnable(bool enable)
    {
        m_return_enable = enable;
    }

    void Window_SDL::setTextInputRect(RectI rect)
    {
        SDL_Rect rc{ rect.a.x, rect.a.y, rect.b.x - rect.a.x, rect.b.y - rect.a.y };
        SDL_SetTextInputRect(&rc);
    }


    std::string Window_SDL::getClipboardText()
    {
        char* clipboard = SDL_GetClipboardText();
        std::string ret(clipboard);
        SDL_free(clipboard);
        return ret;
    }

    bool Window_SDL::setClipboardText(StringView text)
    {
        if (SDL_SetClipboardText(std::string(text).c_str()) < 0)
            return false;
        return true;
    }


    Window_SDL::Window_SDL()
    {
        InitializeConfigure config;
        config.loadFromFile("config.json");
        if (!createWindow())
            throw std::runtime_error("createWindow failed");
    }
    Window_SDL::~Window_SDL()
    {
        destroyWindow();
    }

    bool Window_SDL::create(Window_SDL** pp_window)
    {
        try
        {
            *pp_window = new Window_SDL();
            return true;
        }
        catch (...)
        {
            *pp_window = nullptr;
            return false;
        }
    }
    bool Window_SDL::create(Vector2U size, StringView title_text, WindowFrameStyle style, bool show, Window_SDL** pp_window)
    {
        try
        {
            auto* p = new Window_SDL();
            *pp_window = p;
            p->setSize(size);
            p->setTitleText(title_text);
            p->setFrameStyle(style);
            p->setTextInputEnable(false);
            if (show)
                p->setLayer(WindowLayer::Normal);
            return true;
        }
        catch (...)
        {
            *pp_window = nullptr;
            return false;
        }
    }

    bool IWindow::create(IWindow** pp_window)
    {
        try
        {
            *pp_window = new Window_SDL();
            return true;
        }
        catch (...)
        {
            *pp_window = nullptr;
            return false;
        }
    }
    bool IWindow::create(Vector2U size, StringView title_text, WindowFrameStyle style, bool show, IWindow** pp_window)
    {
        try
        {
            auto* p = new Window_SDL();
            *pp_window = p;
            p->setSize(size);
            p->setTitleText(title_text);
            p->setFrameStyle(style);
            p->setTextInputEnable(false);
            if (show)
                p->setLayer(WindowLayer::Normal);
            return true;
        }
        catch (...)
        {
            *pp_window = nullptr;
            return false;
        }
    }
}
