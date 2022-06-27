#include <thread>

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <assert.h>

#include <arpa/inet.h>
#include <unistd.h>

#include <pthread.h>


sa_family_t IPv4 = AF_INET;
sa_family_t IPv6 = AF_INET6;

sa_family_t TCP = SOCK_STREAM;
sa_family_t UDP = SOCK_DGRAM;


constexpr unsigned MAXIMUM_NUMBER_OF_CLIENTS = 255;
static int client_sockets[MAXIMUM_NUMBER_OF_CLIENTS];

void DispatchMessage(int id, const char* message, int size);


void TerminateClient(int socket_fd, int code, const char* message)
{
    // Not thread safe!
    char temp[255] = { 0 };

    for (unsigned i = 0; i < MAXIMUM_NUMBER_OF_CLIENTS; ++i)
    {
        int client_socket = client_sockets[i];
        if (client_socket == socket_fd)
        {
            close(client_socket);
            client_sockets[i] = 0;
            break;
        }
    }

    sprintf(temp, ">>> Client %d left <<<\n", socket_fd);
    DispatchMessage(socket_fd, temp, strlen(temp));

    printf("[Exit %d]: %s\n", errno, message);fflush(stdout);
    pthread_exit((void *) code);
}


void Terminate(int code, const char* message)
{
    printf("[Exit %d]: %s\n", errno, message);fflush(stdout);
    exit(code);
}


void DispatchMessage(int socket_fd, const char* message, int size)
{
    for (int i = 0; i < MAXIMUM_NUMBER_OF_CLIENTS; ++i)
    {
        int client_socket = client_sockets[i];
        if (client_socket == 0 || client_socket == socket_fd)
            continue;

        ssize_t bytes_written = write(client_socket, message, size);
        if (bytes_written == -1)
            printf("Couldn't write to socket %d.\n", client_socket);
    }
}


void* HandleClient(void* data)
{
    int socket_fd = *(int *) data;

    constexpr   size_t BUFFER_SIZE  = 1023;
    static char buffer[BUFFER_SIZE] = { 0 };

    sprintf(buffer, ">>> Client %d joined <<<\n", socket_fd);
    printf("%s", buffer);fflush(stdout);

    DispatchMessage(socket_fd, buffer, strlen(buffer));

    sprintf(buffer, "%d", socket_fd);
    // We start off by sending the ID-number to the client. Not at all necessary as it's not needed by the
    // client, but here we probably would send the client some initial setup data.
    ssize_t bytes_written = write(socket_fd, buffer, strlen(buffer));
    if (bytes_written == -1)
        TerminateClient(socket_fd, 0, "Couldn't write to socket.");
    printf("[Info]: Sent ID to the client.\n");fflush(stdout);


    // Some handy variables to have.
    int max_transmission_size = 1000;
    int max_size_to_receive   = 1000;

    snprintf(buffer, BUFFER_SIZE, "Client %d: ", socket_fd);  // This is just what we'll print before the client's message.
    int buffer_start_index = strlen(buffer);

    // This will run until the client disconnects. It's from here we'll receive all messages from the client.
    while (true)
    {
        // Wait for message
        // http://man7.org/linux/man-pages/man2/recvmsg.2.html
        //     recv(socket, buffer, size, flags)
        //          socket: any socket.
        //          buffer: array to fill with the message.
        //          size: the size of the buffer.
        //          flags: options.
        ssize_t bytes_received = recv(socket_fd, &buffer[buffer_start_index], max_size_to_receive, 0);
        if (bytes_received == -1)
        {
            printf("Issue with connection.\n");fflush(stdout);
            break;
        }
        else if (bytes_received == 0)
        {
            printf("Client %d disconnected.\n", socket_fd);fflush(stdout);
            break;
        }

        buffer[buffer_start_index + bytes_received] = '\0';
        printf("%s", buffer);fflush(stdout);
        DispatchMessage(socket_fd, buffer, strlen(buffer));

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }


    TerminateClient(socket_fd, 0, "Tearing down client.");
    return 0;
}



int main(int argc, char* argv[])
{
    if (argc != 2)
        Terminate(1, "Usage: <port>");

    const char* address = "127.0.0.1";
    const int   port = atoi(argv[1]);

    // The steps involved in establishing a socket on the server side are as follows:
    //
    //     1. Create a socket with the socket() system call.
    //     2. Bind the socket to an address using the bind() system call. For a server socket on the Internet,
    //        an address consists of a port number on the host machine.
    //     3. Listen for connections with the listen() system call.
    //     4. Accept a connection with the accept() system call. This call typically blocks until a client connects
    //        with the server.
    //     5. Send and receive data.

    int success = 0;

    // http://man7.org/linux/man-pages/man2/socket.2.html
    //     socket(domain, type, protocol) creates an endpoint for communication and returns a file descriptor that
    //     refers to that endpoint
    //         domain: most commonly AF_INET (IPv4) or AF_INET6 (IPv6)
    //         type: most commonly SOCK_STREAM or SOCK_DGRAM
    //         protocol: 0 unless more protocols in the protocol family exist.
    int listen_socket = socket(IPv4, TCP, 0);
    if (listen_socket == -1)
        Terminate(success, "Couldn't create socket.");


    // http://man7.org/linux/man-pages/man2/bind.2.html
    //     bind(socket, address, size) assigns/binds the address to the socket.
    //
    sockaddr_in server_address{};
    server_address.sin_family = IPv4;
    server_address.sin_port   = htons(port);  // htons - Host to Network Short. Flip endianness to match machine.
    success = inet_pton(IPv4, address, &server_address.sin_addr);  // Pointer (to String) to Number.
    if (success <= 0)
        Terminate(success, "Invalid address.");

    success = bind(listen_socket, (sockaddr*)&server_address, sizeof(server_address));
    if (success == -1)
        Terminate(success, "Couldn't bind socket.");


    // http://man7.org/linux/man-pages/man2/listen.2.html
    //     listen(socket, backlog) marks the socket as a passive socket that will be used to accept incoming connection.
    //        socket: socket of type SOCK_STREAM or SOCK_SEQPACKET.
    //        backlog: the maximum length to which the queue of pending connections for socket may grow.
    success = listen(listen_socket, MAXIMUM_NUMBER_OF_CLIENTS);
    if (success == -1)
        Terminate(success, "Can't listen to socket.");

    printf("[Info]: Waiting for clients...\n");fflush(stdout);

    while (true)
    {
        // http://man7.org/linux/man-pages/man2/accept.2.html
        //     accept(socket, address, size, flags)
        //         socket: socket of type SOCK_STREAM or SOCK_SEQPACKET.
        sockaddr_in client_address{};
        socklen_t   client_address_size = sizeof(client_address);
        int client_socket = accept(listen_socket, (sockaddr*)&client_address, &client_address_size);
        if (client_socket == -1)
            Terminate(success, "Couldn't accept request from client.");
        printf("[Info]: A client connected!\n");fflush(stdout);

        // Now we have two sockets. One for listening for new clients (the 'listening_socket') and one for
        // communicating with our connected client (the 'client_socket').

        for (unsigned i = 0; i < MAXIMUM_NUMBER_OF_CLIENTS; ++i)
        {
            if (client_sockets[i] == 0)
            {
                client_sockets[i] = client_socket;
                break;
            }
        }

        int* client_data = new int{ client_socket };  // @Leak!

        pthread_t thread;
        int status = pthread_create(&thread, NULL, HandleClient, (void *) client_data);
        if (status != 0)
            Terminate(status, "Couldn't create thread.");
    }
}
