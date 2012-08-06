// Preprocessor inputs:
// GROWARRAY_NAME - prefix of of functions to define
// GROWARRAY_OBJECT_TYPE - type of structure where array and capacity sizes are
// GROWARRAY_ARRAY_MEMBER - array member
// GROWARRAY_CAPACITY_MEMBER - capacity member
// GROWARRAY_MAX_CAPACITY - max value of capacity member

#include <limits.h>
#include <string.h>
#include <stddef.h>

#include <misc/debug.h>
#include <misc/balloc.h>
#include <misc/merge.h>

#define GrowArrayObject GROWARRAY_OBJECT_TYPE
#define GrowArray_Init MERGE(GROWARRAY_NAME, _Init)
#define GrowArray_Free MERGE(GROWARRAY_NAME, _Free)
#define GrowArray_DoubleUp MERGE(GROWARRAY_NAME, _DoubleUp)

static int GrowArray_Init (GrowArrayObject *o, size_t capacity) WARN_UNUSED;
static void GrowArray_Free (GrowArrayObject *o);
static int GrowArray_DoubleUp (GrowArrayObject *o) WARN_UNUSED;

static int GrowArray_Init (GrowArrayObject *o, size_t capacity)
{
    ASSERT(capacity > 0)
    
    if (capacity > GROWARRAY_MAX_CAPACITY) {
        return 0;
    }
    
    if (!(o->GROWARRAY_ARRAY_MEMBER = BAllocArray(capacity, sizeof(o->GROWARRAY_ARRAY_MEMBER[0])))) {
        return 0;
    }
        
    o->GROWARRAY_CAPACITY_MEMBER = capacity;
    
    return 1;
}

static void GrowArray_Free (GrowArrayObject *o)
{
    BFree(o->GROWARRAY_ARRAY_MEMBER);
}

static int GrowArray_DoubleUp (GrowArrayObject *o)
{
    ASSERT(o->GROWARRAY_CAPACITY_MEMBER > 0)
    
    if (o->GROWARRAY_CAPACITY_MEMBER > SIZE_MAX / 2 || o->GROWARRAY_CAPACITY_MEMBER > GROWARRAY_MAX_CAPACITY / 2) {
        return 0;
    }
    
    size_t newcap = 2 * o->GROWARRAY_CAPACITY_MEMBER;
    
    void *newarr = BAllocArray(newcap, sizeof(o->GROWARRAY_ARRAY_MEMBER[0]));
    if (!newarr) {
        return 0;
    }
    
    memcpy(newarr, o->GROWARRAY_ARRAY_MEMBER, o->GROWARRAY_CAPACITY_MEMBER * sizeof(o->GROWARRAY_ARRAY_MEMBER[0]));
    
    BFree(o->GROWARRAY_ARRAY_MEMBER);
    
    o->GROWARRAY_ARRAY_MEMBER = newarr;
    o->GROWARRAY_CAPACITY_MEMBER = newcap;
    
    return 1;
}

#undef GROWARRAY_NAME
#undef GROWARRAY_OBJECT_TYPE
#undef GROWARRAY_ARRAY_MEMBER
#undef GROWARRAY_CAPACITY_MEMBER
#undef GROWARRAY_MAX_CAPACITY

#undef GrowArrayObject
#undef GrowArray_Init
#undef GrowArray_Free
#undef GrowArray_DoubleUp
