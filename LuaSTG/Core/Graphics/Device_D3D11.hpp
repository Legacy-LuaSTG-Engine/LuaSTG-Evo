﻿#pragma once
#include "Core/Object.hpp"
#include "Core/Graphics/Device.hpp"

namespace LuaSTG::Core::Graphics
{
	class Device_D3D11 : public Object<IDevice>
	{
	private:
		// DXGI

		HMODULE dxgi_dll{ NULL };
		decltype(::CreateDXGIFactory1)* dxgi_api_CreateDXGIFactory1{ NULL };
		decltype(::CreateDXGIFactory2)* dxgi_api_CreateDXGIFactory2{ NULL };

		Microsoft::WRL::ComPtr<IDXGIFactory1> dxgi_factory;
		Microsoft::WRL::ComPtr<IDXGIFactory2> dxgi_factory2;
		Microsoft::WRL::ComPtr<IDXGIAdapter1> dxgi_adapter;

		std::string preferred_adapter_name;

		std::string dxgi_adapter_name;
		std::vector<std::string> dxgi_adapter_names;

		DWORD dwm_acceleration_level{ 0 };
		BOOL dxgi_support_flip_model{ FALSE };
		BOOL dxgi_support_low_latency{ FALSE };
		BOOL dxgi_support_flip_model2{ FALSE };
		BOOL dxgi_support_tearing{ FALSE };

		// Direct3D

		D3D_FEATURE_LEVEL d3d_feature_level{ D3D_FEATURE_LEVEL_10_0 };

		// Direct3D 11

		HMODULE d3d11_dll{ NULL };
		decltype(::D3D11CreateDevice)* d3d11_api_D3D11CreateDevice{ NULL };

		Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device;
		Microsoft::WRL::ComPtr<ID3D11Device1> d3d11_device1;
		Microsoft::WRL::ComPtr<ID3D11DeviceContext> d3d11_devctx;
		Microsoft::WRL::ComPtr<ID3D11DeviceContext1> d3d11_devctx1;

		// Window Image Component

		Microsoft::WRL::ComPtr<IWICImagingFactory> wic_factory;
		Microsoft::WRL::ComPtr<IWICImagingFactory2> wic_factory2;

		// Direct2D 1
		
		HMODULE d2d1_dll{ NULL };
		HRESULT(WINAPI* d2d1_api_D2D1CreateFactory)(D2D1_FACTORY_TYPE, REFIID, CONST D2D1_FACTORY_OPTIONS*, void**) { NULL };

		Microsoft::WRL::ComPtr<ID2D1Factory> d2d1_factory;
		Microsoft::WRL::ComPtr<ID2D1Factory1> d2d1_factory1;
		Microsoft::WRL::ComPtr<ID2D1Device> d2d1_device;
		Microsoft::WRL::ComPtr<ID2D1DeviceContext> d2d1_devctx;

		// DirectWrite

		HMODULE dwrite_dll{ NULL };
		decltype(DWriteCreateFactory)* dwrite_api_DWriteCreateFactory{ NULL };

		Microsoft::WRL::ComPtr<IDWriteFactory> dwrite_factory;

	public:
		// Get API

		IDXGIFactory1* GetDXGIFactory1() const noexcept { return dxgi_factory.Get(); }
		IDXGIFactory2* GetDXGIFactory2() const noexcept { return dxgi_factory2.Get(); }
		IDXGIAdapter1* GetDXGIAdapter1() const noexcept { return dxgi_adapter.Get(); }

		void SetPreferredAdapter(std::string_view name) { preferred_adapter_name = name; }
		std::string_view GetAdapterName() const noexcept { return dxgi_adapter_name; }
		std::vector<std::string> GetAdapterNameArray() const { return dxgi_adapter_names; }

		D3D_FEATURE_LEVEL GetD3DFeatureLevel() const noexcept { return d3d_feature_level; }

		ID3D11Device* GetD3D11Device() const noexcept { return d3d11_device.Get(); }
		ID3D11Device1* GetD3D11Device1() const noexcept { return d3d11_device1.Get(); }
		ID3D11DeviceContext* GetD3D11DeviceContext() const noexcept { return d3d11_devctx.Get(); }
		ID3D11DeviceContext1* GetD3D11DeviceContext1() const noexcept { return d3d11_devctx1.Get(); }

		BOOL IsFlipSequentialSupport() const noexcept { return dxgi_support_flip_model; }
		BOOL IsFrameLatencySupport() const noexcept { return dxgi_support_low_latency; }
		BOOL IsFlipDiscardSupport() const noexcept { return dxgi_support_flip_model2; }
		BOOL IsTearingSupport() const noexcept { return dxgi_support_tearing; }

	private:
		bool loadDLL();
		void unloadDLL();
		bool selectAdapter();
		bool createDXGI();
		void destroyDXGI();
		bool createD3D11();
		void destroyD3D11();
		bool createWIC();
		void destroyWIC();
		bool createD2D1();
		void destroyD2D1();
		bool createDWrite();
		void destroyDWrite();
		bool doDestroyAndCreate();

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

	public:
		Device_D3D11(std::string_view const& prefered_gpu = "");
		~Device_D3D11();
	};
}