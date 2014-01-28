/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2010 Blender Foundation.
 * All rights reserved.
 *
 * Contributor(s): Andrei Izrantcev <andrei.izrantcev@chaosgroup.com>
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef CGR_ANIM_CACHE_H
#define CGR_ANIM_CACHE_H

#include "utils/murmur3.h"

#include <string>
#include <map>


template<typename T>
struct AnimationFrame {
    AnimationFrame() {
        hash  = 0;
        frame = 0;
        data  = NULL;
    }

    MHash    hash;
    int      frame;
	T*       data;
};


template<typename T>
class AnimationCache {
    typedef typename std::map< std::string, AnimationFrame<T> > ACache;

    ACache cache;

public:
    AnimationCache() {}

    ~AnimationCache() {
        for(typename ACache::iterator it = cache.begin(); it != cache.end(); ++it) {
            delete it->second.data;
        }
    }

	void update(const std::string &name, const MHash &hash, const int &frame, T *data) {
        if(cache.count(name))
            delete cache[name].data;
        cache[name].data  = data;
        cache[name].hash  = hash;
        cache[name].frame = frame;
    }

    MHash getHash(const std::string &name) {
        if(cache.count(name))
            return cache[name].hash;
        return 0;
    }

    int getFrame(const std::string &name) {
        if(cache.count(name))
            return cache[name].frame;
		return -1;
    }

	T* getData(const std::string &name) {
        if(cache.count(name))
            return cache[name].data;
        return NULL;
    }
};

#endif // CGR_ANIM_CACHE_H
