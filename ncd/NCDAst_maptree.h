#define SAVL_PARAM_NAME NCDValue__MapTree
#define SAVL_PARAM_FEATURE_COUNTS 0
#define SAVL_PARAM_FEATURE_NOKEYS 0
#define SAVL_PARAM_TYPE_ENTRY struct NCDValue__map_element
#define SAVL_PARAM_TYPE_KEY NCDValue__maptree_key
#define SAVL_PARAM_TYPE_ARG int
#define SAVL_PARAM_FUN_COMPARE_ENTRIES(arg, entry1, entry2) NCDValue_Compare(&(entry1)->key, &(entry2)->key)
#define SAVL_PARAM_FUN_COMPARE_KEY_ENTRY(arg, key1, entry2) NCDValue_Compare((key1), &(entry2)->key)
#define SAVL_PARAM_MEMBER_NODE tree_node
