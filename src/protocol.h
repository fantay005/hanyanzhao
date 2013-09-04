#ifndef __PROTOCOL_H__
#define __PROTOCOL_H__

void ProtocolDestroyMessage(const char *p);
const char *ProtoclCreateHeartBeat(int *size);
const char *ProtoclCreatLogin(int *size);
void ProtocolHandler(unsigned char *p);

#endif
