#ifndef WIN32SERVICE_H
#define WIN32SERVICE_H

/* Hook for win32service.cpp to exit properly with the service specific error code */
void SetServiceStopped(int status);

/* Marks the service as running, not called until the config is parsed */
void SetServiceRunning();

#endif
