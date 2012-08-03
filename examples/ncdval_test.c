#include <stdio.h>

#include <ncd/NCDVal.h>
#include <base/BLog.h>
#include <misc/debug.h>

#define FORCE(cmd) if (!(cmd)) { fprintf(stderr, "failed\n"); exit(1); }

static void print_indent (int indent)
{
    for (int i = 0; i < indent; i++) {
        printf("  ");
    }
}

static void print_value (NCDValRef val, unsigned int indent)
{
    switch (NCDVal_Type(val)) {
        case NCDVAL_STRING: {
            print_indent(indent);
            printf("string(%zu) %s\n", NCDVal_StringLength(val), NCDVal_StringValue(val));
        } break;
        
        case NCDVAL_LIST: {
            size_t count = NCDVal_ListCount(val);
            
            print_indent(indent);
            printf("list(%zu)\n", NCDVal_ListCount(val));
            
            for (size_t i = 0; i < count; i++) {
                NCDValRef elem_val = NCDVal_ListGet(val, i);
                print_value(elem_val, indent + 1);
            }
        } break;
        
        case NCDVAL_MAP: {
            print_indent(indent);
            printf("map(%zu)\n", NCDVal_MapCount(val));
            
            for (NCDValMapElem e = NCDVal_MapOrderedFirst(val); !NCDVal_MapElemInvalid(e); e = NCDVal_MapOrderedNext(val, e)) {
                NCDValRef ekey = NCDVal_MapElemKey(val, e);
                NCDValRef eval = NCDVal_MapElemVal(val, e);
                
                print_indent(indent + 1);
                printf("key=\n");
                print_value(ekey, indent + 2);
                
                print_indent(indent + 1);
                printf("val=\n");
                print_value(eval, indent + 2);
            }
        } break;
    }
}

int main ()
{
    BLog_InitStdout();
    
    // Some basic usage of values.
    
    NCDValMem mem;
    NCDValMem_Init(&mem);
    
    NCDValRef s1 = NCDVal_NewString(&mem, "Hello World");
    FORCE( !NCDVal_IsInvalid(s1) )
    
    NCDValRef s2 = NCDVal_NewString(&mem, "This is reeeeeeeeeeeeeallllllllyyyyy fun!");
    FORCE( !NCDVal_IsInvalid(s2) )
    
    printf("%s. %s\n", NCDVal_StringValue(s1), NCDVal_StringValue(s2));
    
    NCDValRef l1 = NCDVal_NewList(&mem, 10);
    FORCE( !NCDVal_IsInvalid(l1) )
    
    NCDVal_ListAppend(l1, s1);
    NCDVal_ListAppend(l1, s2);
    
    print_value(s1, 0);
    print_value(s2, 0);
    print_value(l1, 0);
    
    NCDValRef k1 = NCDVal_NewString(&mem, "K1");
    FORCE( !NCDVal_IsInvalid(k1) )
    NCDValRef v1 = NCDVal_NewString(&mem, "V1");
    FORCE( !NCDVal_IsInvalid(v1) )
    
    NCDValRef k2 = NCDVal_NewString(&mem, "K2");
    FORCE( !NCDVal_IsInvalid(k2) )
    NCDValRef v2 = NCDVal_NewString(&mem, "V2");
    FORCE( !NCDVal_IsInvalid(v2) )
    
    NCDValRef m1 = NCDVal_NewMap(&mem, 2);
    FORCE( !NCDVal_IsInvalid(m1) )
    
    FORCE( NCDVal_MapInsert(m1, k1, v1) )
    FORCE( NCDVal_MapInsert(m1, k2, v2) )
    
    print_value(m1, 0);
    
    NCDValMem_Free(&mem);
    
    // Try to make copies of a string within the same memory object.
    // This is an evil test because we cannot simply copy a string using e.g.
    // NCDVal_NewStringBin() - it requires that the buffer passed
    // be outside the memory object of the new string.
    // We use NCDVal_NewCopy(), which takes care of this by creating
    // an uninitialized string using NCDVal_NewStringUninitialized() and
    // then copyng the data.
    
    NCDValMem_Init(&mem);
    
    NCDValRef s[100];
    
    s[0] = NCDVal_NewString(&mem, "Eeeeeeeeeeeevil.");
    FORCE( !NCDVal_IsInvalid(s[0]) )
    
    for (int i = 1; i < 100; i++) {
        s[i] = NCDVal_NewCopy(&mem, s[i - 1]);
        FORCE( !NCDVal_IsInvalid(s[i]) )
        ASSERT( !strcmp(NCDVal_StringValue(s[i - 1]), "Eeeeeeeeeeeevil.") )
        ASSERT( !strcmp(NCDVal_StringValue(s[i]), "Eeeeeeeeeeeevil.") )
    }
    
    for (int i = 0; i < 100; i++) {
        ASSERT( !strcmp(NCDVal_StringValue(s[i]), "Eeeeeeeeeeeevil.") )
    }
    
    NCDValMem_Free(&mem);
    
    return 0;
}
