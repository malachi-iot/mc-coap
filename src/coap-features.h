//
// Created by malachi on 2/23/18.
//

#pragma once


// TODO: Also platform.h interacts with feature flags, need to clean all
// that up and consolidate

// output informational/instrumentation oriented data
#define FEATURE_MCCOAP_PROFILING

// output errors when boundary checks fail
#define FEATURE_MCCOAP_BOUNDS_CHECK

// If defined, use std::clog and std::cerr for outputs, otherwise
// use printf
#define FEATURE_MCCOAP_CLOG

// If defined, IncomingContext itself is manager of message token
// memory.  It also disables token pool within ContextDispatcherHandler
// NOTE: Has a number of problems, not ready yet
//#define FEATURE_MCCOAP_INLINE_TOKEN
