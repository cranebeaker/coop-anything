/* Log file next to Darkest.exe (ddcoop-fw.log), thread safe, one timestamped line per call. */
#ifndef DD_LOG_H
#define DD_LOG_H

void dd_log_open(const char *path);
void dd_log(const char *fmt, ...);
void dd_log_close(void);

#endif
