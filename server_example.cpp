#include <thread>

#include <stdio.h>
#include <string.h>
#include <errno.h>

#include <arpa/inet.h>
#include <unistd.h>


void Terminate(int code, const char* message)
{
    printf("[Exit %d]: %s\n", errno, message);
    exit(code);
}


sa_family_t IPv4 = AF_INET;
sa_family_t IPv6 = AF_INET6;

sa_family_t TCP = SOCK_STREAM;
sa_family_t UDP = SOCK_DGRAM;


int main(int argc, char* argv[])
{
    if (argc != 2)
        Terminate(1, "Usage: <port>\n");

    const char* address = "127.0.0.1";
    const int   port = atoi(argv[1]);

    int max_number_of_clients = 1000;
    int max_size_to_receive   = 1000;

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
    success = listen(listen_socket, max_number_of_clients);
    if (success == -1)
        Terminate(success, "Can't listen to socket.");

    printf("[Info]: Waiting for clients...\n");

    unsigned counter = 0;
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
        printf("[Info]: A client connected!\n");

        // Now we have two sockets. One for listening for new clients (the 'listening_socket') and one for
        // communicating with our connected client (the 'client_socket').

        ++counter;

        // http://man7.org/linux/man-pages/man2/fork.2.html
        // Forking clones the original process into two equivalent processes. The PID (process ID) allows us to branch
        // and do different things on each process. If we didn't have a branch on the PID, then we'd have two equivalent
        // programs (which is quite unnecessary).
        int pid = fork();

        if (pid < 0)
        {
            // Something went wrong when we tried to fork.
            Terminate(success, "Couldn't create fork.");
        }
        else if (pid != 0)
        {
            // This is the original process. It will run until we turn off the server.
            // In this version of the executable, we'll close the connection with the client and continue to listen
            // if there are more clients coming in.
            close(client_socket);
        }
        else
        {
            // This process is the duplicated process. It will run until the client disconnects.
            // In this version of the executable, we'll stop listening for incoming clients and instead handle the
            // client we just connected to.
            close(listen_socket);

            unsigned id = counter;
            printf(">>> Client %d joined <<<\n", counter);

            char number[5];
            sprintf(number, "%d", id);

            // We start off by sending the ID-number to the client. Not at all necessary as it's not needed by the
            // client, but here we probably would send the client some initial setup data.
            ssize_t bytes_written = write(client_socket, number, strlen(number));
            if (bytes_written == -1)
                Terminate(success, "Couldn't write to socket.");
            printf("[Info]: Sent ID to the client.\n");


            // Some handy variables to have.
            int max_transmission_size = 1000;
            char starter[255];
            snprintf(starter, 255, "Client %d: ", id);  // This is just what we'll print before the client's message.


            // This will run until the client disconnects. It's from here we'll receive all messages from the client.
            while (true)
            {
                char* message = new char[max_transmission_size];

                // Wait for message
                // http://man7.org/linux/man-pages/man2/recvmsg.2.html
                //     recv(socket, buffer, size, flags)
                //          socket: any socket.
                //          buffer: array to fill with the message.
                //          size: the size of the buffer.
                //          flags: options.
                ssize_t bytes_received = recv(client_socket, message, max_size_to_receive, 0);
                if (bytes_received == -1)
                    Terminate(success, "Issue with connection.");
                else if (bytes_received == 0)
                    Terminate(success, "Client disconnected.");

                printf("%s%s", starter, message);
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }

            Terminate(0, "Tearing down client.\n");
        }
    }
}
