#include "MapBasedGlobalLockImpl.h"

#include <mutex>

namespace Afina {
namespace Backend {

bool MapBasedGlobalLockImpl::In(const std::string &key) {
    if (container.find(key) != container.end())
        return false;
    return true;
}

bool MapBasedGlobalLockImpl::Put(const std::string &key, const std::string &value) {
    std::unique_lock<std::recursive_mutex>  storage_lock(mutex);

    if (container.size() == _max_size)
        Delete(Oldest());

    if (!In(key)) {
        container[key] = block(_stamp, value);
        _stamp++;

        storage_lock.unlock();
        return true;
    }

    bool result = PutIfAbsent(key, value);

    storage_lock.unlock();

    return result;

}

bool MapBasedGlobalLockImpl::PutIfAbsent(const std::string &key, const std::string &value) {
    std::unique_lock<std::recursive_mutex> storage_lock(mutex);

    bool result= container.insert(std::pair<std::string, block>(key, block(_stamp, value))).second;
    _stamp++;

    storage_lock.unlock();
    return result;
}

bool MapBasedGlobalLockImpl::Set(const std::string &key, const std::string &value) {
    std::unique_lock<std::recursive_mutex> storage_lock(mutex);

    if (!In(key)) {
        storage_lock.unlock();
        return false;
    }

    container[key] = block(_stamp, value);
    _stamp++;

    storage_lock.unlock();
    return true;
}

bool MapBasedGlobalLockImpl::Delete(const std::string &key) {
    std::unique_lock<std::recursive_mutex> storage_lock(mutex);

    container.erase(container.find(key));
    storage_lock.unlock();
    if (In(key)) {
        return false;
    }
    return true;
}

bool MapBasedGlobalLockImpl::Get(const std::string &key, std::string &value) {
    std::unique_lock<std::recursive_mutex>  storage_lock(mutex);

    if (In(key)) {
        storage_lock.unlock();
        return false;
    }

    value = container.at(key).second;
    storage_lock.unlock();
    return true;
}

std::string MapBasedGlobalLockImpl::Oldest() {
    size_t min = _stamp;
    std::string result_key;
    std::unique_lock<std::recursive_mutex> storage_lock(mutex);

    for (auto elem : container) {
        if (elem.second.first < min) {
            min = elem.second.first;
            result_key = elem.first;
        }
    }
    storage_lock.unlock();

    return result_key;
}

} // namespace Backend
} // namespace Afina
