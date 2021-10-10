#include <iostream>
#include <string>
#include <thread>
#include <mutex>

#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdio.h>

#pragma comment(lib, "Ws2_32.lib")

std::string nickname{ "earth core" };
unsigned max_chat_hystory_lines{ 20 };
char message_received[80]; //server side uses c-style strings, cliend uses vector<char> and related methods . just for sake of practice.
//for socket on 64bit system. https://stackoverflow.com/questions/1953639/is-it-safe-to-cast-socket-to-int-under-win64
using socket_t = decltype(socket(0, 0, 0));

void print_vec(const std::vector<std::string>& vec)
{
    for (auto x = vec.rbegin(); x != vec.rend(); ++x) {
        std::cout << *x;
    }
    std::cout << '\n';
}
void receiveMessages(std::vector<std::string>* chat_story, socket_t* newSd, bool* exit, std::mutex& mutex) {
    //alternative solution with std::string buffer /or vector<char>
    //auto bytesReceived = recv(socket, buffer.data(), 4000, 0);
    //red from buffer all between buffer.front() and buffer[bytesReceived] (does this resize the string? do I need to if I ... I can set max string size in initiation.)
    std::string input_message{};
    while (true) {
        if (*exit == true) break;
        memset(&message_received, 0, sizeof(message_received));
        auto bytesRecv = recv(*newSd, message_received, sizeof(message_received), 0);

        if (!strcmp(message_received, " ")) //exit
        {
            mutex.lock();
            (*chat_story).insert((*chat_story).begin(), "client has disconnected. \n");
            mutex.unlock();
            *exit = true;
            if (chat_story->size() > max_chat_hystory_lines)
            {
                mutex.lock();
                chat_story->pop_back();
                mutex.unlock();
            }
            print_vec(*chat_story);
            break;
        }
        if (bytesRecv > 0) {
            input_message = std::string(message_received) + "\n";
            mutex.lock();
            chat_story->insert(chat_story->begin(), input_message);
            mutex.unlock();
            if (chat_story->size() > max_chat_hystory_lines)
            {
                mutex.lock();
                chat_story->pop_back();
                mutex.unlock();
            }
            print_vec(*chat_story);
        }
    }
}
void sendMessages(std::vector<std::string>* chat_story, socket_t* clientSd, bool* exit, std::mutex& mutex) {
    std::string client_input;
    while (true)
    {
        std::cout << ">";
        std::getline(std::cin, client_input);
        if (client_input == "exit") {
            send(*clientSd, " ", 1, 0);
            *exit = true;
            break;
        }
        client_input.insert(0, nickname + ": ");
        send(*clientSd, client_input.data(), client_input.length(), 0);

        mutex.lock();
        chat_story->insert(chat_story->begin(), client_input + "\n");
        mutex.unlock();
        if (chat_story->size() > max_chat_hystory_lines)
        {
            mutex.lock();
            chat_story->pop_back();
            mutex.unlock();
        }
        print_vec(*chat_story);
    }
}
//Client side

//Server side
int main(int argc, char* argv[])
{
    WSADATA wsaData;
    int iResult;
    iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != 0) {
        printf("WSAStartup failed with error: %d\n", iResult);
        return 1;
    }

    const int port = 12010;
    //setup a socket and connection tools
    sockaddr_in servAddr;
    //clear the buffer
    memset((char*)&servAddr, '\0', sizeof(servAddr));
    servAddr.sin_family = AF_INET;
    servAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servAddr.sin_port = htons(port);

    //open stream oriented socket with internet address
    //also keep track of the socket descriptor
    socket_t serverSd = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSd == INVALID_SOCKET)
    {
        std::cerr << "Error establishing the server socket" << std::endl;
        return 1;
    }
    //bind the socket to its local address
    int bindStatus = bind(serverSd, (struct sockaddr*)&servAddr,
        sizeof(servAddr));
    if (bindStatus < 0)
    {
        std::cerr << "Error binding socket to local address" << std::endl;
        return 1;
    }
    std::cout << "Waiting for a client to connect..." << std::endl;
    //listen for 1 requests at a time
    listen(serverSd, 1);
    //receive a request from client using accept
    //we need a new address to connect with the client
    sockaddr_in newSockAddr;
    socklen_t newSockAddrSize = sizeof(newSockAddr);
    //accept, create a new socket descriptor to 
    //handle the new connection with client
    socket_t newSd = accept(serverSd, (sockaddr*)&newSockAddr, &newSockAddrSize); // was int newSd
    if (newSd < 0)
    {
        std::cerr << "Error accepting request from client!" << std::endl;
        return 1;
    }
    std::cout << "Connected with client!" << std::endl;

    // socket options https://stackoverflow.com/questions/30395258/setting-timeout-to-recv-function
    DWORD timeout = 1 * 1000;
    setsockopt(newSd, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout));

    //main 
    // 
    //some variables and buffers
    std::vector<std::string> chat_story{};
    std::vector<std::string>* chat_ptr{ &chat_story };
    std::mutex chat_story_mutex;
    bool exit = false;
    bool* exit_ptr{ &exit };

     //receive messages
    socket_t* newSd_ptr{ &newSd };
    std::thread recv_thread(receiveMessages, chat_ptr, newSd_ptr, exit_ptr, std::ref(chat_story_mutex));
    std::thread send_thread(sendMessages, chat_ptr, newSd_ptr, exit_ptr, std::ref(chat_story_mutex));
    recv_thread.join();
    send_thread.join();

    closesocket(newSd);
    closesocket(serverSd);

    std::cout << "Connection closed..." << std::endl;
    return 0;
}