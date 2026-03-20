#pragma once

namespace CurrentThread{
    extern __thread int t_cachedTid;

    void cacheTie();

    inline int tid(){
        if (__builtin_expect(t_cachedTid ==0,0)){
            cacheTie();
        }
        return t_cachedTid;
    }
}