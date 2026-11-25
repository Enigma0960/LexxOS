typedef struct {
} cmd_tbl_t;

#define CMD_RET_FAILURE 1
#define CMD_RET_SUCCESS 0
#define U_BOOT_CMD(name, maxargs, repeatable, fn, usage, help) \
	static void* command_reference __attribute__((unused)) = (void*) &fn
