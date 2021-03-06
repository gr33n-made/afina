#include "Worker.h"

#include <iostream>

#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <netdb.h>
#include <protocol/Parser.h>

#include <afina/execute/Command.h>
#include <cstring>

#include "Utils.h"

#define MAXEVENTS 64
#define MAXSIZERECV 1024

namespace Afina {
namespace Network {
namespace NonBlocking {

// See Worker.h
Worker::Worker(std::shared_ptr<Afina::Storage> ps) {
    pStorage = ps;
    parser = Afina::Network::Blocking::ParseRunner(pStorage);
}

// See Worker.h
Worker::~Worker() {}

// See Worker.h
void Worker::Start(int server_socket) {
    std::cout << "network debug: " << __PRETTY_FUNCTION__ << std::endl;

    this->server_socket = server_socket;
    running.store(true);
    if (pthread_create(&thread, NULL, OnRunProxy, this) != 0) { //<0
        throw std::runtime_error("Could not create worker thread");
    }
}

// See Worker.h
void Worker::Stop() {
    std::cout << "network debug: " << __PRETTY_FUNCTION__ << std::endl;
    running.store(false);
}

// See Worker.h
void Worker::Join() {
    std::cout << "network debug: " << __PRETTY_FUNCTION__ << std::endl;
    pthread_join(thread, 0);
}

void* Worker::OnRunProxy(void *p) {
    Worker *worker = reinterpret_cast<Worker *>(p);
    try {
        worker->OnRun();
    } catch (std::runtime_error &ex) {
        std::cerr << "Server fails: " << ex.what() << std::endl;
    }
    return 0;
}

// See Worker.h
void Worker::OnRun() {
    std::cout << "network debug: " << __PRETTY_FUNCTION__ << std::endl;

    int info;

    struct epoll_event  event;
    struct epoll_event* events;

    epoll_fd = epoll_create(0);
    if (epoll_fd < 0) {
        perror("Epoll creation error");
        throw std::runtime_error("Could not create epoll");
    }

    //event.data.fd = server_socket;
    event.events = EPOLLIN | EPOLLET;
    //info = epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_socket, &event);
    //if (info != 0)
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_socket, &event) != 0){
        perror("epoll_ctl");
        throw std::runtime_error("Epoll control error!");
    }

    // Create buffer for events
    events = (epoll_event*)calloc(MAXEVENTS, sizeof(event));

    std::string out;
    while (running.load()) {
        int desq_num;

        desq_num = epoll_wait(epoll_fd, events, MAXEVENTS, 1000);
        for (int i = 0; i < desq_num; i++) {

            /*if ((events[i].events == EPOLLERR) and !(events[i].events == EPOLLHUP) or (!events[i].events == EPOLLIN)) {
                fprintf(stderr, "Error with file descriptor\n");
                close(events[i].data.fd);
                std::cerr << "блять!" << std::endl;
                continue;
            }
            else */
            if (server_socket == events[i].data.fd) {
                while (true) {
                    struct sockaddr in_addr;
                    socklen_t in_len;
                    int in_fd;
                    char hbuf[NI_MAXHOST], sbuf[NI_MAXSERV];

                    //in_len = sizeof(in_addr);

                    in_fd = accept(server_socket, &in_addr, NULL);
                    if (in_fd == -1) {
                        if (!(errno == EAGAIN) && !(errno == EWOULDBLOCK)) {
                            perror("Acception");
                        }
                        break;
                    }

                    info = getnameinfo(&in_addr, sizeof(in_addr), hbuf, sizeof(hbuf), sbuf, sizeof(sbuf),
                                       NI_NUMERICHOST | NI_NUMERICSERV);
                    if (info !=0) {
                        printf("Connection on descriptor: %d (host=%s, port=%s)\n", in_fd, hbuf, sbuf);
                    }

                    make_socket_non_blocking(in_fd);

                    event.data.fd = in_fd;
                    event.events = EPOLLIN | EPOLLET; //| EPOLLEXCLUSIVE;
                    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, in_fd, &event) < 0) {
                        perror("epoll_ctl");
                        throw std::runtime_error("Epoll control error!");
                    }
                }
            } else {
                bool done = false, receive = true;

                while (receive) {
                    ssize_t count;
                    char buf[MAXSIZERECV];

                    count = recv(events[i].data.fd, buf, MAXSIZERECV, 0);
                    buf[count] = '\0';

                    parser.Load(buf, count);
                    while (!parser.IsEmpty()) {
                        try {
                            out = parser.Run();
                        } catch (std::exception &ex) {
                            parser.Reset();

                            out = std::string("SERVER_ERROR ") + ex.what() + "\r\n";
                            send(events[i].data.fd, out.c_str(), out.length(), 0);

                            done = true, receive = false;
                            break;
                        }

                        if (out == "")
                            continue;

                        if (send(events[i].data.fd, out.c_str(), out.length(), 0) < out.length()) {
                            done = true, receive = false;
                            break;
                        }

                        if (!running.load()) {
                            done = true, receive = false;
                            break;
                        }
                    }
                }

                if (done) {
                    printf("Close connection!\n");
                    close(events[i].data.fd);
                }
            }
        }
    }
}

} // namespace NonBlocking
} // namespace Network
} // namespace Afina
