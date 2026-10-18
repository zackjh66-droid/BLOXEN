// System cursor identities backed by actual CoreWindow cursor selection.
struct StockCursor {UINT resource;};
static StockCursor stockCursors[]={{32512},{32513},{32514},{32515},{32516},{32642},{32643},{32644},{32645},{32646},{32648},{32649},{32651}};
using CursorSelect=BOOL(__cdecl*)(UINT,int);
static CursorSelect selectCursor=nullptr;static HCURSOR selectedCursor=nullptr;
extern "C" __declspec(dllexport) void __cdecl HWConfigureCursor(CursorSelect callback){std::lock_guard<std::recursive_mutex> l(mutex);selectCursor=callback;}
static HCURSOR WINAPI LoadStockCursor(HINSTANCE instance,LPCWSTR name){if(instance||(ULONG_PTR)name>65535||!name){SetLastError(ERROR_NOT_SUPPORTED);return nullptr;}
 for(auto& cursor:stockCursors)if(cursor.resource==(UINT)(ULONG_PTR)name)return reinterpret_cast<HCURSOR>(&cursor);
 SetLastError(ERROR_RESOURCE_NAME_NOT_FOUND);return nullptr;
}
static HCURSOR WINAPI SelectCursor(HCURSOR value){std::lock_guard<std::mutex> operation(cursorOperation);UINT resource=0;CursorSelect callback;HCURSOR prior;int visible;
 {std::lock_guard<std::recursive_mutex> l(mutex);if(value){bool valid=false;for(auto& cursor:stockCursors)if(value==reinterpret_cast<HCURSOR>(&cursor)){resource=cursor.resource;valid=true;break;}if(!valid){SetLastError(ERROR_INVALID_CURSOR_HANDLE);return nullptr;}}
 callback=selectCursor;prior=selectedCursor;visible=cursorBalance>=0;}
 if(!callback){SetLastError(ERROR_NOT_SUPPORTED);return nullptr;}if(!callback(resource,visible))return nullptr;
 {std::lock_guard<std::recursive_mutex> l(mutex);selectedCursor=value;}return prior;
}
