void	IN_Init ( void *windowData );
void	IN_Shutdown (void);
void    IN_Restart (void);
void	IN_JoystickCommands (void);

void	IN_Move (usercmd_t *cmd);
void	IN_Frame (void);

void	Sys_SendKeyEvents (void);
void	Sys_QueEvent( int time, sysEventType_t type, int value, int value2, int ptrLength, void *ptr );
