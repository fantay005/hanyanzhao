#ifndef	 __SMS_CMD__
#define	 __SMS_CMD__

void SMSCmdSetUser(int index, const char *user);
typedef struct {
	char user[6][12];
} USERParam;

#endif
