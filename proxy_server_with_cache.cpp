#include "proxy_parse.h"
#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <ctime>
#include <sys/wait.h>
#include <cerrno>
#include <pthread.h>
#include <semaphore.h>
#include <memory>
#include <vector>
#include <string>

#define MAX_BYTES 4096
#define MAX_CLIENTS 400
#define MAX_SIZE 200*(1<<20)
#define MAX_ELEMENT_SIZE 10*(1<<20)

struct CacheElement {
    char* data;
    int len;
    char* url;
    time_t lru_time_track;
    CacheElement* next;
    
    CacheElement() : data(nullptr), len(0), url(nullptr), lru_time_track(0), next(nullptr) {}
    ~CacheElement() {
        if (data) free(data);
        if (url) free(url);
    }
};

CacheElement* find(const char* url);
int add_cache_element(const char* data, int size, const char* url);
void remove_cache_element();

int port_number = 8080;
int proxy_socketId;
std::vector<pthread_t> tid(MAX_CLIENTS);
sem_t seamaphore;
pthread_mutex_t lock;

CacheElement* head = nullptr;
int cache_size = 0;

int sendErrorMessage(int socket, int status_code) {
    char str[1024];
    char currentTime[50];
    time_t now = time(0);

    struct tm data = *gmtime(&now);
    strftime(currentTime, sizeof(currentTime), "%a, %d %b %Y %H:%M:%S %Z", &data);

    switch(status_code) {
        case 400: 
            snprintf(str, sizeof(str), "HTTP/1.1 400 Bad Request\r\nContent-Length: 95\r\nConnection: keep-alive\r\nContent-Type: text/html\r\nDate: %s\r\nServer: VaibhavN/14785\r\n\r\n<HTML><HEAD><TITLE>400 Bad Request</TITLE></HEAD>\n<BODY><H1>400 Bad Rqeuest</H1>\n</BODY></HTML>", currentTime);
            std::cout << "400 Bad Request\n";
            send(socket, str, strlen(str), 0);
            break;

        case 403: 
            snprintf(str, sizeof(str), "HTTP/1.1 403 Forbidden\r\nContent-Length: 112\r\nContent-Type: text/html\r\nConnection: keep-alive\r\nDate: %s\r\nServer: VaibhavN/14785\r\n\r\n<HTML><HEAD><TITLE>403 Forbidden</TITLE></HEAD>\n<BODY><H1>403 Forbidden</H1><br>Permission Denied\n</BODY></HTML>", currentTime);
            std::cout << "403 Forbidden\n";
            send(socket, str, strlen(str), 0);
            break;

        case 404: 
            snprintf(str, sizeof(str), "HTTP/1.1 404 Not Found\r\nContent-Length: 91\r\nContent-Type: text/html\r\nConnection: keep-alive\r\nDate: %s\r\nServer: VaibhavN/14785\r\n\r\n<HTML><HEAD><TITLE>404 Not Found</TITLE></HEAD>\n<BODY><H1>404 Not Found</H1>\n</BODY></HTML>", currentTime);
            std::cout << "404 Not Found\n";
            send(socket, str, strlen(str), 0);
            break;

        case 500: 
            snprintf(str, sizeof(str), "HTTP/1.1 500 Internal Server Error\r\nContent-Length: 115\r\nConnection: keep-alive\r\nContent-Type: text/html\r\nDate: %s\r\nServer: VaibhavN/14785\r\n\r\n<HTML><HEAD><TITLE>500 Internal Server Error</TITLE></HEAD>\n<BODY><H1>500 Internal Server Error</H1>\n</BODY></HTML>", currentTime);
            send(socket, str, strlen(str), 0);
            break;

        case 501: 
            snprintf(str, sizeof(str), "HTTP/1.1 501 Not Implemented\r\nContent-Length: 103\r\nConnection: keep-alive\r\nContent-Type: text/html\r\nDate: %s\r\nServer: VaibhavN/14785\r\n\r\n<HTML><HEAD><TITLE>404 Not Implemented</TITLE></HEAD>\n<BODY><H1>501 Not Implemented</H1>\n</BODY></HTML>", currentTime);
            std::cout << "501 Not Implemented\n";
            send(socket, str, strlen(str), 0);
            break;

        case 505: 
            snprintf(str, sizeof(str), "HTTP/1.1 505 HTTP Version Not Supported\r\nContent-Length: 125\r\nConnection: keep-alive\r\nContent-Type: text/html\r\nDate: %s\r\nServer: VaibhavN/14785\r\n\r\n<HTML><HEAD><TITLE>505 HTTP Version Not Supported</TITLE></HEAD>\n<BODY><H1>505 HTTP Version Not Supported</H1>\n</BODY></HTML>", currentTime);
            std::cout << "505 HTTP Version Not Supported\n";
            send(socket, str, strlen(str), 0);
            break;

        default:  
            return -1;
    }
    return 1;
}

int connectRemoteServer(const char* host_addr, int port_num) {
    int remoteSocket = socket(AF_INET, SOCK_STREAM, 0);

    if(remoteSocket < 0) {
        std::cerr << "Error in Creating Socket.\n";
        return -1;
    }
    
    struct hostent *host = gethostbyname(host_addr);	
    if(host == nullptr) {
        std::cerr << "No such host exists.\n";	
        return -1;
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port_num);
    memcpy(&server_addr.sin_addr.s_addr, host->h_addr, host->h_length);

    if(connect(remoteSocket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        std::cerr << "Error in connecting!\n"; 
        return -1;
    }
    return remoteSocket;
}

int handle_request(int clientSocket, ParsedRequest *request, const char *tempReq) {
    std::string buf = "GET ";
    buf += request->path;
    buf += " ";
    buf += request->version;
    buf += "\r\n";

    if (ParsedHeader_set(request, "Connection", "close") < 0) {
        std::cerr << "set header key not work\n";
    }

    if(ParsedHeader_get(request, "Host") == nullptr) {
        if(ParsedHeader_set(request, "Host", request->host) < 0) {
            std::cerr << "Set \"Host\" header key not working\n";
        }
    }

    char headers[MAX_BYTES];
    if (ParsedRequest_unparse_headers(request, headers, MAX_BYTES - buf.size()) < 0) {
        std::cerr << "unparse failed\n";
    }
    buf += headers;

    int server_port = 80;
    if(request->port != nullptr) {
        server_port = atoi(request->port);
    }

    int remoteSocketID = connectRemoteServer(request->host, server_port);
    if(remoteSocketID < 0) {
        return -1;
    }

    int bytes_send = send(remoteSocketID, buf.c_str(), buf.size(), 0);
    if (bytes_send < 0) {
        std::cerr << "Error sending request to remote server\n";
        close(remoteSocketID);
        return -1;
    }

    std::vector<char> temp_buffer;
    char recv_buffer[MAX_BYTES];
    while((bytes_send = recv(remoteSocketID, recv_buffer, MAX_BYTES-1, 0)) > 0) {
        int sent = send(clientSocket, recv_buffer, bytes_send, 0);
        if (sent < 0) {
            std::cerr << "Error in sending data to client socket.\n";
            break;
        }
        temp_buffer.insert(temp_buffer.end(), recv_buffer, recv_buffer + bytes_send);
    }

    if (!temp_buffer.empty()) {
        add_cache_element(temp_buffer.data(), temp_buffer.size(), tempReq);
    }
    
    std::cout << "Done\n";
    close(remoteSocketID);
    return 0;
}

int checkHTTPversion(const char *msg) {
    if(strncmp(msg, "HTTP/1.1", 8) == 0) {
        return 1;
    }
    else if(strncmp(msg, "HTTP/1.0", 8) == 0) {
        return 1;
    }
    return -1;
}

void* thread_fn(void* arg) {
    sem_wait(&seamaphore); 
    int p;
    sem_getvalue(&seamaphore, &p);
    std::cout << "semaphore value:" << p << std::endl;
    
    int socket = *static_cast<int*>(arg);
    int bytes_send_client, len;
    
    std::vector<char> buffer(MAX_BYTES, 0);
    bytes_send_client = recv(socket, buffer.data(), MAX_BYTES, 0);
    
    while(bytes_send_client > 0) {
        len = strlen(buffer.data());
        if(strstr(buffer.data(), "\r\n\r\n") == nullptr) {	
            bytes_send_client = recv(socket, buffer.data() + len, MAX_BYTES - len, 0);
        }
        else {
            break;
        }
    }

    std::string tempReq(buffer.data(), buffer.size());
    CacheElement* temp = find(tempReq.c_str());

    if(temp != nullptr) {
        int size = temp->len;
        int pos = 0;
        char response[MAX_BYTES];
        while(pos < size) {
            memset(response, 0, MAX_BYTES);
            int chunk_size = std::min(MAX_BYTES, size - pos);
            memcpy(response, temp->data + pos, chunk_size);
            send(socket, response, chunk_size, 0);
            pos += chunk_size;
        }
        std::cout << "Data retrieved from the Cache\n\n";
        std::cout << response << "\n\n";
    }
    else if(bytes_send_client > 0) {
        len = strlen(buffer.data());
        ParsedRequest* request = ParsedRequest_create();
        
        if (ParsedRequest_parse(request, buffer.data(), len) < 0) {
            std::cerr << "Parsing failed\n";
        }
        else {	
            if(!strcmp(request->method, "GET")) {
                if(request->host && request->path && (checkHTTPversion(request->version) == 1)) {
                    bytes_send_client = handle_request(socket, request, tempReq.c_str());
                    if(bytes_send_client == -1) {	
                        sendErrorMessage(socket, 500);
                    }
                }
                else {
                    sendErrorMessage(socket, 500);
                }
            }
            else {
                std::cout << "This code doesn't support any method other than GET\n";
            }
        }
        ParsedRequest_destroy(request);
    }
    else if(bytes_send_client < 0) {
        std::cerr << "Error in receiving from client.\n";
    }
    else if(bytes_send_client == 0) {
        std::cout << "Client disconnected!\n";
    }

    shutdown(socket, SHUT_RDWR);
    close(socket);
    sem_post(&seamaphore);	
    
    sem_getvalue(&seamaphore, &p);
    std::cout << "Semaphore post value:" << p << std::endl;
    return nullptr;
}

CacheElement* find(const char* url) {
    CacheElement* site = nullptr;
    int temp_lock_val = pthread_mutex_lock(&lock);
    std::cout << "Remove Cache Lock Acquired " << temp_lock_val << std::endl;
    
    if(head != nullptr) {
        site = head;
        while (site != nullptr) {
            if(!strcmp(site->url, url)) {
                std::cout << "LRU Time Track Before : " << site->lru_time_track << std::endl;
                std::cout << "\nurl found\n";
                site->lru_time_track = time(nullptr);
                std::cout << "LRU Time Track After : " << site->lru_time_track << std::endl;
                break;
            }
            site = site->next;
        }       
    }
    else {
        std::cout << "\nurl not found\n";
    }
    
    temp_lock_val = pthread_mutex_unlock(&lock);
    std::cout << "Remove Cache Lock Unlocked " << temp_lock_val << std::endl;
    return site;
}

void remove_cache_element() {
    CacheElement *p, *q, *temp;
    int temp_lock_val = pthread_mutex_lock(&lock);
    std::cout << "Remove Cache Lock Acquired " << temp_lock_val << std::endl;
    
    if(head != nullptr) {
        for (q = head, p = head, temp = head; q->next != nullptr; q = q->next) {
            if((q->next->lru_time_track) < (temp->lru_time_track)) {
                temp = q->next;
                p = q;
            }
        }
        
        if(temp == head) { 
            head = head->next;
        } 
        else {
            p->next = temp->next;
        }
        
        cache_size = cache_size - (temp->len) - sizeof(CacheElement) - strlen(temp->url) - 1;
        delete temp;
    } 
    
    temp_lock_val = pthread_mutex_unlock(&lock);
    std::cout << "Remove Cache Lock Unlocked " << temp_lock_val << std::endl;
}

int add_cache_element(const char* data, int size, const char* url) {
    int temp_lock_val = pthread_mutex_lock(&lock);
    std::cout << "Add Cache Lock Acquired " << temp_lock_val << std::endl;
    
    int element_size = size + 1 + strlen(url) + sizeof(CacheElement);
    if(element_size > MAX_ELEMENT_SIZE) {
        temp_lock_val = pthread_mutex_unlock(&lock);
        std::cout << "Add Cache Lock Unlocked " << temp_lock_val << std::endl;
        return 0;
    }
    else {
        while(cache_size + element_size > MAX_SIZE) {
            remove_cache_element();
        }
        
        CacheElement* element = new CacheElement();
        element->data = strdup(data);
        element->url = strdup(url);
        element->lru_time_track = time(nullptr);
        element->next = head;
        element->len = size;
        head = element;
        cache_size += element_size;
        
        temp_lock_val = pthread_mutex_unlock(&lock);
        std::cout << "Add Cache Lock Unlocked " << temp_lock_val << std::endl;
        return 1;
    }
    return 0;
}

int main(int argc, char * argv[]) {
    int client_socketId, client_len;
    struct sockaddr_in server_addr, client_addr;

    sem_init(&seamaphore, 0, MAX_CLIENTS);
    pthread_mutex_init(&lock, nullptr);

    if(argc == 2) {
        port_number = atoi(argv[1]);
    }
    else {
        std::cerr << "Too few arguments\n";
        exit(1);
    }

    std::cout << "Setting Proxy Server Port : " << port_number << std::endl;

    proxy_socketId = socket(AF_INET, SOCK_STREAM, 0);
    if(proxy_socketId < 0) {
        std::cerr << "Failed to create socket.\n";
        exit(1);
    }

    int reuse = 1;
    if (setsockopt(proxy_socketId, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
        std::cerr << "setsockopt(SO_REUSEADDR) failed\n";
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port_number);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    if(bind(proxy_socketId, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        std::cerr << "Port is not free\n";
        exit(1);
    }
    std::cout << "Binding on port: " << port_number << std::endl;

    int listen_status = listen(proxy_socketId, MAX_CLIENTS);
    if(listen_status < 0) {
        std::cerr << "Error while Listening!\n";
        exit(1);
    }

    int i = 0;
    std::vector<int> Connected_socketId(MAX_CLIENTS);

    while(true) {
        memset(&client_addr, 0, sizeof(client_addr));
        client_len = sizeof(client_addr);

        client_socketId = accept(proxy_socketId, (struct sockaddr*)&client_addr, (socklen_t*)&client_len);
        if(client_socketId < 0) {
            std::cerr << "Error in Accepting connection!\n";
            exit(1);
        }
        else {
            Connected_socketId[i] = client_socketId;
        }

        struct sockaddr_in* client_pt = (struct sockaddr_in*)&client_addr;
        struct in_addr ip_addr = client_pt->sin_addr;
        char str[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &ip_addr, str, INET_ADDRSTRLEN);
        std::cout << "Client is connected with port number: " << ntohs(client_addr.sin_port) 
                  << " and ip address: " << str << std::endl;
        
        pthread_create(&tid[i], nullptr, thread_fn, &Connected_socketId[i]);
        i++;
    }
    
    close(proxy_socketId);
    return 0;
}

