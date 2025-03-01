﻿#pragma once
#include "Core/Object.hpp"
#include "Core/Graphics/Device.hpp"
#include "Core/Type.hpp"
#include "glad/gl.h"
#include "SDL.h"
#include <cstddef>
#include <cstdint>
#include <optional>
// #include "Platform/RuntimeLoader/DXGI.hpp"
// #include "Platform/RuntimeLoader/Direct3D11.hpp"
// #include "Platform/RuntimeLoader/Direct2D1.hpp"
// #include "Platform/RuntimeLoader/DirectWrite.hpp"

namespace Core::Graphics
{
	class Device_OpenGL : public Object<IDevice>
	{
	private:
		enum class EventType
		{
			DeviceCreate,
			DeviceDestroy,
		};
		bool m_is_dispatch_event{ false };
		std::vector<IDeviceEventListener*> m_eventobj;
		std::vector<IDeviceEventListener*> m_eventobj_late;
	private:
		void dispatchEvent(EventType t);
	public:
		void addEventListener(IDeviceEventListener* e);
		void removeEventListener(IDeviceEventListener* e);

		bool recreate();

		void* getNativeHandle() { return nullptr; }
		void* getNativeRendererHandle() { return SDL_GL_GetCurrentContext(); }

		bool createTextureFromFile(StringView path, bool mipmap, ITexture2D** pp_texutre);
		bool createTextureFromMemory(void const* data, size_t size, bool mipmap, ITexture2D** pp_texutre);
		bool createTexture(Vector2U size, ITexture2D** pp_texutre);

		bool createRenderTarget(Vector2U size, IRenderTarget** pp_rt);
		bool createDepthStencilBuffer(Vector2U size, IDepthStencilBuffer** pp_ds);

	public:
		Device_OpenGL();
		~Device_OpenGL();

	public:
		static bool create(Device_OpenGL** p_device);
	};

	class Texture2D_OpenGL
		: public Object<ITexture2D>
		, public IDeviceEventListener
	{
	private:
		ScopeObject<Device_OpenGL> m_device;
		std::optional<SamplerState> m_sampler;
		ScopeObject<IData> m_data;
		std::string source_path;
		GLuint opengl_texture2d = 0;
		Vector2U m_size{};
		bool m_dynamic{ false };
		bool m_premul{ false };
		bool m_mipmap{ false };
		bool m_isrt{ false };

	public:
		void onDeviceCreate();
		void onDeviceDestroy();

		bool createResource();

	public:
		GLuint GetResource() { return opengl_texture2d; }
		GLuint GetView() { return opengl_texture2d; }

	public:
		void* getNativeHandle() { return (void*)(intptr_t)opengl_texture2d; }

		bool isDynamic() { return m_dynamic; }
		bool isPremultipliedAlpha() { return m_premul; }
		void setPremultipliedAlpha(bool v) { m_premul = v; }
		Vector2U getSize() { return m_size; }
		bool setSize(Vector2U size);

		bool uploadPixelData(RectU rc, void const* data, uint32_t pitch);
		void setPixelData(IData* p_data) { m_data = p_data; }

		bool saveToFile(StringView path);

		void setSamplerState(SamplerState sampler) { m_sampler = sampler; }
		std::optional<SamplerState> getSamplerState() { return m_sampler; }

	public:
		Texture2D_OpenGL(Device_OpenGL* device, StringView path, bool mipmap);
		Texture2D_OpenGL(Device_OpenGL* device, void const* data, size_t size, bool mipmap);
		Texture2D_OpenGL(Device_OpenGL* device, Vector2U size, bool rendertarget); // if rendertarget, then hand over control to RenderTarget_OpenGL
		~Texture2D_OpenGL();
	};

	class DepthStencilBuffer_OpenGL
		: public Object<IDepthStencilBuffer>
		, public IDeviceEventListener
	{
	private:
		ScopeObject<Device_OpenGL> m_device;
		GLuint opengl_depthstencilbuffer = 0;
		Vector2U m_size{};

	public:
		void onDeviceCreate();
		void onDeviceDestroy();

		bool createResource();

	public:
		GLuint GetResource() { return opengl_depthstencilbuffer; }
		GLuint GetView() { return opengl_depthstencilbuffer; }

	public:
		void* getNativeHandle() { return (void*)(intptr_t)opengl_depthstencilbuffer; }

		bool setSize(Vector2U size);
		Vector2U getSize() { return m_size; }

	public:
		DepthStencilBuffer_OpenGL(Device_OpenGL* device, Vector2U size);
		~DepthStencilBuffer_OpenGL();
	};

	class RenderTarget_OpenGL
		: public Object<IRenderTarget>
		, public IDeviceEventListener
	{
	private:
		ScopeObject<Device_OpenGL> m_device;
		ScopeObject<Texture2D_OpenGL> m_texture;
		ScopeObject<DepthStencilBuffer_OpenGL> m_depthstencilbuffer;
		GLuint opengl_framebuffer = 0;
		bool m_depthstencilbuffer_enable = false;

	public:
		void onDeviceCreate();
		void onDeviceDestroy();

		bool createResource();

	public:
		GLuint GetFramebuffer() { return opengl_framebuffer; }

	public:
		bool DepthStencilBufferEnabled() { return m_depthstencilbuffer_enable; }

	public:
		void* getNativeHandle() { return (void*)(intptr_t)opengl_framebuffer; }

		bool setSize(Vector2U size);
		ITexture2D* getTexture() { return *m_texture; }

	public:
		RenderTarget_OpenGL(Device_OpenGL* device, Vector2U size);
		~RenderTarget_OpenGL();
	};
}
