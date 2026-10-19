#include "NetworkAvailability.h"
#include <collection.h>
#include <roapi.h>
#include <cstring>
BOOL WINAPI HWIsNetworkAlive(DWORD* flags){
 if(!flags){SetLastError(ERROR_INVALID_PARAMETER);return FALSE;}*flags=0;
 // WinRT activation must be valid on game-created threads as well as the XAML
 // thread. Balance only this call's successful initialization reference.
 auto initialized=RoInitialize(RO_INIT_MULTITHREADED);if(FAILED(initialized)&&initialized!=RPC_E_CHANGED_MODE){SetLastError(HRESULT_FACILITY(initialized)==FACILITY_WIN32?HRESULT_CODE(initialized):ERROR_GEN_FAILURE);return FALSE;}
 struct Apartment {bool owned;~Apartment(){if(owned)RoUninitialize();}} apartment={SUCCEEDED(initialized)};
 try{
  using namespace Windows::Networking::Connectivity;DWORD result=0;bool unknown=false;
  for(auto profile:NetworkInformation::GetConnectionProfiles()){
   if(profile->GetNetworkConnectivityLevel()==NetworkConnectivityLevel::None)continue;
   auto adapter=profile->NetworkAdapter;if(!adapter){unknown=true;continue;}
   auto kind=adapter->IanaInterfaceType;
   // A live Ethernet/Wi-Fi connection establishes NETWORK_ALIVE_LAN. SENS
   // defines WAN specifically as RAS, not general Internet/cellular access.
   // Do not invent a RAS state from a WinRT InternetAccess enum or WWAN flag.
   if(kind==6||kind==71)result|=1;
   else if(kind!=24)unknown=true; // Pure software loopback is not an active LAN.
  }
  if(unknown){SetLastError(ERROR_NOT_SUPPORTED);return FALSE;}
  *flags=result;SetLastError(ERROR_SUCCESS);return result?TRUE:FALSE;
 }catch(Platform::Exception^ e){SetLastError(HRESULT_FACILITY(e->HResult)==FACILITY_WIN32?HRESULT_CODE(e->HResult):ERROR_GEN_FAILURE);return FALSE;}
}
void* ResolveNetworkAvailabilityImport(const char* dll,const char* name){return dll&&name&&!_stricmp(dll,"sensapi.dll")&&!strcmp(name,"IsNetworkAlive")?reinterpret_cast<void*>(&HWIsNetworkAlive):nullptr;}
extern "C" __declspec(dllexport) int __cdecl HWNetworkAvailabilityProbe(DWORD* flags,DWORD* error){if(!flags||!error)return E_POINTER;auto result=HWIsNetworkAlive(flags);*error=GetLastError();return result?1:0;}
