#ifndef LOGGER_C_H
#define LOGGER_C_H

#ifdef __cplusplus
extern "C" {
#endif

void utl_trace
    (
    const char* aFormat, 
    ...
    );

void utl_warning
    (
    const char* aFormat, 
    ...
    );
    
void utl_error
    (
    const char* aFormat, 
    ...
    );
    
#ifdef __cplusplus
}
#endif
    
#endif
