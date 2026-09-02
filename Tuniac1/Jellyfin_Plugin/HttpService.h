#pragma once

#include <WinInet.h>

// Structure to hold HTTP response data
// Caller is responsible for freeing pContent if it's not NULL
struct HttpResponse
{
	DWORD statusCode;		// HTTP status code (200, 404, 500, etc.)
	char* pContent;			// Response content buffer (caller manages memory)
	DWORD contentLength;	// Length of content in bytes
	DWORD errorCode;		// WinINet error code (0 if no error)
	bool success;			// True if request succeeded

	HttpResponse() :
		statusCode(0),
		pContent(NULL),
		contentLength(0),
		errorCode(0),
		success(false)
	{
	}
};

// Structure to hold parsed URL components
struct ParsedUrl
{
	TCHAR szHost[256];
	TCHAR szPath[2048];
	INTERNET_PORT port;
	bool bUseSsl;
	bool bValid;

	ParsedUrl() :
		port(INTERNET_DEFAULT_HTTP_PORT),
		bUseSsl(false),
		bValid(false)
	{
		szHost[0] = TEXT('\0');
		szPath[0] = TEXT('\0');
	}
};

class CHttpService
{
public:
	CHttpService();
	~CHttpService();

	// Perform HTTP GET request
	// szUrl: Full URL (e.g., "https://example.com/api/endpoint")
	// szHeaders: Optional additional headers (NULL if none), format: "Header1: Value1\r\nHeader2: Value2\r\n"
	// Returns: HttpResponse structure with response data
	HttpResponse Get(LPTSTR szUrl, LPTSTR szHeaders = NULL);

	// Perform HTTP POST request
	// szUrl: Full URL (e.g., "https://example.com/api/endpoint")
	// szData: POST data to send (NULL if none)
	// szHeaders: Optional additional headers (NULL if none), format: "Header1: Value1\r\nHeader2: Value2\r\n"
	// Returns: HttpResponse structure with response data
	HttpResponse Post(LPTSTR szUrl, LPTSTR szData = NULL, LPTSTR szHeaders = NULL);

private:
	HINTERNET m_hSession;	// WinINet session handle

	// Parse URL into components
	bool ParseUrl(LPTSTR szUrl, ParsedUrl& parsedUrl);

	// Internal method to perform HTTP request
	HttpResponse PerformRequest(LPTSTR szMethod, ParsedUrl& parsedUrl, LPTSTR szData, LPTSTR szHeaders);
};


