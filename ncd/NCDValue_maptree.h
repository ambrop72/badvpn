#define CAVL_PARAM_USE_COUNTS 0
#define CAVL_PARAM_NAME NCDValue__MapTree
#define CAVL_PARAM_ENTRY NCDMapElement
#define CAVL_PARAM_LINK NCDValue__maptree_link
#define CAVL_PARAM_KEY NCDValue__maptree_key
#define CAVL_PARAM_ARG int
#define CAVL_PARAM_NULL NULL
#define CAVL_PARAM_DEREF(arg, link) (link)
#define CAVL_PARAM_COMPARE_NODES(arg, node1, node2) NCDValue_Compare(&(node1).ptr->key, &(node2).ptr->key)
#define CAVL_PARAM_COMPARE_KEY_NODE(arg, key1, node2) NCDValue_Compare((key1), &(node2).ptr->key)
#define CAVL_PARAM_NODE_LINK tree_link
#define CAVL_PARAM_NODE_BALANCE tree_balance
#define CAVL_PARAM_NODE_PARENT tree_parent
