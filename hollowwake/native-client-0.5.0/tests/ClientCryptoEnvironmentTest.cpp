#include "../Native/ClientCryptoEnvironment.h"
#include <iostream>
#include <string>
#include <intrin.h>
int main(){
 int maximum[4]={},features[4]={};__cpuidex(maximum,0,0);if(maximum[0]>=7)__cpuidex(features,7,0);
 std::cout<<"actualCPUID7_SHA="<<((static_cast<unsigned>(features[1])>>29)&1)<<" actualCPUID7_AVX2="<<((static_cast<unsigned>(features[1])>>5)&1)<<"; hardware response unchanged by client configuration\n";
 const char* key="OPENSSL_ia32cap";auto prior=getenv(key);bool hadPrior=prior!=nullptr;std::string saved=prior?prior:"";
 const char* probe="HW_OWNED_CRYPTO_ENV_TEST";auto oldProbe=getenv(probe);bool hadProbe=oldProbe!=nullptr;std::string savedProbe=oldProbe?oldProbe:"";
 ClientCryptoEnvironment policy;bool missing=!policy.Bind(nullptr)&&!policy.Enable();
 bool configured=policy.Bind(&getenv);bool cleaned=_putenv_s(key,"")==0;bool before=policy.Lookup(key)==nullptr;
 bool active=policy.Enable();auto selected=policy.Lookup(key);
 bool overlay=selected&&!strcmp(selected,":~0x20000000")&&getenv(key)==nullptr;
 bool sentinel=_putenv_s(probe,"owned-value")==0;auto other=policy.Lookup(probe);bool delegated=other&&!strcmp(other,"owned-value");
 bool set=_putenv_s(key,"~0x1000000")==0;auto explicitValue=policy.Lookup(key);bool respected=explicitValue&&!strcmp(explicitValue,"~0x1000000");
 bool restored=_putenv_s(key,hadPrior?saved.c_str():"")==0&&_putenv_s(probe,hadProbe?savedProbe.c_str():"")==0;
 bool passed=missing&&configured&&cleaned&&before&&active&&overlay&&sentinel&&delegated&&set&&respected&&restored;
 std::cout<<(passed?"PASS":"FAIL")<<" real CRT getenv/putenv delegation; client-only OpenSSL default, explicit setting respected, host unchanged by overlay, owned environment restored. No hash, game or Xbox success inferred.\n";return passed?0:1;
}
