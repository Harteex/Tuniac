#include "stdafx.h"
#include "HttpService.h"
#include <Strsafe.h>
#include <tchar.h>

CHttpService::CHttpService() :
	m_hSession(NULL),
	m_bAllowInvalidCertificates(false)
{
	// Initialize WinINet session
	m_hSession = InternetOpen(
		TEXT("Tuniac/1.0"),
		INTERNET_OPEN_TYPE_PRECONFIG,
		NULL,
		NULL,
		0
	);

	// Requests run on the UI thread, so don't let an unreachable server hang it for long
	if (m_hSession != NULL)
	{
		DWORD dwConnectTimeout = 10000;
		DWORD dwReceiveTimeout = 30000;
		InternetSetOption(m_hSession, INTERNET_OPTION_CONNECT_TIMEOUT, &dwConnectTimeout, sizeof(DWORD));
		InternetSetOption(m_hSession, INTERNET_OPTION_SEND_TIMEOUT, &dwReceiveTimeout, sizeof(DWORD));
		InternetSetOption(m_hSession, INTERNET_OPTION_RECEIVE_TIMEOUT, &dwReceiveTimeout, sizeof(DWORD));
	}
}

void CHttpService::SetAllowInvalidCertificates(bool bAllow)
{
	m_bAllowInvalidCertificates = bAllow;
}

CHttpService::~CHttpService()
{
	// Clean up WinINet session
	if (m_hSession != NULL)
	{
		InternetCloseHandle(m_hSession);
		m_hSession = NULL;
	}
}

bool CHttpService::ParseUrl(LPTSTR szUrl, ParsedUrl& parsedUrl)
{
	if (szUrl == NULL || szUrl[0] == TEXT('\0'))
	{
		return false;
	}

	// Reset parsed URL
	parsedUrl.bValid = false;
	parsedUrl.port = INTERNET_DEFAULT_HTTP_PORT;
	parsedUrl.bUseSsl = false;
	parsedUrl.szHost[0] = TEXT('\0');
	parsedUrl.szPath[0] = TEXT('\0');

	// Check for http:// or https://
	if (StrCmpNI(szUrl, TEXT("http://"), 7) == 0)
	{
		parsedUrl.bUseSsl = false;
		parsedUrl.port = INTERNET_DEFAULT_HTTP_PORT;
		szUrl += 7;
	}
	else if (StrCmpNI(szUrl, TEXT("https://"), 8) == 0)
	{
		parsedUrl.bUseSsl = true;
		parsedUrl.port = INTERNET_DEFAULT_HTTPS_PORT;
		szUrl += 8;
	}
	else
	{
		// No protocol specified, assume http
		parsedUrl.bUseSsl = false;
		parsedUrl.port = INTERNET_DEFAULT_HTTP_PORT;
	}

	// Find the first '/' or '?' to separate host from path
	LPTSTR szPathStart = NULL;
	LPTSTR szPortStart = NULL;
	size_t hostLen = 0;

	for (size_t i = 0; szUrl[i] != TEXT('\0'); i++)
	{
		if (szUrl[i] == TEXT(':'))
		{
			szPortStart = &szUrl[i];
		}
		else if (szUrl[i] == TEXT('/') || szUrl[i] == TEXT('?'))
		{
			szPathStart = &szUrl[i];
			break;
		}
	}

	// Extract hostname
	if (szPathStart != NULL)
	{
		hostLen = szPathStart - szUrl;
	}
	else
	{
		hostLen = _tcslen(szUrl);
	}

	if (hostLen == 0 || hostLen >= 256)
	{
		return false;
	}

	// Copy hostname (check for port)
	if (szPortStart != NULL && szPortStart < szUrl + hostLen)
	{
		// Port specified in URL
		size_t hostnameLen = szPortStart - szUrl;
		if (hostnameLen > 0 && hostnameLen < 256)
		{
			StringCchCopyN(parsedUrl.szHost, 256, szUrl, hostnameLen);
			
			// Extract port number
			size_t portLen = (szPathStart != NULL) ? (szPathStart - szPortStart - 1) : (_tcslen(szPortStart) - 1);
			if (portLen > 0 && portLen < 16)
			{
				TCHAR szPort[16];
				StringCchCopyN(szPort, 16, szPortStart + 1, portLen);
				parsedUrl.port = (INTERNET_PORT)_tstol(szPort);
			}
		}
		else
		{
			return false;
		}
	}
	else
	{
		// No port specified, use default
		StringCchCopyN(parsedUrl.szHost, 256, szUrl, hostLen);
	}

	// Extract path
	if (szPathStart != NULL)
	{
		StringCchCopy(parsedUrl.szPath, 2048, szPathStart);
	}
	else
	{
		StringCchCopy(parsedUrl.szPath, 2048, TEXT("/"));
	}

	parsedUrl.bValid = true;
	return true;
}

HttpResponse CHttpService::PerformRequest(LPTSTR szMethod, ParsedUrl& parsedUrl, const char* pBody, DWORD cbBody, LPTSTR szHeaders)
{
	HttpResponse response;

	if (m_hSession == NULL)
	{
		response.errorCode = ERROR_INVALID_HANDLE;
		return response;
	}

	if (!parsedUrl.bValid)
	{
		response.errorCode = ERROR_INVALID_PARAMETER;
		return response;
	}

	HINTERNET hConnect = NULL;
	HINTERNET hRequest = NULL;

	// Connect to server
	hConnect = InternetConnect(
		m_hSession,
		parsedUrl.szHost,
		parsedUrl.port,
		NULL,
		NULL,
		INTERNET_SERVICE_HTTP,
		0,
		0
	);

	if (hConnect == NULL)
	{
		response.errorCode = GetLastError();
		return response;
	}

	// Determine flags
	DWORD dwFlags = INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE;
	if (parsedUrl.bUseSsl)
	{
		dwFlags |= INTERNET_FLAG_SECURE;
	}

	// Open HTTP request
	hRequest = HttpOpenRequest(
		hConnect,
		szMethod,
		parsedUrl.szPath,
		NULL,
		NULL,
		NULL,
		dwFlags,
		0
	);

	if (hRequest == NULL)
	{
		response.errorCode = GetLastError();
		InternetCloseHandle(hConnect);
		return response;
	}

	// Self-signed certificates need SECURITY_FLAG_IGNORE_UNKNOWN_CA, which can only be set on the
	// request handle, not passed to HttpOpenRequest
	if (parsedUrl.bUseSsl && m_bAllowInvalidCertificates)
	{
		DWORD dwSecFlags = 0;
		DWORD dwSecFlagsSize = sizeof(dwSecFlags);
		InternetQueryOption(hRequest, INTERNET_OPTION_SECURITY_FLAGS, &dwSecFlags, &dwSecFlagsSize);
		dwSecFlags |= SECURITY_FLAG_IGNORE_UNKNOWN_CA | SECURITY_FLAG_IGNORE_CERT_DATE_INVALID |
			SECURITY_FLAG_IGNORE_CERT_CN_INVALID | SECURITY_FLAG_IGNORE_REVOCATION;
		InternetSetOption(hRequest, INTERNET_OPTION_SECURITY_FLAGS, &dwSecFlags, sizeof(dwSecFlags));
	}

	// Add custom headers if provided
	if (szHeaders != NULL && szHeaders[0] != TEXT('\0'))
	{
		HttpAddRequestHeaders(
			hRequest,
			szHeaders,
			-1,
			HTTP_ADDREQ_FLAG_ADD | HTTP_ADDREQ_FLAG_REPLACE
		);
	}

	// Send request
	BOOL bSendResult = HttpSendRequest(
		hRequest,
		NULL,
		0,
		(LPVOID)pBody,
		(pBody != NULL) ? cbBody : 0
	);

	if (!bSendResult)
	{
		response.errorCode = GetLastError();
		InternetCloseHandle(hRequest);
		InternetCloseHandle(hConnect);
		return response;
	}

	// Get HTTP status code
	DWORD dwStatusCode = 0;
	DWORD dwStatusCodeSize = sizeof(dwStatusCode);
	HttpQueryInfo(
		hRequest,
		HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER,
		&dwStatusCode,
		&dwStatusCodeSize,
		NULL
	);

	response.statusCode = dwStatusCode;

	// Read response content
	const DWORD BUFFER_SIZE = 4096;
	char* pBuffer = NULL;
	DWORD dwTotalBytesRead = 0;
	DWORD dwBufferSize = 0;

	while (true)
	{
		char szReadBuffer[BUFFER_SIZE];
		DWORD dwBytesRead = 0;

		BOOL bRead = InternetReadFile(
			hRequest,
			szReadBuffer,
			BUFFER_SIZE,
			&dwBytesRead
		);

		if (!bRead)
		{
			DWORD dwError = GetLastError();
			if (dwError != ERROR_SUCCESS)
			{
				response.errorCode = dwError;
				if (pBuffer != NULL)
				{
					delete[] pBuffer;
				}
				InternetCloseHandle(hRequest);
				InternetCloseHandle(hConnect);
				return response;
			}
		}

		if (dwBytesRead == 0)
		{
			// End of data
			break;
		}

		// Reallocate buffer to accommodate new data
		char* pNewBuffer = new char[dwBufferSize + dwBytesRead + 1];
		if (pNewBuffer == NULL)
		{
			response.errorCode = ERROR_OUTOFMEMORY;
			if (pBuffer != NULL)
			{
				delete[] pBuffer;
			}
			InternetCloseHandle(hRequest);
			InternetCloseHandle(hConnect);
			return response;
		}

		// Copy existing data
		if (pBuffer != NULL)
		{
			memcpy(pNewBuffer, pBuffer, dwBufferSize);
			delete[] pBuffer;
		}

		// Append new data
		memcpy(pNewBuffer + dwBufferSize, szReadBuffer, dwBytesRead);
		dwBufferSize += dwBytesRead;
		pBuffer = pNewBuffer;
		dwTotalBytesRead += dwBytesRead;
	}

	// Null-terminate the buffer
	if (pBuffer != NULL)
	{
		pBuffer[dwTotalBytesRead] = '\0';
		response.pContent = pBuffer;
		response.contentLength = dwTotalBytesRead;
	}

	// Clean up
	InternetCloseHandle(hRequest);
	InternetCloseHandle(hConnect);

	// Success if status code is 2xx
	response.success = (response.statusCode >= 200 && response.statusCode < 300);
	if (!response.success && response.errorCode == 0)
	{
		// HTTP error but no WinINet error
		response.errorCode = response.statusCode;
	}

	return response;
}

HttpResponse CHttpService::Get(LPTSTR szUrl, LPTSTR szHeaders)
{
	HttpResponse response;

	ParsedUrl parsedUrl;
	if (!ParseUrl(szUrl, parsedUrl))
	{
		response.errorCode = ERROR_INVALID_PARAMETER;
		return response;
	}

	return PerformRequest(TEXT("GET"), parsedUrl, NULL, 0, szHeaders);
}

HttpResponse CHttpService::Post(LPTSTR szUrl, const char* pBody, DWORD cbBody, LPTSTR szHeaders)
{
	HttpResponse response;

	ParsedUrl parsedUrl;
	if (!ParseUrl(szUrl, parsedUrl))
	{
		response.errorCode = ERROR_INVALID_PARAMETER;
		return response;
	}

	return PerformRequest(TEXT("POST"), parsedUrl, pBody, cbBody, szHeaders);
}

