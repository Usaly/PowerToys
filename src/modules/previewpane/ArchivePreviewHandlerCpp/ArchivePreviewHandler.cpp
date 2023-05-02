#include "pch.h"
#include "ArchivePreviewHandler.h"
#include "Generated Files/resource.h"

#include <shellapi.h>
#include <Shlwapi.h>
#include <string>

#include <common/interop/shared_constants.h>
#include <common/logger/logger.h>
#include <common/SettingsAPI/settings_helpers.h>
#include <common/utils/process_path.h>

extern HINSTANCE g_hInst;
extern long g_cDllRef;

ArchivePreviewHandler::ArchivePreviewHandler() :
    m_cRef(1), m_hwndParent(NULL), m_rcParent(), m_punkSite(NULL), m_process(NULL)
{
    m_resizeEvent = CreateEvent(nullptr, false, false, CommonSharedConstants::ARCHIVE_PREVIEW_RESIZE_EVENT);

    std::filesystem::path logFilePath(PTSettingsHelper::get_local_low_folder_location());
    logFilePath.append(LogSettings::mdPrevLogPath);
    Logger::init(LogSettings::mdPrevLoggerName, logFilePath.wstring(), PTSettingsHelper::get_log_settings_file_location());

    InterlockedIncrement(&g_cDllRef);
}

ArchivePreviewHandler::~ArchivePreviewHandler()
{
    InterlockedDecrement(&g_cDllRef);
}

#pragma region IUnknown

IFACEMETHODIMP ArchivePreviewHandler::QueryInterface(REFIID riid, void** ppv)
{
    static const QITAB qit[] = {
        QITABENT(ArchivePreviewHandler, IPreviewHandler),
        QITABENT(ArchivePreviewHandler, IInitializeWithFile),
        QITABENT(ArchivePreviewHandler, IPreviewHandlerVisuals),
        QITABENT(ArchivePreviewHandler, IOleWindow),
        QITABENT(ArchivePreviewHandler, IObjectWithSite),
        { 0 },
    };
    return QISearch(this, qit, riid, ppv);
}

IFACEMETHODIMP_(ULONG)
ArchivePreviewHandler::AddRef()
{
    return InterlockedIncrement(&m_cRef);
}

IFACEMETHODIMP_(ULONG)
ArchivePreviewHandler::Release()
{
    ULONG cRef = InterlockedDecrement(&m_cRef);
    if (0 == cRef)
    {
        delete this;
    }
    return cRef;
}

#pragma endregion

#pragma region IInitializationWithFile

IFACEMETHODIMP ArchivePreviewHandler::Initialize(LPCWSTR pszFilePath, DWORD grfMode)
{
    m_filePath = pszFilePath;
    return S_OK;
}

#pragma endregion

#pragma region IPreviewHandler

IFACEMETHODIMP ArchivePreviewHandler::SetWindow(HWND hwnd, const RECT* prc)
{
    if (hwnd && prc)
    {
        m_hwndParent = hwnd;
        m_rcParent = *prc;
    }
    return S_OK;
}

IFACEMETHODIMP ArchivePreviewHandler::SetFocus()
{
    return S_OK;
}

IFACEMETHODIMP ArchivePreviewHandler::QueryFocus(HWND* phwnd)
{
    HRESULT hr = E_INVALIDARG;
    if (phwnd)
    {
        *phwnd = ::GetFocus();
        if (*phwnd)
        {
            hr = S_OK;
        }
        else
        {
            hr = HRESULT_FROM_WIN32(GetLastError());
        }
    }
    return hr;
}

IFACEMETHODIMP ArchivePreviewHandler::TranslateAccelerator(MSG* pmsg)
{
    HRESULT hr = S_FALSE;
    IPreviewHandlerFrame* pFrame = NULL;
    if (m_punkSite && SUCCEEDED(m_punkSite->QueryInterface(&pFrame)))
    {
        hr = pFrame->TranslateAccelerator(pmsg);

        pFrame->Release();
    }
    return hr;
}

IFACEMETHODIMP ArchivePreviewHandler::SetRect(const RECT* prc)
{
    HRESULT hr = E_INVALIDARG;
    if (prc != NULL)
    {
        if (!m_resizeEvent)
        {
            Logger::error(L"Failed to create resize event for ArchivePreviewHandler");
        }
        else
        {
            if (m_rcParent.right != prc->right || m_rcParent.left != prc->left || m_rcParent.top != prc->top || m_rcParent.bottom != prc->bottom)
            {
                if (!SetEvent(m_resizeEvent))
                {
                    Logger::error(L"Failed to signal resize event for ArchivePreviewHandler");
                }
            }
        }
        m_rcParent = *prc;
        hr = S_OK;
    }
    return hr;
}

IFACEMETHODIMP ArchivePreviewHandler::DoPreview()
{
    try
    {
        Logger::info(L"Starting ArchivePreviewHandler.exe");

        STARTUPINFO info = { sizeof(info) };
        std::wstring cmdLine{ L"\"" + m_filePath + L"\"" };
        cmdLine += L" ";
        std::wostringstream ss;
        ss << std::hex << m_hwndParent;

        cmdLine += ss.str();
        std::wstring appPath = get_module_folderpath(g_hInst) + L"\\PowerToys.ArchivePreviewHandler.exe";

        SHELLEXECUTEINFO sei{ sizeof(sei) };
        sei.fMask = { SEE_MASK_NOCLOSEPROCESS | SEE_MASK_FLAG_NO_UI };
        sei.lpFile = appPath.c_str();
        sei.lpParameters = cmdLine.c_str();
        sei.nShow = SW_SHOWDEFAULT;
        ShellExecuteEx(&sei);
        m_process = sei.hProcess;
    }
    catch (std::exception& e)
    {
        std::wstring errorMessage = std::wstring{ winrt::to_hstring(e.what()) };
        Logger::error(L"Failed to start ArchivePreviewHandler.exe. Error: {}", errorMessage);
    }

    return S_OK;
}

IFACEMETHODIMP ArchivePreviewHandler::Unload()

{
    Logger::info(L"Unload and terminate .exe");

    TerminateProcess(m_process, 0);
    return S_OK;
}

#pragma endregion

#pragma region IPreviewHandlerVisuals

IFACEMETHODIMP ArchivePreviewHandler::SetBackgroundColor(COLORREF color)
{
    return S_OK;
}

IFACEMETHODIMP ArchivePreviewHandler::SetFont(const LOGFONTW* plf)
{
    return S_OK;
}

IFACEMETHODIMP ArchivePreviewHandler::SetTextColor(COLORREF color)
{
    return S_OK;
}

#pragma endregion

#pragma region IOleWindow

IFACEMETHODIMP ArchivePreviewHandler::GetWindow(HWND* phwnd)
{
    HRESULT hr = E_INVALIDARG;
    if (phwnd)
    {
        *phwnd = m_hwndParent;
        hr = S_OK;
    }
    return hr;
}

IFACEMETHODIMP ArchivePreviewHandler::ContextSensitiveHelp(BOOL fEnterMode)
{
    return E_NOTIMPL;
}

#pragma endregion

#pragma region IObjectWithSite

IFACEMETHODIMP ArchivePreviewHandler::SetSite(IUnknown* punkSite)
{
    if (m_punkSite)
    {
        m_punkSite->Release();
        m_punkSite = NULL;
    }
    return punkSite ? punkSite->QueryInterface(&m_punkSite) : S_OK;
}

IFACEMETHODIMP ArchivePreviewHandler::GetSite(REFIID riid, void** ppv)
{
    *ppv = NULL;
    return m_punkSite ? m_punkSite->QueryInterface(riid, ppv) : E_FAIL;
}

#pragma endregion

#pragma region Helper Functions

#pragma endregion