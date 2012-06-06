#define CAVL_PARAM_USE_COUNTS 1
#define CAVL_PARAM_NAME IndexedList__Tree
#define CAVL_PARAM_ENTRY IndexedListNode
#define CAVL_PARAM_LINK IndexedList__tree_link
#define CAVL_PARAM_KEY IndexedList__tree_key
#define CAVL_PARAM_ARG IndexedList__tree_arg
#define CAVL_PARAM_COUNT uint64_t
#define CAVL_PARAM_COUNT_MAX UINT64_MAX
#define CAVL_PARAM_NULL NULL
#define CAVL_PARAM_DEREF(arg, link) (link)
#define CAVL_PARAM_COMPARE_NODES(arg, node1, node2) _IndexedList_comparator((arg), &(node1).ptr->key, &(node2).ptr->key)
#define CAVL_PARAM_COMPARE_KEY_NODE(arg, key1, node2) _IndexedList_comparator((arg), (key1), &(node2).ptr->key)
#define CAVL_PARAM_NODE_LINK tree_link
#define CAVL_PARAM_NODE_BALANCE tree_balance
#define CAVL_PARAM_NODE_PARENT tree_parent
#define CAVL_PARAM_NODE_COUNT tree_count
