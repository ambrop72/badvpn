#define CAVL_PARAM_USE_COUNTS 0
#define CAVL_PARAM_NAME NCDVal__MapTree
#define CAVL_PARAM_ENTRY NCDVal__maptree_entry
#define CAVL_PARAM_LINK NCDVal__idx
#define CAVL_PARAM_KEY NCDValRef
#define CAVL_PARAM_ARG NCDVal__maptree_arg
#define CAVL_PARAM_NULL ((NCDVal__idx)-1)
#define CAVL_PARAM_DEREF(arg, link) ((struct NCDVal__mapelem *)NCDValMem__BufAt((arg), (link)))
#define CAVL_PARAM_COMPARE_NODES(arg, node1, node2) NCDVal_Compare(NCDVal__Ref((arg), (node1).ptr->key_idx), NCDVal__Ref((arg), (node2).ptr->key_idx))
#define CAVL_PARAM_COMPARE_KEY_NODE(arg, key1, node2) NCDVal_Compare((key1), NCDVal__Ref((arg), (node2).ptr->key_idx))
#define CAVL_PARAM_NODE_LINK tree_link
#define CAVL_PARAM_NODE_BALANCE tree_balance
#define CAVL_PARAM_NODE_PARENT tree_parent
