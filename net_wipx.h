#pragma once
int WIPX_Init();
void WIPX_Shutdown();
void WIPX_Listen(qboolean state);
int WIPX_OpenSocket(int port);
int WIPX_CloseSocket(int socket);
int WIPX_Connect(int socket, qsockaddr* addr);
int WIPX_CheckNewConnections();
int WIPX_Read(int socket, byte* buf, int len, qsockaddr* addr);
int WIPX_Write(int socket, byte* buf, int len, qsockaddr* addr);
int WIPX_Broadcast(int socket, byte* buf, int len);
char* WIPX_AddrToString(qsockaddr* addr);
int WIPX_StringToAddr(char* string, qsockaddr* addr);
int WIPX_GetSocketAddr(int socket, qsockaddr* addr);
int WIPX_GetNameFromAddr(qsockaddr* addr, char* name);
int WIPX_GetAddrFromName(char* name, qsockaddr* addr);
int WIPX_AddrCompare(qsockaddr* addr1, qsockaddr* addr2);
int WIPX_GetSocketPort(qsockaddr* addr);
int WIPX_SetSocketPort(qsockaddr* addr, int port);
