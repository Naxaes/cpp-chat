#include <thread>

#include <stdio.h>
#include <string.h>
#include <errno.h>

#include <arpa/inet.h>
#include <unistd.h>


void Terminate(int code, const char* message)
{
    printf("[Exit %d]: %s\n", errno, message);fflush(stdout);
    exit(code);
}


sa_family_t IPv4 = AF_INET;
sa_family_t IPv6 = AF_INET6;

sa_family_t TCP = SOCK_STREAM;
sa_family_t UDP = SOCK_DGRAM;


int main(int argc, char* argv[])
{
    if (argc != 3)
        Terminate(1, "Usage: <address> <port>\n");

    const char* address = argv[1];
    const int   port = atoi(argv[2]);

    int max_size_to_receive = 1000;
    int success = 0;

    printf("[Info]: Trying to connect to server %s on port %d.\n", address, port);fflush(stdout);
    
    // http://man7.org/linux/man-pages/man2/socket.2.html
    int client_socket = socket(IPv4, TCP, 0);
    if (client_socket == -1)
        Terminate(success, "Couldn't create socket.");


    // http://man7.org/linux/man-pages/man7/ip.7.html
    // struct sockaddr_in {
    //     sa_family_t    sin_family; /* address family: AF_INET */
    //     in_port_t      sin_port;   /* port in network byte order */
    //     struct in_addr sin_addr;   /* internet address */
    // };
    // sin_family is always set to AF_INET.
    sockaddr_in server_address{};
    server_address.sin_family = IPv4;
    server_address.sin_port   = htons(port);  // htons - Host to Network Short. Flip endianness to match machine.
    success = inet_pton(IPv4, address, &server_address.sin_addr);  // pton == Pointer (to String) to Number.
    if (success <= 0)
        Terminate(success, "Invalid address.");
    success = connect(client_socket, (sockaddr*)&server_address, sizeof(server_address));
    if (success == -1)
        Terminate(success, "Couldn't connect to server.");

    printf("[Info]: Connected to server!\n");fflush(stdout);


    // Start by receiving the ID and then the welcome message.
    char* buffer = new char[max_size_to_receive];

    // http://man7.org/linux/man-pages/man2/recvmsg.2.html
    //     recv(socket, buffer, size, flags)
    //          socket: any socket.
    //          buffer: array to fill with the message.
    //          size: the size of the buffer.
    //          flags: options.
    ssize_t bytes_received = recv(client_socket, buffer, max_size_to_receive, 0);
    if (bytes_received == -1)
        Terminate(success, "Issue with connection.");
    else if (bytes_received == 0)
        Terminate(success, "The server disconnected.");

    int id = atoi(buffer);
    printf("[Info]: Connected with id %d.\n", id);fflush(stdout);

    // This will run until we disconnect. It's from here we'll send all messages to the server.
    while (true)
    {
        char message[1000] = { 0 };
        printf(">>> ");fflush(stdout);
        read(STDIN_FILENO, message, 1000);

        if (message[0] != '\0')
        {
            ssize_t bytes_written = write(client_socket, message, strlen(message));
            if (bytes_written == -1)
                Terminate(success, "Couldn't write to socket.");
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}