#include <Windows.h>
#include <winhttp.h>
#include <atlstr.h>
#include <atlfile.h>

#define OBS_VERSION_STRING      L"1.0.0.1"

BOOL HTTPGetFile(CString url, CString outputPath, CString extraHeaders, int *responseCode, TCHAR *sigOut, DWORD *sigOutLen)
{
	HINTERNET hSession = NULL;
	HINTERNET hConnect = NULL;
	HINTERNET hRequest = NULL;
	URL_COMPONENTS  urlComponents;
	BOOL secure = FALSE;
	BOOL ret = FALSE;

	CString hostName, path;

	const TCHAR *acceptTypes[] = {
		TEXT("*/*"),
		NULL
	};

	hostName.GetBuffer(256);
	path.GetBuffer(1024);

	ZeroMemory(&urlComponents, sizeof(urlComponents));

	urlComponents.dwStructSize = sizeof(urlComponents);

	urlComponents.lpszHostName = (LPWSTR)(LPCWSTR)hostName;
	urlComponents.dwHostNameLength = hostName.GetLength();

	urlComponents.lpszUrlPath = (LPWSTR)(LPCWSTR)path;
	urlComponents.dwUrlPathLength = path.GetLength();

	WinHttpCrackUrl(url, 0, 0, &urlComponents);

	if (urlComponents.nPort == 443)
		secure = TRUE;

	hSession = WinHttpOpen(OBS_VERSION_STRING, WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
	if (!hSession)
		goto failure;

	hConnect = WinHttpConnect(hSession, hostName, secure ? INTERNET_DEFAULT_HTTPS_PORT : INTERNET_DEFAULT_HTTP_PORT, 0);
	if (!hConnect)
		goto failure;

	hRequest = WinHttpOpenRequest(hConnect, TEXT("GET"), path, NULL, WINHTTP_NO_REFERER, acceptTypes, secure ? WINHTTP_FLAG_SECURE | WINHTTP_FLAG_REFRESH : WINHTTP_FLAG_REFRESH);
	if (!hRequest)
		goto failure;

	BOOL bResults = WinHttpSendRequest(hRequest, extraHeaders, extraHeaders ? -1 : 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0);

	// End the request.
	if (bResults)
		bResults = WinHttpReceiveResponse(hRequest, NULL);
	else
		goto failure;

	TCHAR statusCode[8];
	DWORD statusCodeLen;

	statusCodeLen = sizeof(statusCode);
	if (!WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE, WINHTTP_HEADER_NAME_BY_INDEX, &statusCode, &statusCodeLen, WINHTTP_NO_HEADER_INDEX))
		goto failure;

	*responseCode = wcstoul(statusCode, NULL, 10);

	if (sigOut)
	{
		if (!WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_CUSTOM, TEXT("X-Signature"), sigOut, sigOutLen, WINHTTP_NO_HEADER_INDEX))
		{
			if (GetLastError() == ERROR_WINHTTP_HEADER_NOT_FOUND)
				*sigOutLen = 0;
			else if (GetLastError() == ERROR_INSUFFICIENT_BUFFER)
				*sigOutLen = 0;
			else
				goto failure;
		}
		else
		{
			// We need character count, not byte count.
			*sigOutLen /= 2;
		}
	}

	if (bResults && *responseCode == 200)
	{
		BYTE buffer[16384];
		DWORD dwSize, dwOutSize;

		//XFile updateFile;

		CAtlFile updateFile;
		HRESULT hr = updateFile.Create(outputPath, GENERIC_READ | GENERIC_WRITE,
			FILE_SHARE_READ | FILE_SHARE_WRITE, CREATE_ALWAYS);

		if (!SUCCEEDED(hr))
			goto failure;
		

		do
		{
			// Check for available data.
			dwSize = 0;
			if (!WinHttpQueryDataAvailable(hRequest, &dwSize))
				goto failure;

			if (!WinHttpReadData(hRequest, (LPVOID)buffer, dwSize, &dwOutSize))
			{
				goto failure;
			}
			else
			{
				if (!dwOutSize)
					break;

				if (!updateFile.Write(buffer, dwOutSize))
					goto failure;
			}
		} while (dwSize > 0);

		updateFile.Close();
	}

	ret = TRUE;

failure:
	if (hSession)
		WinHttpCloseHandle(hSession);
	if (hConnect)
		WinHttpCloseHandle(hConnect);
	if (hRequest)
		WinHttpCloseHandle(hRequest);

	if (sigOutLen && !ret)
		*sigOutLen = 0;

	return ret;
}

CStringA HTTPGetString(CString url, CString extraHeaders, int *responseCode)
{
	HINTERNET hSession = NULL;
	HINTERNET hConnect = NULL;
	HINTERNET hRequest = NULL;
	URL_COMPONENTS  urlComponents;
	BOOL secure = FALSE;
	CStringA result = "";

	CString hostName, path;

	const TCHAR *acceptTypes[] = {
		TEXT("*/*"),
		NULL
	};

	hostName.GetBuffer(256);
	path.GetBuffer(1024);

	ZeroMemory(&urlComponents, sizeof(urlComponents));

	urlComponents.dwStructSize = sizeof(urlComponents);

	urlComponents.lpszHostName = (LPWSTR)(LPCWSTR)hostName;
	urlComponents.dwHostNameLength = hostName.GetLength();

	urlComponents.lpszUrlPath = (LPWSTR)(LPCWSTR)path;
	urlComponents.dwUrlPathLength = path.GetLength();

	WinHttpCrackUrl(url, 0, 0, &urlComponents);

	if (urlComponents.nPort == 443)
		secure = TRUE;

	hSession = WinHttpOpen(OBS_VERSION_STRING, WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
	if (!hSession)
		goto failure;

	hConnect = WinHttpConnect(hSession, hostName, secure ? INTERNET_DEFAULT_HTTPS_PORT : INTERNET_DEFAULT_HTTP_PORT, 0);
	if (!hConnect)
		goto failure;

	hRequest = WinHttpOpenRequest(hConnect, TEXT("GET"), path, NULL, WINHTTP_NO_REFERER, acceptTypes, secure ? WINHTTP_FLAG_SECURE | WINHTTP_FLAG_REFRESH : WINHTTP_FLAG_REFRESH);
	if (!hRequest)
		goto failure;

	BOOL bResults = WinHttpSendRequest(hRequest, extraHeaders, extraHeaders ? -1 : 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0);

	// End the request.
	if (bResults)
		bResults = WinHttpReceiveResponse(hRequest, NULL);
	else
		goto failure;

	TCHAR statusCode[8];
	DWORD statusCodeLen;

	statusCodeLen = sizeof(statusCode);
	if (!WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE, WINHTTP_HEADER_NAME_BY_INDEX, &statusCode, &statusCodeLen, WINHTTP_NO_HEADER_INDEX))
		goto failure;

	*responseCode = wcstoul(statusCode, NULL, 10);

	if (bResults && *responseCode == 200)
	{
		CHAR buffer[16384];
		DWORD dwSize, dwOutSize;

		do
		{
			// Check for available data.
			dwSize = 0;
			if (!WinHttpQueryDataAvailable(hRequest, &dwSize))
				goto failure;

			if (!WinHttpReadData(hRequest, (LPVOID)buffer, dwSize, &dwOutSize))
			{
				goto failure;
			}
			else
			{
				if (!dwOutSize)
					break;

				// Ensure the string is terminated.
				buffer[dwOutSize] = 0;

				CStringA b = CStringA((LPCSTR)buffer);
				result.Append(b);				
			}
		} while (dwSize > 0);
	}

failure:
	if (hSession)
		WinHttpCloseHandle(hSession);
	if (hConnect)
		WinHttpCloseHandle(hConnect);
	if (hRequest)
		WinHttpCloseHandle(hRequest);

	return result;
}


CString CreateHTTPURL(CString host, CString path, CString extra, bool secure)
{
	URL_COMPONENTS components = {
		sizeof URL_COMPONENTS,
		secure ? L"https" : L"http",
		secure ? 5 : 4,
		secure ? INTERNET_SCHEME_HTTPS : INTERNET_SCHEME_HTTP,
		(LPWSTR)(LPCWSTR)host, host.GetLength(),
		secure ? INTERNET_DEFAULT_HTTPS_PORT : INTERNET_DEFAULT_HTTP_PORT,
		nullptr, 0,
		nullptr, 0,
		(LPWSTR)(LPCWSTR)path, path.GetLength(),
		(LPWSTR)(LPCWSTR)extra, extra.GetLength()
	};

	CString url;
	url.GetBuffer(MAX_PATH);
	DWORD length = MAX_PATH;
	if (!WinHttpCreateUrl(&components, ICU_ESCAPE, (LPWSTR)(LPCWSTR)url, &length))
		return L"";

	url.GetBuffer(length);
	return url;
}