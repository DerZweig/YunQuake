#pragma once
int WINS_Init();
void WINS_Shutdown();
void WINS_Listen(qboolean state);
int WINS_OpenSocket(int port);
int WINS_CloseSocket(int socket);
int WINS_Connect(int socket, qsockaddr* addr);
int WINS_CheckNewConnections();
int WINS_Read(int socket, byte* buf, int len, qsockaddr* addr);
int WINS_Write(int socket, byte* buf, int len, qsockaddr* addr);
int WINS_Broadcast(int socket, byte* buf, int len);
char* WINS_AddrToString(qsockaddr* addr);
int WINS_StringToAddr(char* string, qsockaddr* addr);
int WINS_GetSocketAddr(int socket, qsockaddr* addr);
int WINS_GetNameFromAddr(qsockaddr* addr, char* name);
int WINS_GetAddrFromName(char* name, qsockaddr* addr);
int WINS_AddrCompare(qsockaddr* addr1, qsockaddr* addr2);
int WINS_GetSocketPort(qsockaddr* addr);
int WINS_SetSocketPort(qsockaddr* addr, int port);
