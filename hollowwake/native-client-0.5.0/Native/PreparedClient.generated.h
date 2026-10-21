#pragma once
#include "ClientImageStartup.h"
// Public source contains no game payload. Build.ps1 generates exact pins only
// from a verified private preparation before compiling the integrated runtime.
static constexpr bool kPreparedClientAvailable=false;
static constexpr SIZE_T kPreparedClientBytes=0;
static constexpr SIZE_T kPreparedOriginalImageBytes=0;
static const BYTE kPreparedClientSha[32]={0};
static const ClientImageProtection kPreparedClientProtections[]={{0,0,0}};
static constexpr unsigned kPreparedClientProtectionCount=0;
