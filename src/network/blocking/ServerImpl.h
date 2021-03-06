#ifndef AFINA_NETWORK_BLOCKING_SERVER_H
#define AFINA_NETWORK_BLOCKING_SERVER_H

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <pthread.h>

#include <afina/network/Server.h>
#include <unordered_map>
#include <assert.h>

#include <afina/network/Connection.h>
#include <unordered_set>
#include <iostream>

namespace Afina {
namespace Network {
namespace Blocking {


class ConnectionImpl;
/**
 * # Network resource manager implementation
 * Server that is spawning a separate thread for each connection
 */
class ServerImpl : public Server {
public:
    ServerImpl(std::shared_ptr<Afina::Storage> ps);
    ~ServerImpl();

    // See Server.h
    void Start(uint32_t port, uint16_t workers) override;

    // See Server.h
    void Stop() override;

    // See Server.h
    void Join() override;

    //// Methods for miltithreading work woth connections
    ///////////////////////////////////////////////////////////////////////////////

    void AddConnection(Connection *p_connection);

    void EraseConnection(Connection *p_connection);

    int SizeConnections();

protected:
    /**
     * Method is running in the connection acceptor thread
     */
    void RunAcceptor();

private:

    static void *RunAcceptorProxy(void *p);


    // Atomic flag to notify threads when it is time to stop. Note that
    // flag must be atomic in order to safely publisj changes cross thread
    // bounds
    std::atomic<bool> running;

    // Thread that is accepting new connections
    pthread_t accept_thread;

    // Maximum number of client allowed to exists concurrently
    // on server, permits access only from inside of accept_thread.
    // Read-only
    uint16_t max_workers;

    // Port to listen for new connections, permits access only from
    // inside of accept_thread
    // Read-only
    uint32_t listen_port;

    // Mutex used to access connections list
    std::mutex connections_mutex;

    // Conditional variable used to notify waiters about empty
    // connections list
    std::condition_variable connections_cv;

    // Threads that are processing connection data, permits
    // access only from inside of accept_thread
    std::unordered_set<Connection*> set_connections;

    int server_socket;
};


} // namespace Blocking
} // namespace Network
} // namespace Afina

#endif // AFINA_NETWORK_BLOCKING_SERVER_H
