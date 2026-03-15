#pragma once

#include "../KICachePolicy.h"
#include "myArcLruPart.h"
#include "myArcLfuPart.h"
#include <memory>

namespace KamaCache 
{

template<typename Key, typename Value>
class myArcCache : public KICachePolicy<Key, Value> 
{
public:
    explicit myArcCache(size_t capacity = 10, size_t transformThreshold = 2)
        : capacity_(capacity)
        , transformThreshold_(transformThreshold)
        , lruPart_(std::make_unique<ArcLruPart<Key, Value>>(capacity/2, transformThreshold))
        , lfuPart_(std::make_unique<ArcLfuPart<Key, Value>>(capacity-capacity/2, transformThreshold))
    {}

    ~myArcCache() override = default;

    enum class CacheLocation
    {
        NonExist = 0,
        LruPart = 1,
        LfuPart = 2
    };

    void put(Key key, Value value) override 
    {
        CacheLocation inmain = inMainCache(key);
        if (inmain == CacheLocation::LfuPart)
        {
            lfuPart_->put(key, value);
        }
        else if (inmain == CacheLocation::LruPart)
        {
            bool shouldTransform = false;
            lruPart_->put(key, value, shouldTransform);
            if(shouldTransform)
            {
                lruPart_->removeMainkey(key);
                lfuPart_->put(key, value);
            }
        } 
        else if (inmain == CacheLocation::NonExist)
        {
            CacheLocation inghost = inGhostCache(key);
            if (inghost == CacheLocation::LfuPart)
            {
                lfuPart_->increaseCapacity();
                lruPart_->decreaseCapacity();
                lfuPart_->removeGhostkey(key);
                lfuPart_->put(key, value);
            }
            else if (inghost == CacheLocation::LruPart)
            {
                lruPart_->increaseCapacity();
                lfuPart_->decreaseCapacity();
                lruPart_->removeGhostkey(key);
                lruPart_->put(key, value);
            }
            else if (inghost == CacheLocation::NonExist)
            {
                lruPart_->put(key, value);
            }
        }
    }

    bool get(Key key, Value& value) override 
    {
        CacheLocation inmain = inMainCache(key);
        if (inmain == CacheLocation::LfuPart)
        {
            lfuPart_->get(key, value);
            return true;
        }
        else if (inmain == CacheLocation::LruPart)
        {
            bool shouldTransform = false;
            lruPart_->get(key, value, shouldTransform);
            if(shouldTransform)
            {
                lruPart_->removeMainkey(key);
                lfuPart_->put(key, value);
            }
            return true;
        } 
        else if (inmain == CacheLocation::NonExist)
        {
            CacheLocation inghost = inGhostCache(key);
            if (inghost == CacheLocation::LfuPart)
            {
                lfuPart_->removeGhostkey(key); // remove to avoid multiple adjusting
                if (lruPart_->decreaseCapacity())
                {
                    lfuPart_->increaseCapacity();
                }
            }
            else if (inghost == CacheLocation::LruPart)
            {
                lruPart_->removeGhostkey(key);
                if (lfuPart_->decreaseCapacity())
                {
                    lruPart_->increaseCapacity();
                }
            }
            return false;
        }
        return false;
    }

    Value get(Key key) override 
    {
        Value value{};
        get(key, value);
        return value;
    }

private:
    CacheLocation inMainCache(const Key &key)   // check LFU first
    {
        if(lfuPart_->containMain(key))
        {
            return CacheLocation::LfuPart;
        }
        if(lruPart_->containMain(key))
        {
            return CacheLocation::LruPart;
        }
        return CacheLocation::NonExist;
    }
    CacheLocation inGhostCache(const Key &key)
    {
        // ... 
        if(lfuPart_->containGhost(key))
        {
            return CacheLocation::LfuPart;
        }
        if(lruPart_->containGhost(key))
        {
            return CacheLocation::LruPart;
        }
        return CacheLocation::NonExist;
    }

private:
    size_t capacity_;
    size_t transformThreshold_;
    std::unique_ptr<ArcLruPart<Key, Value>> lruPart_;
    std::unique_ptr<ArcLfuPart<Key, Value>> lfuPart_;
};

} // namespace KamaCache