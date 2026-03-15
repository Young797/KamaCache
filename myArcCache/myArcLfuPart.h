#pragma once

#include "myArcCacheNode.h"
#include <unordered_map>
#include <map>
#include <mutex>
#include <list>

namespace KamaCache 
{

template<typename Key, typename Value>
class ArcLfuPart 
{
public:
    using NodeType = ArcNode<Key, Value>;
    using NodePtr = std::shared_ptr<NodeType>;
    using NodeMap = std::unordered_map<Key, NodePtr>;
    using FreqMap = std::map<size_t, std::list<NodePtr>>;

    explicit ArcLfuPart(size_t capacity, size_t transformThreshold)
        : capacity_(capacity)
        , ghostCapacity_(capacity)
        , transformThreshold_(transformThreshold)
        , minFreq_(0)
    {
        ghostCache_ = std::make_unique<GhostCache<Key>>(ghostCapacity_);
    }

    bool put(Key key, Value value) 
    {
        if (capacity_ == 0) 
            return false;

        std::lock_guard<std::mutex> lock(mutex_);
        auto it = mainCache_.find(key);
        if (it != mainCache_.end()) 
        {
            return updateExistingNode(it->second, value);
        }
        return addNewNode(key, value);
    }

    bool get(Key key, Value& value) 
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = mainCache_.find(key);
        if (it != mainCache_.end()) 
        {
            updateNodeFrequency(it->second);
            value = it->second->getValue();
            return true;
        }
        return false;
    }

    void increaseCapacity() { ++capacity_; }
    
    bool decreaseCapacity() 
    {
        if (capacity_ <= 0) return false;
        if (mainCache_.size() == capacity_) 
        {
            evictLeastFrequent();
        }
        --capacity_;
        return true;
    }

private:
    bool updateExistingNode(NodePtr node, const Value& value) 
    {
        node->setValue(value);
        updateNodeFrequency(node);
        return true;
    }

    bool addNewNode(const Key& key, const Value& value) 
    {
        if (mainCache_.size() >= capacity_) 
        {
            evictLeastFrequent();
        }

        NodePtr newNode = std::make_shared<NodeType>(key, value);
        mainCache_[key] = newNode;
        
        // 将新节点添加到频率为1的列表中
        if (freqMap_.find(1) == freqMap_.end()) 
        {
            freqMap_[1] = std::list<NodePtr>();
        }
        freqMap_[1].push_back(newNode);
        minFreq_ = 1;
        
        return true;
    }

    void updateNodeFrequency(NodePtr node) 
    {
        size_t oldFreq = node->getAccessCount();
        node->incrementAccessCount();
        size_t newFreq = node->getAccessCount();

        // 从旧频率列表中移除
        auto& oldList = freqMap_[oldFreq];
        oldList.remove(node);
        if (oldList.empty()) 
        {
            freqMap_.erase(oldFreq);
            if (oldFreq == minFreq_) 
            {
                minFreq_ = newFreq;
            }
        }

        // 添加到新频率列表
        if (freqMap_.find(newFreq) == freqMap_.end()) 
        {
            freqMap_[newFreq] = std::list<NodePtr>();
        }
        freqMap_[newFreq].push_back(node);
    }

    void evictLeastFrequent() 
    {
        if (freqMap_.empty()) 
            return;

        // 获取最小频率的列表
        auto& minFreqList = freqMap_[minFreq_];
        if (minFreqList.empty()) 
            return;

        // 移除最少使用的节点
        NodePtr leastNode = minFreqList.front();
        minFreqList.pop_front();

        // 如果该频率的列表为空，则删除该频率项
        if (minFreqList.empty()) 
        {
            freqMap_.erase(minFreq_);
            // 更新最小频率
            if (!freqMap_.empty()) 
            {
                minFreq_ = freqMap_.begin()->first;
            }
        }

        // 将节点移到幽灵缓存
        ghostCache_->addGhostKey(leastNode->getKey());
        
        // 从主缓存中移除
        mainCache_.erase(leastNode->getKey());
    }

public:
    bool containMain(const Key& key);
    bool containGhost(const Key& key);
    void removeGhostkey(const Key key);

private:
    size_t capacity_;
    size_t ghostCapacity_;
    size_t transformThreshold_;
    size_t minFreq_;
    std::mutex mutex_;

    NodeMap mainCache_;
    std::unique_ptr<GhostCache<Key>> ghostCache_;
    FreqMap freqMap_;
};

template<typename Key, typename Value>
bool ArcLfuPart<Key, Value>::containMain(const Key& key)
{
    return mainCache_.find(key) != mainCache_.end();
}
template<typename Key, typename Value>
bool ArcLfuPart<Key, Value>::containGhost(const Key& key)
{
    return ghostCache_->inCache(key);
}
template<typename Key, typename Value>
void ArcLfuPart<Key, Value>::removeGhostkey(const Key key)
{
    ghostCache_->removeKey(key);
}
} // namespace KamaCache