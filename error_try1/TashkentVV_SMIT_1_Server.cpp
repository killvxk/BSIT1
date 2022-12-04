#define _CRT_SECURE_NO_WARNINGS
// File TashkentVV_SMIT_1_Server.cpp

#include <iostream>
#include "interface_h.h"
#include <fstream>
#include <windows.h>
#include <stdio.h>

#define MAX_BUF 100000
#define MAX_CLIENTS 100
#pragma comment (lib, "Rpcrt4.lib")

typedef struct info_client {
    handle_t handle;
    FILE* file;
    int file_status = 0;
};
info_client clients[MAX_CLIENTS];

int count_cur_clients = 0;

int login_client(const char* login, const char* password, int* index)
{
    int index_new = 0;
    handle_t handle = 0;

    if (count_cur_clients > MAX_CLIENTS) return 1;

    while (clients[index_new].handle)
        index_new++;

    if (index_new == MAX_CLIENTS) return 1;
    //���� ������� ����������� �������, �� ��������� ���������� ������, ������� ������������ ��������� � ������� ������������. \
	����� �� ������ ������������ ���� ���������� ������ ��� ������������� ���������� ������������ ���, � ����������� �������, \
	��� �������� ��������, ������� ����������� � ��������� ���������� ������������.
    if (!LogonUserA((LPCSTR)login, NULL, (LPCSTR)password, LOGON32_LOGON_INTERACTIVE, LOGON32_PROVIDER_DEFAULT, &handle))
    {
        printf("The user name or password is incorrect. Try again.\n");
        return -1;
    }
    //������� ImpersonateLoggedOnUser ��������� ����������� ������ ������������ �������� ������������ ��������� � ������� ������������. \
	������������ ����������� ������������ ������.
    if (!ImpersonateLoggedOnUser(handle))
    {
        printf("Impersonate error.\n");
        return -1;
    }

    clients[index_new].handle = handle;
    clients[index_new].file = 0;
    clients[index_new].file_status = 0;
    *index = index_new;
    printf("�lient connected.");
    return 0;
}

int download_to_client(const char* path, char buf[MAX_BUF], int* length_buf, int index, int* check_eof)
{
    if (!ImpersonateLoggedOnUser(clients[index].handle))
    {
        printf("Impersonate error.\n");
        return -1;
    }
    if (!clients[index].file_status)
    {
        if (!(clients[index].file = fopen(path, "rb")))
        {
            printf("File isn't found");
            return 1;
        }
        clients[index].file_status = 1;
    }
    *length_buf = fread(buf, sizeof(char), MAX_BUF, clients[index].file);
    if (*length_buf < MAX_BUF)
    {
        clients[index].file_status = 0;
        fclose(clients[index].file);
    }
    return 0;
}

int send_to_server(const char* file_name, char buf[MAX_BUF], int length_buf, int index, int check_eof)
{
    int i = 0;
    if (!ImpersonateLoggedOnUser(clients[index].handle))
    {
        printf("Impersonate error!");
        return -1;
    }

    if (!clients[index].file_status)
    {
        //FILE* test = fopen("i_am_lucky.txt", "wb");
        clients[index].file = fopen(file_name, "wb");
        clients[index].file_status = 1;
    }
    if (!clients[index].file)
    {
        printf("No write access\n");
        return 1;
    }
    fwrite(buf, sizeof(char), length_buf, clients[index].file);
    if (length_buf < MAX_BUF)
    {
        clients[index].file_status = 0;
        fclose(clients[index].file);
    }
    return 0;
}

int delete_file_on_server(const char* path, int index)
{
    if (!ImpersonateLoggedOnUser(clients[index].handle))
    {
        printf("Impersonate error.\n");
        return -1;
    }
    //
    if (remove((const char*)path) == -1)
    {
        printf("Error remove.\n");
        return 1;
    }
    printf("Successfully!\n");
    return 0;
}

int client_out(int index)
{
    CloseHandle(clients[index].handle);
    clients[index].handle = NULL;
    clients[index].file = NULL;
    clients[index].file_status = 0;

    count_cur_clients--;
    printf("Client disconnected");
    return 0;
}

// Naive security callback.
RPC_STATUS CALLBACK SecurityCallback(RPC_IF_HANDLE /*hInterface*/, void* /*pBindingHandle*/)
{
    return RPC_S_OK; // Always allow anyone.
}





int main()
{
    setlocale(LC_ALL, "rus");
    SetConsoleCP(1251);
    SetConsoleOutputCP(1251); // ���������� ������� ����

    	char dst_address[10] = { "\0" };
	printf("Enter port: ");
	fgets(dst_address, sizeof(dst_address), stdin);
	dst_address[strlen(dst_address) - 1] = '\0';

    RPC_STATUS status;
    RpcServerRegisterAuthInfoA(
        nullptr,
        RPC_C_AUTHN_WINNT,
        0,
        nullptr);


    // Uses the protocol combined with the endpoint for receiving
    // remote procedure calls.
    status = RpcServerUseProtseqEpA(
        (RPC_CSTR)("ncacn_ip_tcp"),                     // Use TCP/IP protocol.
        RPC_C_PROTSEQ_MAX_REQS_DEFAULT,                 // Backlog queue length for TCP/IP.
        (RPC_CSTR)(dst_address),                        // TCP/IP port to use.
        NULL);                                          // No security.

    if (status)
        exit(status);


    // Registers the InterfaceRPC interface.
    //������� RpcServerRegisterIf2 ������������ ��������� � ����������� ������� ���������� RPC.
    status = RpcServerRegisterIf2(
        InterfaceRPC_v1_0_s_ifspec,                    // Interface to register.
        NULL,                                          // Use the MIDL generated entry-point vector.
        NULL,                                          // Use the MIDL generated entry-point vector.
        RPC_IF_ALLOW_CALLBACKS_WITH_NO_AUTH,           // Forces use of security callback.
        RPC_C_LISTEN_MAX_CALLS_DEFAULT,                // Use default number of concurrent calls.
        (unsigned)-1,                                  // Infinite max size of incoming data blocks.
        SecurityCallback);                             // Naive security callback.
                   

    if (status)
        exit(status);
    printf("Listening...");



    // Start to listen for remote procedure
    // calls for all registered interfaces.
    // This call will not return until
    // RpcMgmtStopServerListening is called.
    // ������� RpcServerListen ������������� ���������� ������� ���������� RPC ������������ ��������� ������ ��������
    status = RpcServerListen(
        1,                                 // Recommended minimum number of threads.
        RPC_C_LISTEN_MAX_CALLS_DEFAULT,    // Recommended maximum number of threads.
        FALSE);                            // Start listening now.
    if (status)
        exit(status);

}

// Memory allocation function for RPC.
// The runtime uses these two functions for allocating/deallocating
// enough memory to pass the string to the server.
void* __RPC_USER midl_user_allocate(size_t size)
{
    return malloc(size);
}

// Memory deallocation function for RPC.
void __RPC_USER midl_user_free(void* p)
{
    free(p);
}