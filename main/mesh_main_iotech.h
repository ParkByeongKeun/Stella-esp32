typedef struct {
	uint8_t cmd;
	bool on;
	uint8_t token_id;
	uint16_t token_value;
} mesh_light_ctl_t;
#define  MESH_TOKEN_ID       (0x0)
#define  MESH_TOKEN_VALUE    (0xbeef)
#define  MESH_CONTROL_CMD    (0x2)


