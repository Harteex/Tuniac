#include "stdafx.h"
#include "JellyfinService.h"
#include <WinSock2.h>
#include <WS2tcpip.h>

#define CLIENT_NAME TEXT("Tuniac")

/*

Size = 256 * sizeof(WCHAR);
	RegQueryValueEx(	hTuniacPrefKey,
						PLUGINFORMATSTRING,
						NULL,
						&Type,
						(LPBYTE)&m_szPluginFormatString,
						&Size);




RegSetValueEx(	hTuniacPrefKey,
					WINDOWFORMATSTRING,
					NULL,
					REG_SZ,
					(LPBYTE)&m_szWindowFormatString,
					(wcsnlen_s(m_szWindowFormatString, 256) + 1) * sizeof(TCHAR));

*/

CJellyfinService::CJellyfinService(void) :
	m_szHost(TEXT("")),
	m_szDevice(TEXT("")),
	m_szVersion(TEXT("")),
	m_szToken(TEXT("a")),
	m_szHostname(TEXT("")),
	m_bUseSsl(false)
{
	DWORD lpRegType = REG_SZ;
	DWORD Size;
	
	//TCHAR prefValue[256];
	//TCHAR prefValue2[256];

	/*if (tuniacApp.m_Preferences.PluginGetValue(TEXT("plugins\\Jellyfin"), L"Host", &lpRegType, (LPBYTE)&m_szHost, &Size))
		StringCchCopy(m_szHost, 256, prefValue);
	if (tuniacApp.m_Preferences.PluginGetValue(TEXT("plugins\\Jellyfin"), L"Token", &lpRegType, (LPBYTE)&m_szToken, &Size))
		StringCchCopy(m_szToken, 256, prefValue2);*/

	Size = 256;
	tuniacApp.m_Preferences.PluginGetValue(TEXT("plugins\\Jellyfin"), L"Host", &lpRegType, (LPBYTE)&m_szHost, &Size);
	Size = 128;
	tuniacApp.m_Preferences.PluginGetValue(TEXT("plugins\\Jellyfin"), L"Token", &lpRegType, (LPBYTE)&m_szToken, &Size);

	ExtractHostnameAndSsl(m_szHost, m_szHostname, &m_bUseSsl);

	StringCchCopy(m_szDevice, 128, TEXT("Windows"));

	TCHAR  infoBuf[512];
	DWORD  bufCharCount = 512;
	if (GetComputerName(infoBuf, &bufCharCount))
		StringCchCopy(m_szDevice, 128, infoBuf);

	StringCchCopy(m_szVersion, 128, TEXT("1.0"));
}

CJellyfinService::~CJellyfinService(void)
{
}

bool CJellyfinService::CanHandle(LPTSTR szSource)
{
	// TODO Verify length?
	return StrCmpNI(szSource, TEXT("jellyfin://"), 11) == 0;
}

bool CJellyfinService::TranslateSource(LPTSTR szDest, size_t size, LPTSTR szSource)
{
	TCHAR itemId[33];
	StringCchCopy(itemId, 33, szSource + 11);

	HRESULT result = StringCchPrintf(
		szDest,
		size,
		TEXT("%s/Audio/%s/stream?static=true\r\nX-Emby-Authorization: MediaBrowser Client=\"%s\", Device=\"%s\", DeviceId=\"%s\", Version=\"%s\", Token=\"%s\"\r\n"),
		m_szHost,
		itemId,
		CLIENT_NAME,
		m_szDevice,
		m_szDevice,
		m_szVersion,
		m_szToken
	);

	return SUCCEEDED(result);
}

bool CJellyfinService::ExtractHostnameAndSsl(LPTSTR szHost, LPTSTR szOutHostname, bool* bOutUseSsl)
{
	if (StrCmpNI(szHost, TEXT("http://"), 7) == 0)
	{
		*bOutUseSsl = false;
		StringCchCopy(szOutHostname, 128, szHost + 7);
	}
	else if (StrCmpNI(szHost, TEXT("https://"), 8) == 0)
	{
		*bOutUseSsl = true;
		StringCchCopy(szOutHostname, 128, szHost + 8);
	}
	else
	{
		// Unknown protocol
		return false;
	}

	return true;
}

bool CJellyfinService::Login(LPTSTR szHost, LPTSTR szUsername, LPTSTR szPassword)
{






	/*WCHAR szToken[256] = L"c93ce065114849d4b20fc8fbeafc6d25";
	tuniacApp.m_Preferences.PluginSetValue(L"plugins\\Jellyfin", TEXT("Token"), REG_SZ, (LPBYTE)&szToken, (wcsnlen_s(szToken, 255) + 1) * sizeof(WCHAR));
	
	tuniacApp.m_Preferences.PluginSetValue(L"plugins\\Jellyfin", TEXT("Host"), REG_SZ, (LPBYTE)szHost, (wcsnlen_s(szHost, 255) + 1) * sizeof(WCHAR));*/


	return true;
}

#define DEFAULT_BUFLEN 512
#define DEFAULT_PORT "80"

void CJellyfinService::DoHttpRequest()
{
	// FIXME för ssl, testa detta! https://docs.microsoft.com/sv-se/windows/win32/winsock/using-secure-socket-extensions
	// Intressant: https://stackoverflow.com/questions/1011339/how-do-you-make-a-http-request-with-c
	
	// Eller köra på https://docs.microsoft.com/en-us/windows/win32/wininet/about-wininet
	// https://gist.github.com/AhnMo/5cf37bbd9a6fa2567f99ac0528eaa185


	SOCKET ConnectSocket = INVALID_SOCKET;
	struct addrinfo* result = NULL,
		* ptr = NULL,
		hints;
	const char* sendbuf = "this is a test";
	char recvbuf[DEFAULT_BUFLEN];
	int iResult;
	int recvbuflen = DEFAULT_BUFLEN;




    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    // Resolve the server address and port
    iResult = getaddrinfo("harteex.com", DEFAULT_PORT, &hints, &result);
    if (iResult != 0) {
        printf("getaddrinfo failed with error: %d\n", iResult);
        return;
    }

    // Attempt to connect to an address until one succeeds
    for (ptr = result; ptr != NULL; ptr = ptr->ai_next) {

        // Create a SOCKET for connecting to server
        ConnectSocket = socket(ptr->ai_family, ptr->ai_socktype,
            ptr->ai_protocol);
        if (ConnectSocket == INVALID_SOCKET) {
            printf("socket failed with error: %ld\n", WSAGetLastError());
            return;
        }

        // Connect to server.
        iResult = connect(ConnectSocket, ptr->ai_addr, (int)ptr->ai_addrlen);
        if (iResult == SOCKET_ERROR) {
            closesocket(ConnectSocket);
            ConnectSocket = INVALID_SOCKET;
            continue;
        }
        break;
    }

    freeaddrinfo(result);

    if (ConnectSocket == INVALID_SOCKET) {
        printf("Unable to connect to server!\n");
		return;
    }

    // Send an initial buffer
    iResult = send(ConnectSocket, sendbuf, (int)strlen(sendbuf), 0);
    if (iResult == SOCKET_ERROR) {
        printf("send failed with error: %d\n", WSAGetLastError());
        closesocket(ConnectSocket);
        return;
    }

    printf("Bytes Sent: %ld\n", iResult);

    // shutdown the connection since no more data will be sent
    iResult = shutdown(ConnectSocket, SD_SEND);
    if (iResult == SOCKET_ERROR) {
        printf("shutdown failed with error: %d\n", WSAGetLastError());
        closesocket(ConnectSocket);
        return;
    }

    // Receive until the peer closes the connection
    do {

        iResult = recv(ConnectSocket, recvbuf, recvbuflen, 0);
        if (iResult > 0)
            printf("Bytes received: %d\n", iResult);
        else if (iResult == 0)
            printf("Connection closed\n");
        else
            printf("recv failed with error: %d\n", WSAGetLastError());

    } while (iResult > 0);

    // cleanup
    closesocket(ConnectSocket);

}

void CJellyfinService::DoHttpRequest2()
{
	HINTERNET hSession = InternetOpen(L"Mozilla/5.0", INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
	HINTERNET hConnect = InternetConnect(hSession, L"test.com", INTERNET_DEFAULT_HTTPS_PORT, L"", L"", INTERNET_SERVICE_HTTP, 0, 0);
	HINTERNET hHttpFile = HttpOpenRequest(hConnect, L"GET", L"/", NULL, NULL, NULL, INTERNET_FLAG_SECURE | INTERNET_FLAG_RELOAD, 0);

	/*WCHAR szHeader[BUFSIZ] = {0};

	lstrcpy(szHeader, L"User-Agent: Mozilla/5.0\r\n");
	lstrcat(szHeader, L"\r\n");

	HttpAddRequestHeaders(hHttpFile, szHeader, -1, HTTP_ADDREQ_FLAG_ADD);*/

	// FIXME while here is likely a bad idea.... should break no? or number of tries?
	/*while (!HttpSendRequest(hHttpFile, NULL, 0, 0, 0)) {
		printf("HttpSendRequest error : (%lu)\n", GetLastError());

		InternetErrorDlg(
			GetDesktopWindow(),
			hHttpFile,
			ERROR_INTERNET_CLIENT_AUTH_CERT_NEEDED,
			FLAGS_ERROR_UI_FILTER_FOR_ERRORS |
			FLAGS_ERROR_UI_FLAGS_GENERATE_DATA |
			FLAGS_ERROR_UI_FLAGS_CHANGE_OPTIONS,
			NULL);
	}*/

	if (!HttpSendRequest(hHttpFile, NULL, 0, 0, 0)) {
		InternetCloseHandle(hHttpFile);
		InternetCloseHandle(hConnect);
		InternetCloseHandle(hSession);
		return;
	}

	DWORD dwFileSize;
	dwFileSize = BUFSIZ;

	char* buffer;
	buffer = new char[dwFileSize + 1];

	while (true) {
		DWORD dwBytesRead;
		BOOL bRead;

		bRead = InternetReadFile(
			hHttpFile,
			buffer,
			dwFileSize + 1,
			&dwBytesRead);

		if (dwBytesRead == 0)
			break;

		if (!bRead) {
			printf("InternetReadFile error : <%lu>\n", GetLastError());
		}
		else {
			buffer[dwBytesRead] = 0;
			printf("Retrieved %lu data bytes: %s\n", dwBytesRead, buffer);
		}
	}

	InternetCloseHandle(hHttpFile);
	InternetCloseHandle(hConnect);
	InternetCloseHandle(hSession);

	return;
}
