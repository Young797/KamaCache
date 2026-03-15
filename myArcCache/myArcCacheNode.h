// added Ghost node class (maintained FIFO rule)
// ghost nodes only store key
#pragma once

#include <memory>
#include <unordered_map>

namespace KamaCache 
{

template<typename Key, typename Value>
class ArcNode 
{
private:
    Key key_;
    Value value_;
    size_t accessCount_;
    std::weak_ptr<ArcNode> prev_;
    std::shared_ptr<ArcNode> next_;

public:
    ArcNode() : accessCount_(1), next_(nullptr) {}
    
    ArcNode(Key key, Value value) 
        : key_(key)
        , value_(value)
        , accessCount_(1)
        , next_(nullptr) 
    {}

    // Getters
    Key getKey() const { return key_; }
    Value getValue() const { return value_; }
    size_t getAccessCount() const { return accessCount_; }
    
    // Setters
    void setValue(const Value& value) { value_ = value; }
    void incrementAccessCount() { ++accessCount_; }

    template<typename K, typename V> friend class ArcLruPart;
    template<typename K, typename V> friend class ArcLfuPart;
};

template<typename Key>
class GhostCache{
public:
    GhostCache(size_t capacity) : capacity_(capacity)
    {
        dummyHead_ = std::make_shared<GhostNode>();
        dummyTail_ = std::make_shared<GhostNode>();
        dummyHead_->next_ = dummyTail_;
        dummyTail_->prev_ = dummyHead_;
    }
    void addGhostKey(Key key)
    {
        if(isFull())    evictLastNode();
        auto newnode = std::make_shared<GhostNode>(key);
        newnode->next_ = dummyHead_->next_;
        newnode->prev_ = dummyHead_;
        dummyHead_->next_->prev_ = newnode;
        dummyHead_->next_ = newnode; 
        nodeMap_[key] = newnode;
    }
    bool inCache(const Key& key)
    {
        return nodeMap_.find(key) != nodeMap_.end();
    }
    void removeKey(Key key)
    {
        if(!inCache(key)) return;
        if(isEmpty())    return;
        auto todel = nodeMap_[key];
        auto prev = todel->prev_.lock();
        prev->next_ = todel->next_;
        todel->next_->prev_ = prev;
        todel->next_ = nullptr;
        nodeMap_.erase(key);
    }
private:
    void evictLastNode()
    {
        if(isEmpty())   return;
        auto todel = dummyTail_->prev_.lock();
        auto prev = todel->prev_.lock();
        prev->next_ = todel->next_;
        todel->next_->prev_ = prev;
        todel->next_ = nullptr;
        nodeMap_.erase(todel->getKey());
    }
    bool isEmpty()
    {
        return dummyHead_->next_ == dummyTail_;
    }
    bool isFull()
    {
        return nodeMap_.size() >= capacity_;
    }

public:
struct GhostNode{
    Key key_;
    std::weak_ptr<GhostNode> prev_;
    std::shared_ptr<GhostNode> next_;

    GhostNode(Key key) : key_(key), next_(nullptr) {} 
    GhostNode() : next_(nullptr) {} 
    Key getKey() const { return key_; }
};

private:
    size_t capacity_;
    std::unordered_map<Key, std::shared_ptr<GhostNode>> nodeMap_;
    std::shared_ptr<GhostNode> dummyHead_;
    std::shared_ptr<GhostNode> dummyTail_;
};
} // namespace KamaCache