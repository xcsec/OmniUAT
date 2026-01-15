#include "App.h"
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>

static int g_socket_fd = -1;
static struct sockaddr_in g_server_addr;
static pthread_t g_network_thread;
static bool g_network_running = false;

static void* network_receive_thread(void* arg) {
    char buffer[4096];
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    
    while (g_network_running) {
        ssize_t recv_len = recvfrom(g_socket_fd, buffer, sizeof(buffer), 0,
                                     (struct sockaddr*)&client_addr, &client_len);
        if (recv_len > 0) {
            
            
                   inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
        }
    }
    
    return NULL;
}

int ocall_network_init(const char* ip, uint16_t port) {
    if (ip == NULL) return -1;
    
    
    g_socket_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (g_socket_fd < 0) {
        perror("socket");
        return -1;
    }
    
    
    memset(&g_server_addr, 0, sizeof(g_server_addr));
    g_server_addr.sin_family = AF_INET;
    g_server_addr.sin_addr.s_addr = inet_addr(ip);
    g_server_addr.sin_port = htons(port);
    
    
    if (bind(g_socket_fd, (struct sockaddr*)&g_server_addr, 
             sizeof(g_server_addr)) < 0) {
        perror("bind");
        close(g_socket_fd);
        g_socket_fd = -1;
        return -1;
    }
    
    
    g_network_running = true;
    if (pthread_create(&g_network_thread, NULL, network_receive_thread, NULL) != 0) {
        g_network_running = false;
        close(g_socket_fd);
        g_socket_fd = -1;
        return -1;
    }

    return 0;
}

int ocall_network_send(uint32_t node_id, uint8_t* data, size_t len) {
    if (data == NULL || len == 0) return -1;
    
    if (g_socket_fd < 0) return -1;
    
    
    
    struct sockaddr_in dest_addr;
    memset(&dest_addr, 0, sizeof(dest_addr));
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    dest_addr.sin_port = htons(8080 + node_id);
    
    ssize_t sent = sendto(g_socket_fd, data, len, 0,
                          (struct sockaddr*)&dest_addr, sizeof(dest_addr));
    
    if (sent < 0) {
        perror("sendto");
        return -1;
    }
    
    return 0;
}

int ocall_network_receive(uint8_t* data, size_t* len) {
    if (data == NULL || len == NULL) return -1;
    
    if (g_socket_fd < 0) return -1;
    
    
    
    return -1; 
}

int ocall_network_close(void) {
    g_network_running = false;
    
    if (g_network_thread) {
        pthread_join(g_network_thread, NULL);
    }
    
    if (g_socket_fd >= 0) {
        close(g_socket_fd);
        g_socket_fd = -1;
    }
    
    return 0;
}
