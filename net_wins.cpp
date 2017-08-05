#include "quakedef.h"
#include "winquake.h"


extern cvar_t hostname;

#define MAXHOSTNAMELEN		256

static int net_acceptsocket = -1; // socket for fielding new connections
static int net_controlsocket;
static int net_broadcastsocket = 0;
static struct qsockaddr broadcastaddr;

static unsigned long myAddr;

bool winsock_lib_initialized;

#include "net_wins.h"

unsigned int winsock_initialized = 0;
WSADATA winsockdata;




//=============================================================================

static double blocktime;

BOOL PASCAL FAR BlockingHook()
{
	MSG msg;

	if (Sys_FloatTime() - blocktime > 2.0)
	{
		WSACancelBlockingCall();
		return FALSE;
	}

	/* get the next message, if any */
	auto ret = static_cast<BOOL>(PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE));

	/* if we got one, process it */
	if (ret)
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	/* TRUE if we got a message */
	return ret;
}


void WINS_GetLocalAddress()
{
	char buff[MAXHOSTNAMELEN];

	if (myAddr != INADDR_ANY)
		return;

	if (gethostname(buff, MAXHOSTNAMELEN) == SOCKET_ERROR)
		return;

	blocktime = Sys_FloatTime();
	WSASetBlockingHook(BlockingHook);
	auto local = gethostbyname(buff);
	WSAUnhookBlockingHook();
	if (local == nullptr)
		return;

	myAddr = *reinterpret_cast<int *>(local->h_addr_list[0]);

	auto addr = ntohl(myAddr);
	sprintf(my_tcpip_address, "%d.%d.%d.%d", addr >> 24 & 0xff, addr >> 16 & 0xff, addr >> 8 & 0xff, addr & 0xff);
}


int WINS_Init()
{
	int i;
	char buff[MAXHOSTNAMELEN];
	char* p;

	// initialize the Winsock function vectors (we do this instead of statically linking
	// so we can run on Win 3.1, where there isn't necessarily Winsock)
	auto hInst = LoadLibrary("wsock32.dll");

	if (hInst == nullptr)
	{
		Con_SafePrintf("Failed to load winsock.dll\n");
		winsock_lib_initialized = false;
		return -1;
	}

	winsock_lib_initialized = true;


	if (COM_CheckParm("-noudp"))
		return -1;

	if (winsock_initialized == 0)
	{
		auto r = WSAStartup(MAKEWORD(1, 1), &winsockdata);

		if (r)
		{
			Con_SafePrintf("Winsock initialization failed.\n");
			return -1;
		}
	}
	winsock_initialized++;

	// determine my name
	if (gethostname(buff, MAXHOSTNAMELEN) == SOCKET_ERROR)
	{
		Con_DPrintf("Winsock TCP/IP Initialization failed.\n");
		if (--winsock_initialized == 0)
			WSACleanup();
		return -1;
	}

	// if the quake hostname isn't set, set it to the machine name
	if (Q_strcmp(hostname.string, "UNNAMED") == 0)
	{
		// see if it's a text IP address (well, close enough)
		for (p = buff; *p; p++)
			if ((*p < '0' || *p > '9') && *p != '.')
				break;

		// if it is a real name, strip off the domain; we only want the host
		if (*p)
		{
			for (i = 0; i < 15; i++)
				if (buff[i] == '.')
					break;
			buff[i] = 0;
		}
		Cvar_Set("hostname", buff);
	}

	i = COM_CheckParm("-ip");
	if (i)
	{
		if (i < com_argc - 1)
		{
			myAddr = inet_addr(com_argv[i + 1]);
			if (myAddr == INADDR_NONE)
				Sys_Error("%s is not a valid IP address", com_argv[i + 1]);
			strcpy(my_tcpip_address, com_argv[i + 1]);
		}
		else
		{
			Sys_Error("NET_Init: you must specify an IP address after -ip");
		}
	}
	else
	{
		myAddr = INADDR_ANY;
		strcpy(my_tcpip_address, "INADDR_ANY");
	}

	if ((net_controlsocket = WINS_OpenSocket(0)) == -1)
	{
		Con_Printf("WINS_Init: Unable to open control socket\n");
		if (--winsock_initialized == 0)
			WSACleanup();
		return -1;
	}

	reinterpret_cast<sockaddr_in *>(&broadcastaddr)->sin_family = AF_INET;
	reinterpret_cast<sockaddr_in *>(&broadcastaddr)->sin_addr.s_addr = INADDR_BROADCAST;
	reinterpret_cast<sockaddr_in *>(&broadcastaddr)->sin_port = htons(static_cast<unsigned short>(net_hostport));

	Con_Printf("Winsock TCP/IP Initialized\n");
	tcpipAvailable = true;

	return net_controlsocket;
}

//=============================================================================

void WINS_Shutdown()
{
	WINS_Listen(false);
	WINS_CloseSocket(net_controlsocket);
	if (--winsock_initialized == 0)
		WSACleanup();
}

//=============================================================================

void WINS_Listen(bool state)
{
	// enable listening
	if (state)
	{
		if (net_acceptsocket != -1)
			return;
		WINS_GetLocalAddress();
		if ((net_acceptsocket = WINS_OpenSocket(net_hostport)) == -1)
			Sys_Error("WINS_Listen: Unable to open accept socket\n");
		return;
	}

	// disable listening
	if (net_acceptsocket == -1)
		return;
	WINS_CloseSocket(net_acceptsocket);
	net_acceptsocket = -1;
}

//=============================================================================

int WINS_OpenSocket(int port)
{
	int newsocket;
	sockaddr_in address;
	u_long _true = 1;

	if ((newsocket = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
		return -1;

	if (ioctlsocket(newsocket, FIONBIO, &_true) == -1)
		goto ErrorReturn;

	address.sin_family = AF_INET;
	address.sin_addr.s_addr = myAddr;
	address.sin_port = htons(static_cast<unsigned short>(port));
	if (bind(newsocket, reinterpret_cast<const sockaddr *>(&address), sizeof address) == 0)
		return newsocket;

	Sys_Error("Unable to bind to %s", WINS_AddrToString(reinterpret_cast<qsockaddr *>(&address)));
ErrorReturn:
	closesocket(newsocket);
	return -1;
}

//=============================================================================

int WINS_CloseSocket(int socket)
{
	if (socket == net_broadcastsocket)
		net_broadcastsocket = 0;
	return closesocket(socket);
}


//=============================================================================
/*
============
PartialIPAddress

this lets you type only as much of the net address as required, using
the local network components to fill in the rest
============
*/
static int PartialIPAddress(char* in, struct qsockaddr* hostaddr)
{
	char buff[256];
	int port;

	buff[0] = '.';
	auto b = buff;
	strcpy(buff + 1, in);
	if (buff[1] == '.')
		b++;

	auto addr = 0;
	auto mask = -1;
	while (*b == '.')
	{
		b++;
		auto num = 0;
		auto run = 0;
		while (!(*b < '0' || *b > '9'))
		{
			num = num * 10 + *b++ - '0';
			if (++run > 3)
				return -1;
		}
		if ((*b < '0' || *b > '9') && *b != '.' && *b != ':' && *b != 0)
			return -1;
		if (num < 0 || num > 255)
			return -1;
		mask <<= 8;
		addr = (addr << 8) + num;
	}

	if (*b++ == ':')
		port = Q_atoi(b);
	else
		port = net_hostport;

	hostaddr->sa_family = AF_INET;
	reinterpret_cast<sockaddr_in *>(hostaddr)->sin_port = htons(static_cast<short>(port));
	reinterpret_cast<sockaddr_in *>(hostaddr)->sin_addr.s_addr = myAddr & htonl(mask) | htonl(addr);

	return 0;
}

//=============================================================================

int WINS_Connect(int socket, struct qsockaddr* addr)
{
	return 0;
}

//=============================================================================

int WINS_CheckNewConnections()
{
	char buf[4096];

	if (net_acceptsocket == -1)
		return -1;

	if (recvfrom(net_acceptsocket, buf, sizeof buf, MSG_PEEK, nullptr, nullptr) > 0)
	{
		return net_acceptsocket;
	}
	return -1;
}

//=============================================================================

int WINS_Read(int socket, byte* buf, int len, struct qsockaddr* addr)
{
	int addrlen = sizeof (struct qsockaddr);

	auto ret = recvfrom(socket, reinterpret_cast<char*>(buf), len, 0, reinterpret_cast<sockaddr *>(addr), &addrlen);
	if (ret == -1)
	{
		auto err = WSAGetLastError();

		if (err == WSAEWOULDBLOCK || err == WSAECONNREFUSED)
			return 0;
	}
	return ret;
}

//=============================================================================

int WINS_MakeSocketBroadcastCapable(int socket)
{
	int i;
	// make this socket broadcast capable
	if (setsockopt(socket, SOL_SOCKET, SO_BROADCAST, reinterpret_cast<char *>(&i), sizeof i) < 0)
		return -1;
	net_broadcastsocket = socket;

	return 0;
}

//=============================================================================

int WINS_Broadcast(int socket, byte* buf, int len)
{
	if (socket != net_broadcastsocket)
	{
		if (net_broadcastsocket != 0)
			Sys_Error("Attempted to use multiple broadcasts sockets\n");
		WINS_GetLocalAddress();
		auto ret = WINS_MakeSocketBroadcastCapable(socket);
		if (ret == -1)
		{
			Con_Printf("Unable to make socket broadcast capable\n");
			return ret;
		}
	}

	return WINS_Write(socket, buf, len, &broadcastaddr);
}

//=============================================================================

int WINS_Write(int socket, byte* buf, int len, struct qsockaddr* addr)
{
	auto ret = sendto(socket, reinterpret_cast<const char*>(buf), len, 0, reinterpret_cast<sockaddr *>(addr), sizeof(qsockaddr));
	if (ret == -1)
		if (WSAGetLastError() == WSAEWOULDBLOCK)
			return 0;

	return ret;
}

//=============================================================================

char* WINS_AddrToString(struct qsockaddr* addr)
{
	static char buffer[22];

	int haddr = ntohl(reinterpret_cast<sockaddr_in *>(addr)->sin_addr.s_addr);
	sprintf(buffer, "%d.%d.%d.%d:%d", haddr >> 24 & 0xff, haddr >> 16 & 0xff, haddr >> 8 & 0xff, haddr & 0xff, ntohs(reinterpret_cast<sockaddr_in *>(addr)->sin_port));
	return buffer;
}

//=============================================================================

int WINS_StringToAddr(char* string, struct qsockaddr* addr)
{
	int ha1, ha2, ha3, ha4, hp;

	sscanf(string, "%d.%d.%d.%d:%d", &ha1, &ha2, &ha3, &ha4, &hp);
	auto ipaddr = ha1 << 24 | ha2 << 16 | ha3 << 8 | ha4;

	addr->sa_family = AF_INET;
	reinterpret_cast<sockaddr_in *>(addr)->sin_addr.s_addr = htonl(ipaddr);
	reinterpret_cast<sockaddr_in *>(addr)->sin_port = htons(static_cast<unsigned short>(hp));
	return 0;
}

//=============================================================================

int WINS_GetSocketAddr(int socket, qsockaddr* addr)
{
	int addrlen = sizeof(qsockaddr);
	Q_memset(addr, 0, sizeof(qsockaddr));
	getsockname(socket, reinterpret_cast<sockaddr *>(addr), &addrlen);
	unsigned int a = reinterpret_cast<sockaddr_in *>(addr)->sin_addr.s_addr;
	if (a == 0 || a == inet_addr("127.0.0.1"))
		reinterpret_cast<sockaddr_in *>(addr)->sin_addr.s_addr = myAddr;

	return 0;
}

//=============================================================================

int WINS_GetNameFromAddr(struct qsockaddr* addr, char* name)
{
	auto hostentry = gethostbyaddr(reinterpret_cast<char *>(&reinterpret_cast<sockaddr_in *>(addr)->sin_addr), sizeof(struct in_addr), AF_INET);
	if (hostentry)
	{
		Q_strncpy(name, static_cast<char *>(hostentry->h_name), NET_NAMELEN - 1);
		return 0;
	}

	Q_strcpy(name, WINS_AddrToString(addr));
	return 0;
}

//=============================================================================

int WINS_GetAddrFromName(char* name, struct qsockaddr* addr)
{
	if (name[0] >= '0' && name[0] <= '9')
		return PartialIPAddress(name, addr);

	auto hostentry = gethostbyname(name);
	if (!hostentry)
		return -1;

	addr->sa_family = AF_INET;
	reinterpret_cast<sockaddr_in *>(addr)->sin_port = htons(static_cast<unsigned short>(net_hostport));
	reinterpret_cast<sockaddr_in *>(addr)->sin_addr.s_addr = *reinterpret_cast<int *>(hostentry->h_addr_list[0]);

	return 0;
}

//=============================================================================

int WINS_AddrCompare(struct qsockaddr* addr1, struct qsockaddr* addr2)
{
	if (addr1->sa_family != addr2->sa_family)
		return -1;

	if (reinterpret_cast<struct sockaddr_in *>(addr1)->sin_addr.s_addr != reinterpret_cast<struct sockaddr_in *>(addr2)->sin_addr.s_addr)
		return -1;

	if (reinterpret_cast<struct sockaddr_in *>(addr1)->sin_port != reinterpret_cast<struct sockaddr_in *>(addr2)->sin_port)
		return 1;

	return 0;
}

//=============================================================================

int WINS_GetSocketPort(struct qsockaddr* addr)
{
	return ntohs(reinterpret_cast<struct sockaddr_in *>(addr)->sin_port);
}


int WINS_SetSocketPort(struct qsockaddr* addr, int port)
{
	reinterpret_cast<struct sockaddr_in *>(addr)->sin_port = htons(static_cast<unsigned short>(port));
	return 0;
}

//=============================================================================
