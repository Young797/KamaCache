#pragma once

#include "myArcCacheNode.h"
#include <unordered_map>
#include <mutex>

namespace KamaCache 
{

template<typename Key, typename Value>
class ArcLruPart 
{
public:
    using NodeType = ArcNode<Key, Value>;
    using NodePtr = std::shared_ptr<NodeType>;
    using NodeMap = std::unordered_map<Key, NodePtr>;

    explicit ArcLruPart(size_t capacity, size_t transformThreshold)
        : capacity_(capacity)
        , ghostCapacity_(capacity)
        , transformThreshold_(transformThreshold)
    {
        initializeLists();
    }

    bool put(Key key, Value value)
    {
        bool tmp{};
        return put(key, value, tmp);
    }
    bool put(Key key, Value value, bool& shouldTransform)
    {
        if (capacity_ == 0) return false;
        
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = mainCache_.find(key);
        if (it != mainCache_.end()) 
        {
            it->second->setValue(value);
            shouldTransform = updateNodeAccess(it->second);
            return true;
        }
        return addNewNode(key, value);
    }

    bool get(Key key, Value& value, bool& shouldTransform) 
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = mainCache_.find(key);
        if (it != mainCache_.end()) 
        {
            shouldTransform = updateNodeAccess(it->second);
            value = it->second->getValue();
            return true;
        }
        return false;
    }


    void increaseCapacity() { ++capacity_; }
    
    bool decreaseCapacity() 
    {
        if (capacity_ <= 0) return false;
        if (mainCache_.size() == capacity_) {
            evictLeastRecent();
        }
        --capacity_;
        return true;
    }

private:
    void initializeLists() 
    {
        mainHead_ = std::make_shared<NodeType>();
        mainTail_ = std::make_shared<NodeType>();
        mainHead_->next_ = mainTail_;
        mainTail_->prev_ = mainHead_;

        ghostCache_ = std::make_unique<GhostCache<Key>>(ghostCapacity_);
    }

    bool addNewNode(const Key& key, const Value& value) 
    {
        if (mainCache_.size() >= capacity_) 
        {   
            evictLeastRecent(); // 驱逐最近最少访问
        }

        NodePtr newNode = std::make_shared<NodeType>(key, value);
        mainCache_[key] = newNode;
        addToFront(newNode);
        return true;
    }

    bool updateNodeAccess(NodePtr node) 
    {
        moveToFront(node);
        node->incrementAccessCount();
        return node->getAccessCount() >= transformThreshold_;
    }

    void moveToFront(NodePtr node) 
    {
        // 先从当前位置移除
        if (!node->prev_.expired() && node->next_) {
            auto prev = node->prev_.lock();
            prev->next_ = node->next_;
            node->next_->prev_ = node->prev_;
            node->next_ = nullptr; // 清空指针，防止悬垂引用
        }
        
        // 添加到头部
        addToFront(node);
    }

    void addToFront(NodePtr node) 
    {
        node->next_ = mainHead_->next_;
        node->prev_ = mainHead_;
        mainHead_->next_->prev_ = node;
        mainHead_->next_ = node;
    }

    void evictLeastRecent() 
    {
        NodePtr leastRecent = mainTail_->prev_.lock();
        if (!leastRecent || leastRecent == mainHead_) 
            return;

        // 从主链表中移除
        removeFromMain(leastRecent);

        // 添加到幽灵缓存
        ghostCache_->addGhostKey(leastRecent->getKey());

        // 从主缓存映射中移除
        mainCache_.erase(leastRecent->getKey());
    }

    void removeFromMain(NodePtr node) 
    {
        if (!node->prev_.expired() && node->next_) {
            auto prev = node->prev_.lock();
            prev->next_ = node->next_;
            node->next_->prev_ = node->prev_;
            node->next_ = nullptr; // 清空指针，防止悬垂引用
        }
    }
public:
    bool containMain(const Key& key);
    bool containGhost(const Key& key);
    void removeMainkey(const Key key);
    void removeGhostkey(const Key key);
    
private:
    size_t capacity_;
    size_t ghostCapacity_;
    size_t transformThreshold_; // 转换门槛值
    std::mutex mutex_;

    NodeMap mainCache_; // key -> ArcNode
    std::unique_ptr<GhostCache<Key>>  ghostCache_;
    
    // 主链表
    NodePtr mainHead_;
    NodePtr mainTail_;
};

template<typename Key, typename Value>
bool ArcLruPart<Key, Value>::containMain(const Key& key)
{
    return mainCache_.find(key) != mainCache_.end();
}
template<typename Key, typename Value>
bool ArcLruPart<Key, Value>::containGhost(const Key& key)
{
    return ghostCache_->inCache(key);
}
template<typename Key, typename Value>
void ArcLruPart<Key, Value>::removeMainkey(const Key key)
{
    auto tmp = mainCache_.find(key);
    if(tmp == mainCache_.end()) return;
    NodePtr node = tmp->second;
    removeFromMain(node);
    mainCache_.erase(node->getKey());
}
template<typename Key, typename Value>
void ArcLruPart<Key, Value>::removeGhostkey(const Key key)
{
    ghostCache_->removeKey(key);
}
} // namespace KamaCache