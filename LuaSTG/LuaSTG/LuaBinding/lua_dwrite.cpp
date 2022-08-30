#include "lua_dwrite.hpp"

#include <cassert>
#include <string>
#include <string_view>
#include <vector>
#include <unordered_map>
#include <filesystem>
#include <memory>
#include <functional>

#include "utility/encoding.hpp"
#include "platform/HResultChecker.hpp"
#include "Core/FileManager.hpp"
#include "AppFrame.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define NOSERVICE
#define NOMCX
#define NOIME

#ifdef _WIN32_WINNT
#undef _WIN32_WINNT
#endif
#ifdef NTDDI_VERSION
#undef NTDDI_VERSION
#endif
#ifdef WINVER
#undef WINVER
#endif

#include <sdkddkver.h>

#include <Windows.h>
#include <wrl/client.h>
#include <wrl/wrappers/corewrappers.h>

#include <wincodec.h>
#include <d2d1_3.h>
#include <dwrite_3.h>

#undef WIN32_LEAN_AND_MEAN
#undef NOMINMAX
#undef NOSERVICE
#undef NOMCX
#undef NOIME

namespace DirectWrite
{
	// c++ helper

	struct ScopeFunction
	{
		std::function<void()> f;
		ScopeFunction(std::function<void()> fv) : f(fv) {}
		~ScopeFunction() { f(); }
	};

	// lua helper

	inline std::string_view luaL_check_string_view(lua_State* L, int idx)
	{
		size_t len = 0;
		char const* str = luaL_checklstring(L, idx, &len);
		return std::string_view(str, len);
	}

	template<typename E>
	inline E luaL_check_C_enum(lua_State* L, int idx)
	{
		return static_cast<E>(luaL_checkinteger(L, idx));
	}

	inline float luaL_check_float(lua_State* L, int idx)
	{
		return (float)luaL_checknumber(L, idx);
	}
	inline float luaL_optional_float(lua_State* L, int idx, float v)
	{
		return (float)luaL_optnumber(L, idx, v);
	}
	inline uint32_t luaL_check_uint32(lua_State* L, int idx)
	{
		if constexpr (sizeof(lua_Integer) >= 8)
		{
			return 0xFFFFFFFFu & luaL_checkinteger(L, idx);
		}
		else
		{
			return (uint32_t)luaL_checknumber(L, idx);
		}
	}

	// DirectWrite helper

	template<typename T>
	class UnknownImplement : public T
	{
	private:
		volatile unsigned long m_ref;
	public:
		HRESULT WINAPI QueryInterface(IID const& riid, void** ppvObject)
		{
			if (riid == __uuidof(IUnknown))
			{
				AddRef();
				*ppvObject = static_cast<IUnknown*>(this);
				return S_OK;
			}
			else if (riid == __uuidof(T))
			{
				AddRef();
				*ppvObject = static_cast<T*>(this);
				return S_OK;
			}
			else
			{
				return E_NOINTERFACE;
			}
		}
		ULONG WINAPI AddRef()
		{
			return InterlockedIncrement(&m_ref);
		}
		ULONG WINAPI Release()
		{
			ULONG const ref_count = InterlockedDecrement(&m_ref);
			if (ref_count == 0u)
			{
				delete this;
			}
			return ref_count;
		}
	public:
		UnknownImplement() : m_ref(1) {}
		virtual ~UnknownImplement() {}
	};

	class DWriteFontFileStreamImplement : public UnknownImplement<IDWriteFontFileStream>
	{
	private:
		std::vector<uint8_t> m_data;
	public:
		HRESULT WINAPI ReadFileFragment(void const** fragmentStart, UINT64 fileOffset, UINT64 fragmentSize, void** fragmentContext)
		{
			assert(fragmentStart);
			assert(fragmentContext);
			assert(fileOffset <= UINT32_MAX && fragmentSize <= UINT32_MAX && (fileOffset + fragmentSize) <= UINT32_MAX); // only files smaller than 4GB are supported
			if ((fileOffset + fragmentSize) > m_data.size()) return E_INVALIDARG;
			*fragmentStart = m_data.data() + fileOffset;
			*fragmentContext = m_data.data() + fileOffset; // for identification only
			return S_OK;
		}
		void WINAPI ReleaseFileFragment(void* fragmentContext)
		{
			UNREFERENCED_PARAMETER(fragmentContext);
			// no additional heap memory to free
		}
		HRESULT WINAPI GetFileSize(UINT64* fileSize)
		{
			assert(fileSize);
			*fileSize = m_data.size();
			return S_OK; // always succeed
		}
		HRESULT WINAPI GetLastWriteTime(UINT64* lastWriteTime)
		{
			UNREFERENCED_PARAMETER(lastWriteTime);
			return E_NOTIMPL; // always failed (not applicable for in-memory font files)
		}
	public:
		bool loadFromFile(std::string_view const path)
		{
			std::wstring wide_path(utility::encoding::to_wide(path)); // OOM catch by factory
			Microsoft::WRL::Wrappers::FileHandle file;
			file.Attach(CreateFileW(
				wide_path.c_str(),
				FILE_GENERIC_READ,
				FILE_SHARE_READ,
				NULL,
				OPEN_EXISTING,
				FILE_ATTRIBUTE_NORMAL,
				NULL
			));
			if (!file.IsValid())
			{
				return false;
			}
			LARGE_INTEGER size = {};
			if (!GetFileSizeEx(file.Get(), &size))
			{
				return false;
			}
			if (size.QuadPart > (LONGLONG)UINT32_MAX)
			{
				assert(false);
				return false;
			}
			m_data.resize(size.LowPart); // OOM catch by factory
			DWORD read_size = 0;
			if (!ReadFile(file.Get(), m_data.data(), size.LowPart, &read_size, NULL))
			{
				return false;
			}
			if (read_size != size.LowPart)
			{
				assert(false);
				return false;
			}
			return true;
		}
		bool loadFromFileManager(std::string_view const path)
		{
			return GFileManager().loadEx(path, m_data); // OOM catch by factory
		}
	public:
		DWriteFontFileStreamImplement() {}
		virtual ~DWriteFontFileStreamImplement() {}
	};

	class DWriteFontFileLoaderImplement : public UnknownImplement<IDWriteFontFileLoader>
	{
	private:
		std::unordered_map<std::string, Microsoft::WRL::ComPtr<DWriteFontFileStreamImplement>> m_cache;
	public:
		HRESULT WINAPI CreateStreamFromKey(void const* fontFileReferenceKey, UINT32 fontFileReferenceKeySize, IDWriteFontFileStream** fontFileStream)
		{
			assert(fontFileReferenceKey && fontFileReferenceKeySize > 0);
			assert(fontFileStream);
			try
			{
				std::string path((char*)fontFileReferenceKey, fontFileReferenceKeySize);
				auto it = m_cache.find(path);
				if (it != m_cache.end())
				{
					it->second->AddRef();
					*fontFileStream = it->second.Get();
					return S_OK;
				}

				Microsoft::WRL::ComPtr<DWriteFontFileStreamImplement> object;
				object.Attach(new DWriteFontFileStreamImplement());
				if (!object->loadFromFileManager(path))
					return E_FAIL;

				object->AddRef();
				*fontFileStream = object.Get();

				m_cache.emplace(std::move(path), std::move(object));
				return S_OK;
			}
			catch (...)
			{
				return E_FAIL;
			}
		}
	public:
		DWriteFontFileLoaderImplement() {}
		virtual ~DWriteFontFileLoaderImplement() {}
	};

	using shared_string_list = std::shared_ptr<std::vector<std::string>>;

	class DWriteFontFileEnumeratorImplement : public UnknownImplement<IDWriteFontFileEnumerator>
	{
	private:
		Microsoft::WRL::ComPtr<IDWriteFactory> m_dwrite_factory;
		Microsoft::WRL::ComPtr<IDWriteFontFileLoader> m_dwrite_font_file_loader;
		shared_string_list m_font_file_name_list;
		LONG m_index{};
	public:
		HRESULT WINAPI MoveNext(BOOL* hasCurrentFile)
		{
			assert(hasCurrentFile);
			assert(m_font_file_name_list);
			m_index += 1;
			if (m_index >= 0 && m_index < (LONG)m_font_file_name_list->size())
			{
				*hasCurrentFile = TRUE;
			}
			else
			{
				*hasCurrentFile = FALSE;
			}
			return S_OK;
		}
		HRESULT WINAPI GetCurrentFontFile(IDWriteFontFile** fontFile)
		{
			assert(fontFile);
			assert(m_font_file_name_list);
			assert(m_index >= 0 && m_index < (LONG)m_font_file_name_list->size());
			if (m_index < 0 || m_index >(LONG)m_font_file_name_list->size())
			{
				return E_FAIL;
			}
			std::string const& path = m_font_file_name_list->at((size_t)m_index);
			if (GFileManager().containEx(path))
			{
				return m_dwrite_factory->CreateCustomFontFileReference(
					path.data(),
					(UINT32)path.size(),
					m_dwrite_font_file_loader.Get(),
					fontFile
				);
			}
			else
			{
				std::wstring wide_path(utility::encoding::to_wide(path));
				return m_dwrite_factory->CreateFontFileReference(
					wide_path.c_str(),
					NULL,
					fontFile
				);
			}
		}
	public:
		void reset(IDWriteFactory* factory, IDWriteFontFileLoader* loader, shared_string_list list)
		{
			assert(factory);
			assert(loader);
			m_dwrite_factory = factory;
			m_dwrite_font_file_loader = loader;
			m_font_file_name_list = list; // bulk copy operations, OOM catch by factory
			m_index = -1;
		}
	public:
		DWriteFontFileEnumeratorImplement() {}
		virtual ~DWriteFontFileEnumeratorImplement() {}
	};

	class DWriteFontCollectionLoaderImplement : public UnknownImplement<IDWriteFontCollectionLoader>
	{
	private:
		Microsoft::WRL::ComPtr<IDWriteFactory> m_dwrite_factory;
		Microsoft::WRL::ComPtr<IDWriteFontFileLoader> m_dwrite_font_file_loader;
		shared_string_list m_font_file_name_list;
	public:
		HRESULT WINAPI CreateEnumeratorFromKey(IDWriteFactory* factory, void const* collectionKey, UINT32 collectionKeySize, IDWriteFontFileEnumerator** fontFileEnumerator)
		{
			assert(m_dwrite_factory);
			assert(m_dwrite_font_file_loader);
			assert(m_font_file_name_list);
			assert(collectionKey || collectionKeySize == 0);
			assert(factory);
			assert(fontFileEnumerator);
			//if (std::string_view((char*)collectionKey, collectionKeySize) != "?")
			//	return E_INVALIDARG;
			try
			{
				Microsoft::WRL::ComPtr<DWriteFontFileEnumeratorImplement> object;
				object.Attach(new DWriteFontFileEnumeratorImplement());
				object->reset(
					m_dwrite_factory.Get(),
					m_dwrite_font_file_loader.Get(),
					m_font_file_name_list
				);
				*fontFileEnumerator = object.Detach();
				return S_OK;
			}
			catch (std::exception const&)
			{
				return E_OUTOFMEMORY;
			}
		}
	public:
		void reset(IDWriteFactory* factory, IDWriteFontFileLoader* loader, shared_string_list list)
		{
			assert(factory);
			assert(loader);
			m_dwrite_factory = factory;
			m_dwrite_font_file_loader = loader;
			m_font_file_name_list = list;
		}
	public:
		DWriteFontCollectionLoaderImplement() {}
		virtual ~DWriteFontCollectionLoaderImplement() {}
	};

	static struct ModuleLoader
	{
		HMODULE dll_d2d1;
		HMODULE dll_dwrite;
		HRESULT(WINAPI* api_D2D1CreateFactory)(D2D1_FACTORY_TYPE, REFIID, CONST D2D1_FACTORY_OPTIONS*, void**);
		HRESULT(WINAPI* api_DWriteCreateFactory)(DWRITE_FACTORY_TYPE, REFIID, IUnknown**);

		ModuleLoader()
			: dll_d2d1(NULL)
			, dll_dwrite(NULL)
			, api_D2D1CreateFactory(NULL)
			, api_DWriteCreateFactory(NULL)
		{
			dll_d2d1 = LoadLibraryW(L"d2d1.dll");
			dll_dwrite = LoadLibraryW(L"dwrite.dll");
			if (dll_d2d1)
				api_D2D1CreateFactory = (decltype(api_D2D1CreateFactory))GetProcAddress(dll_d2d1, "D2D1CreateFactory");
			if (dll_dwrite)
				api_DWriteCreateFactory = (decltype(api_DWriteCreateFactory))GetProcAddress(dll_dwrite, "DWriteCreateFactory");
			assert(api_D2D1CreateFactory);
			assert(api_DWriteCreateFactory);
		}
		~ModuleLoader()
		{
			if (dll_d2d1) FreeLibrary(dll_d2d1);
			if (dll_dwrite) FreeLibrary(dll_dwrite);
			dll_d2d1 = NULL;
			dll_dwrite = NULL;
			api_D2D1CreateFactory = NULL;
			api_DWriteCreateFactory = NULL;
		}
	} DLL;

	// DirectWrite renderer

	class DWriteTextRendererImplement : public IDWriteTextRenderer
	{
	private:
		Microsoft::WRL::ComPtr<ID2D1Factory> d2d1_factory;
		Microsoft::WRL::ComPtr<ID2D1RenderTarget> d2d1_rt;
		Microsoft::WRL::ComPtr<ID2D1Brush> d2d1_brush_outline;
		Microsoft::WRL::ComPtr<ID2D1Brush> d2d1_brush_fill;
		FLOAT outline_width;
	public:
		HRESULT WINAPI QueryInterface(IID const& riid, void** ppvObject)
		{
			if (riid == __uuidof(IUnknown))
			{
				AddRef();
				*ppvObject = this;
				return S_OK;
			}
			else if (riid == __uuidof(IDWritePixelSnapping))
			{
				AddRef();
				*ppvObject = this;
				return S_OK;
			}
			else if (riid == __uuidof(IDWriteTextRenderer))
			{
				AddRef();
				*ppvObject = this;
				return S_OK;
			}
			else
			{
				return E_NOINTERFACE;
			}
		}
		ULONG WINAPI AddRef() { return 2; }
		ULONG WINAPI Release() { return 1; }
	public:
		HRESULT WINAPI IsPixelSnappingDisabled(void* clientDrawingContext, BOOL* isDisabled)
		{
			UNREFERENCED_PARAMETER(clientDrawingContext);
			*isDisabled = FALSE; // recommended default value
			return S_OK;
		}
		HRESULT WINAPI GetCurrentTransform(void* clientDrawingContext, DWRITE_MATRIX* transform)
		{
			UNREFERENCED_PARAMETER(clientDrawingContext);
			// forward the render target's transform
			d2d1_rt->GetTransform(reinterpret_cast<D2D1_MATRIX_3X2_F*>(transform));
			return S_OK;
		}
		HRESULT WINAPI GetPixelsPerDip(void* clientDrawingContext, FLOAT* pixelsPerDip)
		{
			UNREFERENCED_PARAMETER(clientDrawingContext);
			float x = 0.0f, y = 0.0f;
			d2d1_rt->GetDpi(&x, &y);
			*pixelsPerDip = x / 96.0f;
			return S_OK;
		}
	public:
		HRESULT WINAPI DrawGlyphRun(
			void* clientDrawingContext,
			FLOAT baselineOriginX,
			FLOAT baselineOriginY,
			DWRITE_MEASURING_MODE measuringMode,
			DWRITE_GLYPH_RUN const* glyphRun,
			DWRITE_GLYPH_RUN_DESCRIPTION const* glyphRunDescription,
			IUnknown* clientDrawingEffect)
		{
			UNREFERENCED_PARAMETER(clientDrawingContext);
			UNREFERENCED_PARAMETER(measuringMode);
			UNREFERENCED_PARAMETER(glyphRunDescription);
			UNREFERENCED_PARAMETER(clientDrawingEffect);

			HRESULT hr = S_OK;

			// Create the path geometry.

			Microsoft::WRL::ComPtr<ID2D1PathGeometry> d2d1_path_geometry;
			hr = gHR = d2d1_factory->CreatePathGeometry(&d2d1_path_geometry);
			if (FAILED(hr)) return hr;

			// Write to the path geometry using the geometry sink.

			Microsoft::WRL::ComPtr<ID2D1GeometrySink> d2d1_geometry_sink;
			hr = gHR = d2d1_path_geometry->Open(&d2d1_geometry_sink);
			if (FAILED(hr)) return hr;

			hr = gHR = glyphRun->fontFace->GetGlyphRunOutline(
				glyphRun->fontEmSize,
				glyphRun->glyphIndices,
				glyphRun->glyphAdvances,
				glyphRun->glyphOffsets,
				glyphRun->glyphCount,
				glyphRun->isSideways,
				glyphRun->bidiLevel % 2,
				d2d1_geometry_sink.Get());
			if (FAILED(hr)) return hr;

			hr = gHR = d2d1_geometry_sink->Close();
			if (FAILED(hr)) return hr;

			D2D1::Matrix3x2F const matrix = D2D1::Matrix3x2F(
				1.0f, 0.0f,
				0.0f, 1.0f,
				baselineOriginX, baselineOriginY
			);
			Microsoft::WRL::ComPtr<ID2D1TransformedGeometry> d2d1_transformed_geometry;
			hr = gHR = d2d1_factory->CreateTransformedGeometry(
				d2d1_path_geometry.Get(),
				&matrix,
				&d2d1_transformed_geometry);
			if (FAILED(hr)) return hr;

			// Draw the outline of the glyph run

			d2d1_rt->DrawGeometry(d2d1_transformed_geometry.Get(), d2d1_brush_outline.Get(), outline_width);

			// Fill in the glyph run

			d2d1_rt->FillGeometry(d2d1_transformed_geometry.Get(), d2d1_brush_fill.Get());

			return S_OK;
		}
		HRESULT WINAPI DrawUnderline(
			void* clientDrawingContext,
			FLOAT baselineOriginX,
			FLOAT baselineOriginY,
			DWRITE_UNDERLINE const* underline,
			IUnknown* clientDrawingEffect)
		{
			UNREFERENCED_PARAMETER(clientDrawingContext);
			UNREFERENCED_PARAMETER(baselineOriginX);
			UNREFERENCED_PARAMETER(baselineOriginY);
			UNREFERENCED_PARAMETER(underline);
			UNREFERENCED_PARAMETER(clientDrawingEffect);
			return E_NOTIMPL;
		}
		HRESULT WINAPI DrawStrikethrough(
			void* clientDrawingContext,
			FLOAT baselineOriginX,
			FLOAT baselineOriginY,
			DWRITE_STRIKETHROUGH const* strikethrough,
			IUnknown* clientDrawingEffect)
		{
			UNREFERENCED_PARAMETER(clientDrawingContext);
			UNREFERENCED_PARAMETER(baselineOriginX);
			UNREFERENCED_PARAMETER(baselineOriginY);
			UNREFERENCED_PARAMETER(strikethrough);
			UNREFERENCED_PARAMETER(clientDrawingEffect);
			return E_NOTIMPL;
		}
		HRESULT WINAPI DrawInlineObject(
			void* clientDrawingContext,
			FLOAT originX,
			FLOAT originY,
			IDWriteInlineObject* inlineObject,
			BOOL isSideways,
			BOOL isRightToLeft,
			IUnknown* clientDrawingEffect)
		{
			UNREFERENCED_PARAMETER(clientDrawingContext);
			UNREFERENCED_PARAMETER(originX);
			UNREFERENCED_PARAMETER(originY);
			UNREFERENCED_PARAMETER(inlineObject);
			UNREFERENCED_PARAMETER(isSideways);
			UNREFERENCED_PARAMETER(isRightToLeft);
			UNREFERENCED_PARAMETER(clientDrawingEffect);
			return E_NOTIMPL;
		}
	public:
		DWriteTextRendererImplement(ID2D1Factory* factory, ID2D1RenderTarget* target, ID2D1Brush* outline, ID2D1Brush* fill, FLOAT width)
			: d2d1_factory(factory), d2d1_rt(target), d2d1_brush_outline(outline), d2d1_brush_fill(fill), outline_width(width) {}
		~DWriteTextRendererImplement() {}
	};

	// lib

	static int LUA_KEY = 0;

	struct FontCollection
	{
		static std::string_view const ClassID;

		std::string name;
		Microsoft::WRL::ComPtr<IDWriteFactory> dwrite_factory;
		Microsoft::WRL::ComPtr<DWriteFontFileLoaderImplement> dwrite_font_file_loader; // from core
		Microsoft::WRL::ComPtr<DWriteFontCollectionLoaderImplement> dwrite_font_collection_loader;
		Microsoft::WRL::ComPtr<IDWriteFontCollection> dwrite_font_collection;
		shared_string_list font_file_name_list;

		void _test()
		{
		}

		bool InitComponents()
		{
			HRESULT hr = S_OK;

			std::stringstream ss;
			ss << this;
			name = ss.str();

			dwrite_font_collection_loader.Attach(new DWriteFontCollectionLoaderImplement());
			dwrite_font_collection_loader->reset(
				dwrite_factory.Get(),
				dwrite_font_file_loader.Get(),
				font_file_name_list);

			hr = gHR = dwrite_factory->RegisterFontCollectionLoader(dwrite_font_collection_loader.Get());
			if (FAILED(hr)) return false;

			hr = gHR = dwrite_factory->CreateCustomFontCollection(
				dwrite_font_collection_loader.Get(),
				name.data(),
				(UINT32)name.size(),
				&dwrite_font_collection);
			if (FAILED(hr)) return false;

			return true;
		}

		FontCollection() {}
		~FontCollection()
		{
			if (dwrite_factory && dwrite_font_collection_loader)
			{
				gHR = dwrite_factory->UnregisterFontCollectionLoader(dwrite_font_collection_loader.Get());
			}
		}

		static int api___tostring(lua_State* L)
		{
			Cast(L, 1);
			lua_pushlstring(L, ClassID.data(), ClassID.size());
			return 1;
		}
		static int api___gc(lua_State* L)
		{
			FontCollection* self = Cast(L, 1);
			self->~FontCollection();
			return 0;
		}

		static FontCollection* Cast(lua_State* L, int idx)
		{
			return (FontCollection*)luaL_checkudata(L, idx, ClassID.data());
		}
		static FontCollection* Create(lua_State* L)
		{
			FontCollection* self = (FontCollection*)lua_newuserdata(L, sizeof(FontCollection));
			new(self) FontCollection();
			luaL_getmetatable(L, ClassID.data()); // ??? udata mt
			lua_setmetatable(L, -2);              // ??? udata
			return self;
		}
		static void Register(lua_State* L)
		{
			luaL_Reg const mt[] = {
				{ "__tostring", &api___tostring },
				{ "__gc", &api___gc },
				{ NULL, NULL },
			};
			luaL_newmetatable(L, ClassID.data());
			luaL_register(L, NULL, mt);
			lua_pop(L, 1);
		}
	};
	std::string_view const FontCollection::ClassID("DirectWrite.FontCollection");

	struct TextFormat
	{
		static std::string_view const ClassID;

		Microsoft::WRL::ComPtr<IDWriteTextFormat> dwrite_text_format;

		void _test()
		{
		}

		TextFormat() {}
		~TextFormat() {}

		static int api___tostring(lua_State* L)
		{
			Cast(L, 1);
			lua_pushlstring(L, ClassID.data(), ClassID.size());
			return 1;
		}
		static int api___gc(lua_State* L)
		{
			TextFormat* self = Cast(L, 1);
			self->~TextFormat();
			return 0;
		}

		static TextFormat* Cast(lua_State* L, int idx)
		{
			return (TextFormat*)luaL_checkudata(L, idx, ClassID.data());
		}
		static TextFormat* Create(lua_State* L)
		{
			TextFormat* self = (TextFormat*)lua_newuserdata(L, sizeof(TextFormat));
			new(self) TextFormat();
			luaL_getmetatable(L, ClassID.data()); // ??? udata mt
			lua_setmetatable(L, -2);              // ??? udata
			return self;
		}
		static void Register(lua_State* L)
		{
			luaL_Reg const mt[] = {
				{ "__tostring", &api___tostring },
				{ "__gc", &api___gc },
				{ NULL, NULL },
			};
			luaL_newmetatable(L, ClassID.data());
			luaL_register(L, NULL, mt);
			lua_pop(L, 1);
		}
	};
	std::string_view const TextFormat::ClassID("DirectWrite.TextFormat");

	struct TextLayout
	{
		static std::string_view const ClassID;

		Microsoft::WRL::ComPtr<IDWriteTextLayout> dwrite_text_layout;

		void _test()
		{
		}

		TextLayout() {}
		~TextLayout() {}

		static int api___tostring(lua_State* L)
		{
			Cast(L, 1);
			lua_pushlstring(L, ClassID.data(), ClassID.size());
			return 1;
		}
		static int api___gc(lua_State* L)
		{
			TextLayout* self = Cast(L, 1);
			self->~TextLayout();
			return 0;
		}

		static TextLayout* Cast(lua_State* L, int idx)
		{
			return (TextLayout*)luaL_checkudata(L, idx, ClassID.data());
		}
		static TextLayout* Create(lua_State* L)
		{
			TextLayout* self = (TextLayout*)lua_newuserdata(L, sizeof(TextLayout));
			new(self) TextLayout();
			luaL_getmetatable(L, ClassID.data()); // ??? udata mt
			lua_setmetatable(L, -2);              // ??? udata
			return self;
		}
		static void Register(lua_State* L)
		{
			luaL_Reg const mt[] = {
				{ "__tostring", &api___tostring },
				{ "__gc", &api___gc },
				{ NULL, NULL },
			};
			luaL_newmetatable(L, ClassID.data());
			luaL_register(L, NULL, mt);
			lua_pop(L, 1);
		}
	};
	std::string_view const TextLayout::ClassID("DirectWrite.TextLayout");

	struct Factory
	{
		Microsoft::WRL::ComPtr<IWICImagingFactory> wic_factory;
		Microsoft::WRL::ComPtr<IDWriteFactory> dwrite_factory;
		Microsoft::WRL::ComPtr<ID2D1Factory> d2d1_factory;
		Microsoft::WRL::ComPtr<DWriteFontFileLoaderImplement> dwrite_font_file_loader;

		void _test()
		{
			
		}

		bool InitComponents()
		{
			HRESULT hr = S_OK;

			hr = gHR = CoCreateInstance(
				CLSID_WICImagingFactory,
				NULL,
				CLSCTX_INPROC_SERVER,
				IID_PPV_ARGS(&wic_factory)
			);
			if (FAILED(hr))
				return false;

			hr = gHR = DLL.api_DWriteCreateFactory(
				DWRITE_FACTORY_TYPE_SHARED,
				__uuidof(IDWriteFactory),
				&dwrite_factory
			);
			if (FAILED(hr))
				return false;

			D2D1_FACTORY_OPTIONS d2d1_options = {
			#ifdef _DEBUG
				.debugLevel = D2D1_DEBUG_LEVEL_INFORMATION,
			#else
				.debugLevel = D2D1_DEBUG_LEVEL_NONE,
			#endif
			};
			hr = gHR = DLL.api_D2D1CreateFactory(
				D2D1_FACTORY_TYPE_SINGLE_THREADED,
				__uuidof(ID2D1Factory),
				&d2d1_options,
				&d2d1_factory
			);
			if (FAILED(hr))
				return false;

			dwrite_font_file_loader.Attach(new DWriteFontFileLoaderImplement());
			hr = gHR = dwrite_factory->RegisterFontFileLoader(dwrite_font_file_loader.Get());
			if (FAILED(hr))
				return false;

			return true;
		}

		Factory() {}
		~Factory()
		{
			if (dwrite_factory && dwrite_font_file_loader)
			{
				gHR = dwrite_factory->UnregisterFontFileLoader(dwrite_font_file_loader.Get());
			}
		}

		static std::string_view const ClassID;

		static int api___tostring(lua_State* L)
		{
			Cast(L, 1);
			lua_pushlstring(L, ClassID.data(), ClassID.size());
			return 1;
		}
		static int api___gc(lua_State* L)
		{
			Factory* self = Cast(L, 1);
			self->~Factory();
			return 0;
		}

		static Factory* Cast(lua_State* L, int idx)
		{
			return (Factory*)luaL_checkudata(L, idx, ClassID.data());
		}
		static Factory* Create(lua_State* L)
		{
			Factory* self = (Factory*)lua_newuserdata(L, sizeof(Factory));
			new(self) Factory();
			luaL_getmetatable(L, ClassID.data()); // ??? udata mt
			lua_setmetatable(L, -2);              // ??? udata
			return self;
		}
		static void Register(lua_State* L)
		{
			luaL_Reg const mt[] = {
				{ "__tostring", &api___tostring },
				{ "__gc", &api___gc },
				{ NULL, NULL },
			};
			luaL_newmetatable(L, ClassID.data());
			luaL_register(L, NULL, mt);
			lua_pop(L, 1);
		}

		static Factory* Get(lua_State* L)
		{
			lua_pushlightuserdata(L, &LUA_KEY);
			lua_gettable(L, LUA_REGISTRYINDEX);
			Factory* core = Cast(L, -1);
			lua_pop(L, 1);
			return core;
		}
	};
	std::string_view const Factory::ClassID("DirectWrite.Factory");

	static int api_CreateFontCollection(lua_State* L)
	{
		luaL_argcheck(L, lua_istable(L, 1), 1, "");

		Factory* core = Factory::Get(L);
		FontCollection* font_collection = FontCollection::Create(L);

		font_collection->dwrite_factory = core->dwrite_factory;
		font_collection->dwrite_font_file_loader = core->dwrite_font_file_loader;
		font_collection->dwrite_font_collection_loader.Attach(new DWriteFontCollectionLoaderImplement());
		font_collection->font_file_name_list = std::make_shared<std::vector<std::string>>();
		size_t const file_count = lua_objlen(L, 1);
		font_collection->font_file_name_list->reserve(file_count);
		for (size_t i = 0; i < file_count; i += 1)
		{
			lua_rawgeti(L, 1, i + 1);
			auto const file_path = luaL_check_string_view(L, -1);
			font_collection->font_file_name_list->emplace_back(file_path);
			lua_pop(L, 1);
		}
		if (!font_collection->InitComponents())
			return luaL_error(L, "[DirectWrite.CreateFontCollection] init failed");

		return 1;
	}
	static int api_CreateTextFormat(lua_State* L)
	{
		auto const font_family_name = luaL_check_string_view(L, 1);
		FontCollection* font_collection{}; if (lua_isuserdata(L, 2)) font_collection = FontCollection::Cast(L, 2);
		auto const font_weight = luaL_check_C_enum<DWRITE_FONT_WEIGHT>(L, 3);
		auto const font_style = luaL_check_C_enum<DWRITE_FONT_STYLE>(L, 4);
		auto const font_stretch = luaL_check_C_enum<DWRITE_FONT_STRETCH>(L, 5);
		auto const font_size = luaL_check_float(L, 6);
		auto const locale_name = luaL_check_string_view(L, 7);

		std::wstring wide_font_family_name(utility::encoding::to_wide(font_family_name));
		std::wstring wide_locale_name(utility::encoding::to_wide(locale_name));

		Factory* core = Factory::Get(L);
		TextFormat* text_format = TextFormat::Create(L);

		HRESULT hr = gHR = core->dwrite_factory->CreateTextFormat(
			wide_font_family_name.c_str(),
			font_collection ? font_collection->dwrite_font_collection.Get() : NULL,
			font_weight,
			font_style,
			font_stretch,
			font_size,
			wide_locale_name.c_str(),
			&text_format->dwrite_text_format);
		if (FAILED(hr))
		{
			return luaL_error(L, "[DirectWrite.CreateTextFormat] IDWriteFactory::CreateTextFormat failed");
		}

		return 1;
	}
	static int api_CreateTextLayout(lua_State* L)
	{
		auto const string = luaL_check_string_view(L, 1);
		auto* text_format = TextFormat::Cast(L, 2);
		auto const max_width = luaL_check_float(L, 3);
		auto const max_height = luaL_check_float(L, 4);

		std::wstring wide_string(utility::encoding::to_wide(string));

		Factory* core = Factory::Get(L);
		TextLayout* text_layout = TextLayout::Create(L);

		HRESULT hr = gHR = core->dwrite_factory->CreateTextLayout(
			wide_string.data(),
			(UINT32)wide_string.size(),
			text_format->dwrite_text_format.Get(),
			max_width,
			max_height,
			&text_layout->dwrite_text_layout);
		if (FAILED(hr))
		{
			return luaL_error(L, "[DirectWrite.CreateTextLayout] IDWriteFactory::CreateTextLayout failed");
		}

		return 1;
	}

	static int api_CreateTextureFromTextLayout(lua_State* L)
	{
		HRESULT hr = S_OK;

		Factory* core = Factory::Get(L);
		auto* text_layout = TextLayout::Cast(L, 1);
		auto const pool_type = luaL_check_string_view(L, 2);
		auto const texture_name = luaL_check_string_view(L, 3);
		auto const outline_width = luaL_optional_float(L, 4, 0.0f);

		// bitmap

		auto const layout_width = text_layout->dwrite_text_layout->GetMaxWidth();
		auto const texture_width = std::ceil(layout_width);
		auto const texture_height = std::ceil(text_layout->dwrite_text_layout->GetMaxHeight());

		hr = gHR = text_layout->dwrite_text_layout->SetMaxWidth(layout_width - 2.0f * outline_width);
		if (FAILED(hr))
			return luaL_error(L, "update layout failed");
		ScopeFunction rec([&]() -> void {
			text_layout->dwrite_text_layout->SetMaxWidth(layout_width);
		});

		Microsoft::WRL::ComPtr<IWICBitmap> wic_bitmap;
		hr = gHR = core->wic_factory->CreateBitmap(
			(UINT)texture_width,
			(UINT)texture_height,
			GUID_WICPixelFormat32bppPBGRA,
			WICBitmapCacheOnDemand,
			&wic_bitmap);
		if (FAILED(hr))
			return luaL_error(L, "create bitmap failed");

		// d2d1 rasterizer

		Microsoft::WRL::ComPtr<ID2D1RenderTarget> d2d1_rt;
		hr = gHR = core->d2d1_factory->CreateWicBitmapRenderTarget(
			wic_bitmap.Get(),
			D2D1::RenderTargetProperties(),
			&d2d1_rt);
		if (FAILED(hr))
			return luaL_error(L, "create rasterizer failed");

		Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> d2d1_pen;
		hr = gHR = d2d1_rt->CreateSolidColorBrush(D2D1::ColorF(1.0f, 1.0f, 1.0f), &d2d1_pen);
		if (FAILED(hr))
			return luaL_error(L, "create rasterizer color failed");

		// rasterize

		if (lua_gettop(L) >= 4)
		{
			Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> d2d1_pen2;
			hr = gHR = d2d1_rt->CreateSolidColorBrush(D2D1::ColorF(0.0f, 0.0f, 0.0f), &d2d1_pen2);
			if (FAILED(hr))
				return luaL_error(L, "create rasterizer color failed");

			DWriteTextRendererImplement renderer(
				core->d2d1_factory.Get(),
				d2d1_rt.Get(),
				d2d1_pen2.Get(),
				d2d1_pen.Get(),
				outline_width);

			d2d1_rt->BeginDraw();
			d2d1_rt->Clear(D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.0f));
			hr = gHR = text_layout->dwrite_text_layout->Draw(NULL, &renderer, outline_width, outline_width);
			if (FAILED(hr))
				return luaL_error(L, "render failed");
			hr = gHR = d2d1_rt->EndDraw();
			if (FAILED(hr))
				return luaL_error(L, "rasterize failed");
		}
		else
		{
			d2d1_rt->BeginDraw();
			d2d1_rt->Clear(D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.0f));
			d2d1_rt->DrawTextLayout(D2D1::Point2F(0.0f, 0.0f), text_layout->dwrite_text_layout.Get(), d2d1_pen.Get());
			if (FAILED(hr))
				return luaL_error(L, "render failed");
			hr = gHR = d2d1_rt->EndDraw();
			if (FAILED(hr))
				return luaL_error(L, "rasterize failed");
		}

		// lock result

		WICRect lock_rect = {
			.X = 0,
			.Y = 0,
			.Width = (INT)texture_width,
			.Height = (INT)texture_height,
		};
		Microsoft::WRL::ComPtr<IWICBitmapLock> wic_bitmap_lock;
		hr = gHR = wic_bitmap->Lock(&lock_rect, WICBitmapLockRead, &wic_bitmap_lock);
		if (FAILED(hr)) return luaL_error(L, "read rasterize result failed");
		UINT buffer_size = 0;
		WICInProcPointer buffer = NULL;
		hr = gHR = wic_bitmap_lock->GetDataPointer(&buffer_size, &buffer);
		if (FAILED(hr)) return luaL_error(L, "read rasterize result failed");
		UINT buffer_stride = 0;
		hr = gHR = wic_bitmap_lock->GetStride(&buffer_stride);
		if (FAILED(hr)) return luaL_error(L, "read rasterize result failed");

		// create texture

		LuaSTGPlus::ResourcePool* pool{};
		if (pool_type == "global")
			pool = LRES.GetResourcePool(LuaSTGPlus::ResourcePoolType::Global);
		else if (pool_type == "stage")
			pool = LRES.GetResourcePool(LuaSTGPlus::ResourcePoolType::Stage);
		else
			return luaL_error(L, "invalid resource pool type");

		if (!pool->CreateTexture(texture_name.data(), (int)texture_width, (int)texture_height))
			return luaL_error(L, "create texture failed");

		auto p_texres = pool->GetTexture(texture_name.data());
		auto* p_texture = p_texres->GetTexture();

		// upload data

		p_texture->setPremultipliedAlpha(true);
		if (!p_texture->uploadPixelData(
			Core::RectU(0, 0, (uint32_t)texture_width, (uint32_t)texture_height),
			buffer, buffer_stride))
			return luaL_error(L, "upload texture data failed");

		return 0;
	}
}

int luaopen_dwrite(lua_State* L)
{
	// register module

	luaL_Reg const lib[] = {
		{ "CreateFontCollection", &DirectWrite::api_CreateFontCollection },
		{ "CreateTextFormat", &DirectWrite::api_CreateTextFormat },
		{ "CreateTextLayout", &DirectWrite::api_CreateTextLayout },
		{ "CreateTextureFromTextLayout", &DirectWrite::api_CreateTextureFromTextLayout },
		{ NULL, NULL },
	};
	luaL_register(L, "DirectWrite", lib);
	DirectWrite::Factory::Register(L);
	DirectWrite::FontCollection::Register(L);
	DirectWrite::TextFormat::Register(L);
	DirectWrite::TextLayout::Register(L);

	// create core

	lua_pushlightuserdata(L, &DirectWrite::LUA_KEY);
	DirectWrite::Factory* core = DirectWrite::Factory::Create(L);
	lua_settable(L, LUA_REGISTRYINDEX);
	if (!core->InitComponents())
		return luaL_error(L, "DirectWrite initialization failed");

	return 1;
}