#define CAVL_PARAM_NAME FDMulticastTree
#define CAVL_PARAM_FEATURE_COUNTS 0
#define CAVL_PARAM_FEATURE_KEYS_ARE_INDICES 0
#define CAVL_PARAM_FEATURE_NOKEYS 0
#define CAVL_PARAM_TYPE_ENTRY FDMulticastTree_entry
#define CAVL_PARAM_TYPE_LINK FDMulticastTree_link
#define CAVL_PARAM_TYPE_KEY uint32_t
#define CAVL_PARAM_TYPE_ARG int
#define CAVL_PARAM_VALUE_NULL ((FDMulticastTree_link)NULL)
#define CAVL_PARAM_FUN_DEREF(arg, link) (link)
#define CAVL_PARAM_FUN_COMPARE_ENTRIES(arg, entry1, entry2) B_COMPARE((entry1).ptr->master.sig, (entry2).ptr->master.sig)
#define CAVL_PARAM_FUN_COMPARE_KEY_ENTRY(arg, key1, entry2) B_COMPARE((key1), (entry2).ptr->master.sig)
#define CAVL_PARAM_MEMBER_CHILD master.tree_child
#define CAVL_PARAM_MEMBER_BALANCE master.tree_balance
#define CAVL_PARAM_MEMBER_PARENT master.tree_parent
